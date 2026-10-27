"""Single-run HB scalability experiment varying vertices and hyperedges.

Each dimension uses nested 20/40/60/80/100% samples.  Vertex samples are
strongly induced: endpoints outside the sampled vertex set are removed,
empty occurrences are omitted, and retained vertices are remapped to a dense
1-based universe.  Edge samples retain the full 1,494-vertex universe.
Every method/condition is launched exactly once with the unchanged 60 s
decomposition deadline.  Complete IDN arrays must agree across all four
executions of a condition.
"""
from __future__ import annotations

import hashlib
import json
import platform
import random
import subprocess
from pathlib import Path

import paper_experiments as engine


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "paper_scalability_single_v2_results"
BUILD = ROOT / "paper_scalability_single_v2_build"
INPUTS = OUT / "inputs"
FRACTIONS = [0.2, 0.4, 0.6, 0.8, 1.0]
METHODS = ["flow", "reverse", "flow_s", "reverse_s"]


def digest(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def write_inputs() -> list[dict]:
    source = ROOT / "dataset/house-bills.txt"
    edges = [sorted({int(v) for v in line.replace(",", " ").split()})
             for line in source.read_text().splitlines() if line.strip()]
    INPUTS.mkdir(parents=True, exist_ok=True)
    edge_order = list(range(len(edges)))
    random.Random(20261023).shuffle(edge_order)
    vertex_order = list(range(1, 1495))
    random.Random(20261023).shuffle(vertex_order)
    specs = []
    for fraction in FRACTIONS:
        edge_subset = [edges[i] for i in edge_order[:round(len(edges) * fraction)]]
        edge_path = INPUTS / f"HB_edges_{fraction:.1f}.txt"
        edge_path.write_text("".join(",".join(map(str, e)) + "\n" for e in edge_subset))
        specs.append(dict(dimension="edges", fraction=fraction, path=edge_path,
                          n=1494, m=len(edge_subset), L=sum(map(len, edge_subset))))

        kept = set(vertex_order[:round(1494 * fraction)])
        remap = {old: new for new, old in enumerate(sorted(kept), 1)}
        vertex_edges = []
        for edge in edges:
            induced = [remap[v] for v in edge if v in kept]
            if induced:
                vertex_edges.append(induced)
        vertex_path = INPUTS / f"HB_vertices_{fraction:.1f}.txt"
        vertex_path.write_text("".join(",".join(map(str, e)) + "\n" for e in vertex_edges))
        specs.append(dict(dimension="vertices", fraction=fraction, path=vertex_path,
                          n=len(kept), m=len(vertex_edges), L=sum(map(len, vertex_edges))))
    return specs


def main() -> None:
    OUT.mkdir(exist_ok=True)
    (OUT / "runs").mkdir(exist_ok=True)
    BUILD.mkdir(exist_ok=True)
    engine.OUT = OUT
    specs = write_inputs()
    sources = [
        "hgd_flow.cpp", "hgd_reverse.cpp", "scc_cached_plus.cpp",
        "scc_reverse_csr_plus.cpp", "verified/hgd_sandwich_cli.cpp",
        "verified/hgd_sandwich.hpp", "verified/hgd_optimized.hpp",
        "verified/hgd_exact.cpp", "scripts/paper_experiments.py",
        "scripts/paper_scalability_single_v2.py",
    ]
    hashes = {name: digest(ROOT / name) for name in sources}
    stamp = OUT / "source_hashes.json"
    if stamp.exists():
        assert json.loads(stamp.read_text()) == hashes, "source changed: use a fresh namespace"
    else:
        stamp.write_text(json.dumps(hashes, indent=2) + "\n")
    commands = {
        "flow": ("hgd_flow.cpp", None),
        "reverse": ("hgd_reverse.cpp", None),
        "flow_s": ("verified/hgd_sandwich_cli.cpp", "flow"),
        "reverse_s": ("verified/hgd_sandwich_cli.cpp", "reverse"),
    }
    for method, (source, _) in commands.items():
        binary = BUILD / method
        subprocess.run(["c++", "-std=c++17", "-O3", "-DNDEBUG", "-pthread",
                        str(ROOT / source), "-o", str(binary)], check=True)
    (OUT / "environment.json").write_text(json.dumps({
        "protocol": "one serial launch per dimension/fraction/method; complete arrays checked",
        "repeats": 1, "timeout_s": 60, "delta": 5, "seed": 20261023,
        "vertex_sampling": "nested strongly induced samples; dense 1-based remapping; empty occurrences omitted",
        "edge_sampling": "nested occurrence samples; full 1494-vertex universe retained",
        "platform": platform.platform(),
        "compiler": subprocess.check_output(["c++", "--version"], text=True),
        "flags": "-std=c++17 -O3 -DNDEBUG -pthread",
    }, indent=2) + "\n")
    rows = []
    ledger = OUT / "measurements.json"
    existing = {r["key"]: r for r in json.loads(ledger.read_text())} if ledger.exists() else {}
    for spec in sorted(specs, key=lambda x: (x["dimension"], x["fraction"])):
        expected = None
        for method in METHODS:
            key = f'HB_{spec["dimension"]}_{spec["fraction"]:.1f}_{method}'
            if key in existing:
                row = existing[key]
            else:
                source, backend = commands[method]
                binary = BUILD / method
                if backend is None:
                    command = [str(binary), str(spec["path"]), "5", str(spec["n"])]
                else:
                    command = [str(binary), backend, str(spec["path"]), "5", str(spec["n"])]
                row = engine.run(key, command, 60)
                row.update(dataset="HB", dimension=spec["dimension"],
                           fraction=spec["fraction"], method=method, repeats=1,
                           n=spec["n"], m=spec["m"], L=spec["L"], delta=5,
                           input_sha256=digest(spec["path"]))
            assert row["status"] == "ok", row
            ids = engine.ids_of(row, spec["n"])
            if expected is None:
                expected = ids
            assert ids == expected, key
            row["equal"] = True
            rows.append(row)
            ledger.write_text(json.dumps(rows, indent=2) + "\n")
    assert len(rows) == 40
    print("PASS: 40 single-run HB vertex/edge scalability conditions; all complete arrays agree")


if __name__ == "__main__":
    main()
