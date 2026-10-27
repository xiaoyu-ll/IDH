"""Run DBLP Flow-Decomp once to completion in a fresh result namespace."""

import hashlib
import json
import os
import platform
import subprocess
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "paper_dblp_flow_completion_single_results"
RUNS = OUT / "runs"
BINARY = ROOT / "paper_public_build" / "hgd_flow"
INPUT = ROOT / "dataset_large" / "coauth-dblp.txt"
N = 1_930_378
DELTA = 5
KEY = "DBLP_hgd_flow_completion_single"


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


def expected_output_hash():
    rows = json.loads((ROOT / "paper_public_results" / "measurements.json").read_text())
    hashes = {row["output_sha256"] for row in rows
              if row.get("dataset") == "DBLP" and row.get("method") == "hgd_reverse"
              and row.get("status") == "ok"}
    assert len(hashes) == 1
    return hashes.pop()


def validate_output(path):
    count = 0
    with Path(path).open() as stream:
        for count, line in enumerate(stream, 1):
            vertex, score = map(int, line.split(","))
            assert vertex == count and score >= 0
    assert count == N
    return count


def main():
    OUT.mkdir(exist_ok=True)
    RUNS.mkdir(exist_ok=True)
    ledger = OUT / "measurements.json"
    if ledger.exists():
        rows = json.loads(ledger.read_text())
        if rows and rows[0].get("status") == "ok":
            print("PASS: existing completed single run retained")
            return
        raise RuntimeError("An unfinished record exists; inspect it before any new launch")

    source_hashes = {
        "hgd_flow.cpp": digest(ROOT / "hgd_flow.cpp"),
        "scc_cached_plus.cpp": digest(ROOT / "scc_cached_plus.cpp"),
        "verified/hgd_optimized.hpp": digest(ROOT / "verified/hgd_optimized.hpp"),
        "scripts/paper_dblp_flow_completion_single.py": digest(Path(__file__)),
    }
    historical = json.loads((ROOT / "paper_public_results" / "source_hashes.json").read_text())
    for name in ["hgd_flow.cpp", "scc_cached_plus.cpp", "verified/hgd_optimized.hpp"]:
        assert source_hashes[name] == historical[name]

    input_hash = digest(INPUT)
    assert input_hash == "c58d20a5c14d53e831f457e84ad9eb122f31fa15861abfff8247bf4bd608127e"
    binary_hash = digest(BINARY)
    expected = expected_output_hash()
    atomic_json(OUT / "source_hashes.json", source_hashes)
    atomic_json(OUT / "environment.json", {
        "protocol": "one serial DBLP Flow-Decomp execution continued until process exit",
        "repeats": 1,
        "deadline_s": None,
        "delta": DELTA,
        "n": N,
        "flags": "-std=c++17 -O3 -DNDEBUG -pthread",
        "platform": platform.platform(),
        "binary_sha256": binary_hash,
        "input_sha256": input_hash,
        "expected_output_sha256": expected,
        "started": time.strftime("%Y-%m-%d %H:%M:%S %z"),
    })

    command = [str(BINARY), str(INPUT), str(DELTA), str(N)]
    output = RUNS / f"{KEY}.out"
    error = RUNS / f"{KEY}.err"
    running = {
        "key": KEY,
        "dataset": "DBLP",
        "method": "hgd_flow",
        "trial": 0,
        "repeats": 1,
        "status": "running",
        "command": command,
        "started": time.strftime("%Y-%m-%d %H:%M:%S %z"),
        "input_sha256": input_hash,
        "binary_sha256": binary_hash,
        "deadline_s": None,
    }
    atomic_json(ledger, [running])
    start = time.perf_counter()
    with output.open("wb") as stdout, error.open("wb") as stderr:
        process = subprocess.Popen(command, cwd=ROOT, stdout=stdout, stderr=stderr,
                                   start_new_session=True)
        running["pid"] = process.pid
        atomic_json(ledger, [running])
        try:
            _, wait_status, usage = os.wait4(process.pid, 0)
        except BaseException:
            try:
                os.killpg(process.pid, 15)
            except ProcessLookupError:
                pass
            running.update(status="interrupted", wall_s=time.perf_counter() - start)
            atomic_json(ledger, [running])
            raise
    code = os.waitstatus_to_exitcode(wait_status)
    state = "ok" if code == 0 else "error"
    row = dict(running)
    row.update(status=state, wall_s=time.perf_counter() - start, returncode=code,
               peak_rss_mib=usage.ru_maxrss / (1024 ** 2 if platform.system() == "Darwin" else 1024),
               output_sha256=digest(output) if state == "ok" else "",
               stderr_tail=error.read_text(errors="replace")[-4000:])
    if state == "ok":
        row["output_rows"] = validate_output(output)
        row["equal"] = row["output_sha256"] == expected
        assert row["equal"]
    atomic_json(ledger, [row])
    if state != "ok":
        raise RuntimeError(f"{KEY} exited with {code}: {row['stderr_tail']}")
    print(f"PASS: {KEY} completed in {row['wall_s']:.6f}s; peak RSS {row['peak_rss_mib']:.3f} MiB")


if __name__ == "__main__":
    main()
