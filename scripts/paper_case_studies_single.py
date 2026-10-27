"""Reproduce the two recovered case studies once with the current algorithms."""
from __future__ import annotations

import csv
import hashlib
import json
import os
import re
import shutil
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "paper_case_study_single_results"
INPUTS = OUT / "inputs"
RUNS = OUT / "runs"
BUILD = ROOT / "paper_case_study_single_build"
SOURCE = Path(os.environ.get("HGD_CASE_STUDY_DATA", ROOT / "dataset_external"))


def digest(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def execute(key: str, command: list[str], stdout: Path | None = None) -> dict:
    err = RUNS / f"{key}.err"
    target = stdout or RUNS / f"{key}.out"
    with target.open("wb") as out, err.open("wb") as e:
        result = subprocess.run(command, stdout=out, stderr=e)
    if result.returncode:
        raise RuntimeError(f"{key}: {err.read_text()}")
    return {"key": key, "command": command, "status": "ok",
            "output_sha256": digest(target)}


def convert_zero_based_hypergraph(source: Path, target: Path) -> None:
    with source.open() as src, target.open("w") as dst:
        for line in src:
            values = [int(x) + 1 for x in line.replace(",", " ").split()]
            if values:
                dst.write(",".join(map(str, values)) + "\n")


def to_account_layers(score_path: Path, target: Path) -> None:
    with score_path.open() as src, target.open("w", newline="") as dst:
        writer = csv.writer(dst)
        writer.writerow(["AccountID", "Layer"])
        for line in src:
            vertex, layer = map(int, line.split(","))
            writer.writerow([vertex - 1, layer])


def parse_top50(path: Path) -> dict:
    rows = list(csv.DictReader(line for line in path.read_text().splitlines()
                               if line and not line.startswith("#") and not line.startswith("num_")
                               and not line.startswith("windows:")))
    row = next(r for r in rows if r["window"] == "top50")
    return {k: float(row[k]) if k not in {"window", "N", "tp", "fp", "fn"}
            else (row[k] if k == "window" else int(row[k])) for k in row}


def parse_legislative(path: Path) -> dict:
    text = path.read_text()
    diag = re.search(r"Train bills=(\d+), Test bills=(\d+)", text)
    counts = re.search(r"Positives\(Test-only-new\)=(\d+), Negatives=(\d+)", text)
    metric = re.search(r"AUC=([0-9.]+)\s+AP=([0-9.]+)", text)
    assert diag and counts and metric, text
    return {"train_bills": int(diag.group(1)), "test_bills": int(diag.group(2)),
            "positive_pairs": int(counts.group(1)), "negative_pairs": int(counts.group(2)),
            "auc": float(metric.group(1)), "ap": float(metric.group(2))}


def main() -> None:
    ledger = OUT / "measurements.json"
    if ledger.exists():
        print("PASS: existing single-run case-study ledger retained; no repetition")
        return
    INPUTS.mkdir(parents=True, exist_ok=True)
    RUNS.mkdir(exist_ok=True)
    BUILD.mkdir(exist_ok=True)
    external = {
        "accounts": SOURCE / "amlsim_accounts.csv",
        "transactions": SOURCE / "amlsim_transactions.csv",
        "alerts": SOURCE / "amlsim由alerts建模/amlsim.txt",
    }
    for name, source in external.items():
        if name != "alerts":
            shutil.copy2(source, INPUTS / source.name)
    alerts = INPUTS / "amlsim_alerts_1based.txt"
    convert_zero_based_hypergraph(external["alerts"], alerts)
    senate = ROOT / "dataset/senate-bills.txt"
    bills = [line for line in senate.read_text().splitlines() if line.strip()]
    cut = int(len(bills) * 0.8)
    train = INPUTS / "senate_bills_train80.txt"
    test = INPUTS / "senate_bills_test20.txt"
    train.write_text("\n".join(bills[:cut]) + "\n")
    test.write_text("\n".join(bills[cut:]) + "\n")
    binaries = {
        "reverse": ROOT / "hgd_reverse.cpp",
        "baselines": ROOT / "verified/paper_baselines.cpp",
        "case_a": ROOT / "casestudya.cpp",
        "case_b": ROOT / "casestudyb.cpp",
    }
    for name, source in binaries.items():
        subprocess.run(["c++", "-std=c++17", "-O3", "-DNDEBUG", "-pthread",
                        str(source), "-o", str(BUILD / name)], check=True)
    rows = []
    methods = {
        "Proposed": [str(BUILD / "reverse"), str(alerts), "5", "10000"],
        "Degree": [str(BUILD / "baselines"), str(alerts), "core", "1", "10000"],
        "Neighbor": [str(BUILD / "baselines"), str(alerts), "neighbor", "1", "10000"],
    }
    fraud = {}
    for method, command in methods.items():
        score = RUNS / f"amlsim_{method.lower()}.scores"
        rows.append(execute(f"amlsim_score_{method.lower()}", command, score))
        layers = RUNS / f"amlsim_{method.lower()}.csv"
        to_account_layers(score, layers)
        result_dir = RUNS / f"amlsim_{method.lower()}_evaluation"
        command = [str(BUILD / "case_a"), str(layers), str(INPUTS / "amlsim_accounts.csv"),
                   str(INPUTS / "amlsim_transactions.csv"), str(result_dir)]
        rows.append(execute(f"amlsim_eval_{method.lower()}", command))
        fraud[method] = parse_top50(result_dir / "windows_summary.txt")

    legislative = {}
    senate_methods = {
        "Proposed": [str(BUILD / "reverse"), str(train), "10", "294"],
        "Degree": [str(BUILD / "baselines"), str(train), "core", "1", "294"],
        "Neighbor": [str(BUILD / "baselines"), str(train), "neighbor", "1", "294"],
    }
    for method, command in senate_methods.items():
        score = RUNS / f"senate_{method.lower()}.scores"
        rows.append(execute(f"senate_score_{method.lower()}", command, score))
        output = RUNS / f"senate_{method.lower()}_prediction.out"
        command = [str(BUILD / "case_b"), str(train), str(test), str(score),
                   "--only_new", "--neg_ratio", "5", "--neg_degmatch", "--seed", "42"]
        rows.append(execute(f"senate_eval_{method.lower()}", command, output))
        legislative[method] = parse_legislative(output)
    payload = {
        "protocol": "one execution per model and case-study condition",
        "fraud_top50": fraud, "legislative_prediction": legislative,
        "runs": rows,
        "input_sha256": {"accounts": digest(INPUTS / "amlsim_accounts.csv"),
                         "transactions": digest(INPUTS / "amlsim_transactions.csv"),
                         "alerts": digest(alerts), "senate": digest(senate)},
    }
    ledger.write_text(json.dumps(payload, indent=2) + "\n")
    print(json.dumps({"fraud_top50": fraud, "legislative_prediction": legislative}, indent=2))
    print("PASS: two case studies reproduced once with current score generators")


if __name__ == "__main__":
    main()
