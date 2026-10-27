"""Single-run search scalability experiment on HC.

For vertices and hyperedges, construct nested 20/40/60/80/100% samples with
one fixed permutation.  Run DSM-PATH, DSM-FLOW, and DSM-FLOW+ exactly once for
each condition.  All three methods must return the same threshold vertex set.
"""
from __future__ import annotations

import hashlib
import json
import platform
import random
import re
import subprocess
from pathlib import Path

import paper_experiments as engine


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "paper_search_scalability_single_results"
BUILD = ROOT / "paper_search_scalability_single_build"
INPUTS = OUT / "inputs"
FRACTIONS = [0.2, 0.4, 0.6, 0.8, 1.0]
METHODS = ["path", "flow", "flow+"]
N = 1290
SEED = 20261026


def digest(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def write_inputs() -> list[dict]:
    source = ROOT / "dataset/house-committees.txt"
    raw = [[int(v) for v in re.split(r"[,\s]+", line.strip()) if v]
           for line in source.read_text().splitlines()
           if line.strip() and not line.lstrip().startswith("#")]
    edges = [sorted(set(edge)) for edge in raw]
    INPUTS.mkdir(parents=True, exist_ok=True)
    edge_order = list(range(len(edges)))
    random.Random(SEED).shuffle(edge_order)
    vertex_order = list(range(1, N + 1))
    random.Random(SEED).shuffle(vertex_order)
    specs = []
    for fraction in FRACTIONS:
        edge_subset = [edges[i] for i in edge_order[:round(len(edges) * fraction)]]
        edge_path = INPUTS / f"HC_edges_{fraction:.1f}.txt"
        edge_path.write_text("".join(",".join(map(str, edge)) + "\n" for edge in edge_subset))
        specs.append(dict(dimension="edges", fraction=fraction, path=edge_path,
                          n=N, m=len(edge_subset), L=sum(map(len, edge_subset))))

        kept = set(vertex_order[:round(N * fraction)])
        remap = {old: new for new, old in enumerate(sorted(kept), 1)}
        vertex_edges = []
        for edge in edges:
            induced = [remap[v] for v in edge if v in kept]
            if induced:
                vertex_edges.append(induced)
        vertex_path = INPUTS / f"HC_vertices_{fraction:.1f}.txt"
        vertex_path.write_text("".join(",".join(map(str, edge)) + "\n" for edge in vertex_edges))
        specs.append(dict(dimension="vertices", fraction=fraction, path=vertex_path,
                          n=len(kept), m=len(vertex_edges), L=sum(map(len, vertex_edges))))
    return specs


def query_vertices(row: dict) -> list[int]:
    return [int(value) for value in engine.output_of(row).split()]


def main() -> None:
    OUT.mkdir(exist_ok=True)
    (OUT / "runs").mkdir(exist_ok=True)
    BUILD.mkdir(exist_ok=True)
    engine.OUT = OUT
    specs = write_inputs()
    sources = ["path.cpp", "flow.cpp", "flow+.cpp", "verified/hgd_optimized.hpp",
               "verified/hgd_optimized_cli.hpp", "scripts/paper_experiments.py",
               "scripts/paper_search_scalability_single.py"]
    hashes = {name: digest(ROOT / name) for name in sources}
    stamp = OUT / "source_hashes.json"
    if stamp.exists():
        assert json.loads(stamp.read_text()) == hashes, "source changed: use a fresh namespace"
    else:
        stamp.write_text(json.dumps(hashes, indent=2) + "\n")
    for method in METHODS:
        subprocess.run(["c++", "-std=c++17", "-O3", "-DNDEBUG", "-pthread",
                        str(ROOT / f"{method}.cpp"), "-o", str(BUILD / method)], check=True)
    (OUT / "environment.json").write_text(json.dumps({
        "protocol": "one serial launch per dimension/fraction/method; query sets checked",
        "repeats": 1, "timeout_s": 60, "k": 5, "delta": 5, "seed": SEED,
        "dataset": "HC", "platform": platform.platform(),
        "compiler": subprocess.check_output(["c++", "--version"], text=True),
        "flags": "-std=c++17 -O3 -DNDEBUG -pthread",
        "vertex_sampling": "nested strongly induced samples with dense remapping",
        "edge_sampling": "nested occurrence samples with the full vertex universe",
    }, indent=2) + "\n")
    ledger = OUT / "measurements.json"
    existing = {r["key"]: r for r in json.loads(ledger.read_text())} if ledger.exists() else {}
    rows = []
    for spec in sorted(specs, key=lambda item: (item["dimension"], item["fraction"])):
        expected = None
        for method in METHODS:
            key = f'HC_{spec["dimension"]}_{spec["fraction"]:.1f}_{method}'
            if key in existing:
                row = existing[key]
            else:
                command = [str(BUILD / method), str(spec["path"]), "5", "5", str(spec["n"])]
                row = engine.run(key, command, 60)
                row.update(dataset="HC", dimension=spec["dimension"], fraction=spec["fraction"],
                           method=method, repeats=1, n=spec["n"], m=spec["m"], L=spec["L"],
                           k=5, delta=5, input_sha256=digest(spec["path"]))
            assert row["status"] == "ok", row
            answer = query_vertices(row)
            if expected is None:
                expected = answer
            assert answer == expected, key
            row["equal"] = True
            rows.append(row)
            ledger.write_text(json.dumps(rows, indent=2) + "\n")
    assert len(rows) == 30
    print("PASS: 30 single-run HC search scalability conditions; all query sets agree")


if __name__ == "__main__":
    main()
