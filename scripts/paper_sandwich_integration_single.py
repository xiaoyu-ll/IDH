"""Single-run Sandwich variants for HB scaling and large-data figures.

The six small full-data conditions reuse the separately source-hashed v2
ablation ledger.  This runner measures only conditions missing from Fig. 7(b)
and Fig. 9.  Each dataset/backend/condition is launched exactly once; scale
runs retain the historical 60 s deadline and large runs retain 180 s.
"""
import hashlib
import json
import os
import platform
import resource
import signal
import subprocess
import threading
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "paper_sandwich_integration_single_results"
BUILD = ROOT / "paper_sandwich_integration_build"
SCALE = [(f, ROOT / "paper_results_final/inputs" / f"HB_{f}.txt", 1494)
         for f in [0.1, 0.25, 0.5, 0.75, 1.0]]
LARGE = {
    "TC": (ROOT / "dataset_large/trivago.txt", 172738),
    "SA": (ROOT / "dataset_large/stackoverflow-answers.txt", 15211989),
    "TM": (ROOT / "dataset_large/threads-math-sx.txt", 176445),
    "DBLP": (ROOT / "dataset_large/coauth-dblp.txt", 1930378),
}


def digest(path):
    h = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def atomic_json(path, value):
    path = Path(path)
    tmp = path.with_suffix(path.suffix + ".tmp")
    tmp.write_text(json.dumps(value, indent=2) + "\n")
    tmp.replace(path)


def wait4_timeout(process, timeout):
    expired = threading.Event()
    timer = threading.Timer(timeout, expired.set)
    timer.start()
    try:
        while True:
            pid, status, usage = os.wait4(process.pid, os.WNOHANG)
            if pid:
                return status, usage
            if expired.is_set():
                raise TimeoutError
            time.sleep(.02)
    finally:
        timer.cancel()


def launch(key, command, timeout):
    out = OUT / "runs" / f"{key}.out"
    err = OUT / "runs" / f"{key}.err"
    start = time.perf_counter()
    with out.open("wb") as stdout, err.open("wb") as stderr:
        process = subprocess.Popen(list(map(str, command)), stdout=stdout, stderr=stderr,
                                   start_new_session=True)
        try:
            status, usage = wait4_timeout(process, timeout)
            code = os.waitstatus_to_exitcode(status)
            state = "ok" if code == 0 else "error"
        except TimeoutError:
            os.killpg(process.pid, signal.SIGKILL)
            _, status, usage = os.wait4(process.pid, 0)
            code, state = -9, "timeout"
    row = {
        "key": key,
        "command": list(map(str, command)),
        "status": state,
        "wall_s": time.perf_counter() - start,
        "peak_rss_mib": usage.ru_maxrss / (1024 ** 2 if platform.system() == "Darwin" else 1024),
        "timeout_s": timeout,
        "returncode": code,
        "output_sha256": digest(out) if state == "ok" else "",
    }
    if state == "error":
        raise RuntimeError(err.read_text())
    if state == "ok":
        meta = json.loads(err.read_text().strip().splitlines()[-1])
        row.update(meta)
    return row


def reference_hashes():
    old = json.loads((ROOT / "paper_results_final/measurements.json").read_text())
    scale = {}
    for fraction, _, _ in SCALE:
        candidates = [r for r in old if r.get("kind") == "scale" and
                      r.get("fraction") == fraction and r["status"] == "ok"]
        assert candidates and len({r["output_sha256"] for r in candidates}) == 1
        scale[fraction] = candidates[0]["output_sha256"]
    large = {}
    for result_dir in ["paper_large_results", "paper_public_results"]:
        rows = json.loads((ROOT / result_dir / "measurements.json").read_text())
        for dataset in LARGE:
            candidates = [r for r in rows if r.get("dataset") == dataset and
                          r.get("method") in {"hgd_flow", "hgd_reverse"} and
                          r["status"] == "ok"]
            if candidates:
                assert len({r["output_sha256"] for r in candidates}) == 1
                large[dataset] = candidates[0]["output_sha256"]
    assert set(large) == set(LARGE)
    return scale, large


def main():
    OUT.mkdir(exist_ok=True)
    (OUT / "runs").mkdir(exist_ok=True)
    BUILD.mkdir(exist_ok=True)
    sources = [
        "verified/hgd_sandwich_cli.cpp",
        "verified/hgd_sandwich.hpp",
        "verified/hgd_optimized.hpp",
        "verified/hgd_exact.cpp",
        "scripts/paper_sandwich_integration_single.py",
    ]
    hashes = {name: digest(ROOT / name) for name in sources}
    stamp = OUT / "source_hashes.json"
    if stamp.exists():
        assert json.loads(stamp.read_text()) == hashes, "source changed: use a fresh namespace"
    else:
        atomic_json(stamp, hashes)
    binary = BUILD / "sandwich"
    subprocess.run(["c++", "-std=c++17", "-O3", "-DNDEBUG", "-pthread",
                    str(ROOT / "verified/hgd_sandwich_cli.cpp"), "-o", str(binary)], check=True)
    atomic_json(OUT / "environment.json", {
        "protocol": "one serial launch per missing figure condition; complete IDN output checked",
        "repeats": 1,
        "scale_timeout_s": 60,
        "large_timeout_s": 180,
        "delta": 5,
        "flags": "-std=c++17 -O3 -DNDEBUG -pthread",
        "platform": platform.platform(),
        "compiler": subprocess.check_output(["c++", "--version"], text=True),
        "binary_sha256": digest(binary),
    })
    scale_reference, large_reference = reference_hashes()
    rows = []
    ledger = OUT / "measurements.json"
    existing = {r["key"]: r for r in json.loads(ledger.read_text())} if ledger.exists() else {}

    conditions = []
    for fraction, path, n in SCALE:
        for backend in ["flow", "reverse"]:
            conditions.append((f"scale_HB_{fraction}_{backend}_sandwich", "scale", "HB",
                               fraction, backend, path, n, 60, scale_reference[fraction]))
    for dataset, (path, n) in LARGE.items():
        for backend in ["flow", "reverse"]:
            conditions.append((f"large_{dataset}_{backend}_sandwich", "large", dataset,
                               1.0, backend, path, n, 180, large_reference[dataset]))

    for key, kind, dataset, fraction, backend, path, n, timeout, reference in conditions:
        if key in existing:
            row = existing[key]
        else:
            row = launch(key, [binary, backend, path, 5, n], timeout)
            row.update(kind=kind, dataset=dataset, fraction=fraction, backend=backend,
                       accelerated=True, repeats=1, input_sha256=digest(path))
            row["equal"] = row["status"] == "ok" and row["output_sha256"] == reference
            if row["status"] == "ok":
                assert row["equal"], key
        rows.append(row)
        atomic_json(ledger, rows)
        print(key, row["status"], f'{row["wall_s"]:.3f}s', flush=True)
    assert len(rows) == 18
    assert all(r["repeats"] == 1 and r["status"] in {"ok", "timeout"} for r in rows)
    assert all(r.get("equal", True) for r in rows if r["status"] == "ok")
    print("PASS: 18 single-run Sandwich integration conditions recorded")


if __name__ == "__main__":
    main()
