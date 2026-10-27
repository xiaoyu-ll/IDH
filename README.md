# Density Decomposition on Hypergraphs

This repository contains the implementation and reproducibility material for
**Density Decomposition on Hypergraphs**.

The current release matches the VLDB Journal manuscript in `paper/`. It uses
the capped hyperedge contribution

\[
F_\delta(X)=\sum_{e\in E}\max\{0,r_e-|e\setminus X|\},
\qquad r_e=\min\{\delta,|e|\}.
\]

## Implemented algorithms

The paper evaluates three dense-set search algorithms:

- `path`: DSM-PATH
- `flow`: DSM-FLOW
- `flow+`: DSM-FLOW+

It evaluates two complete decomposition algorithms:

- `hgd_flow`: Flow-Decomp, using SCC condensation
- `hgd_reverse`: Reverse-Decomp, using reverse propagation

`verified/hgd_sandwich_cli.cpp` applies the certified Sandwich bounds to either
decomposition backend. `insert`, `delete`, `hgd_stream`, and
`hgd_stream_reverse` implement static-batch and streaming maintenance.

## Build

Requirements: a C++17 compiler, GNU Make, and Python 3 for the test and
experiment drivers.

```bash
make -j
```

The executables are written to `build/`.

## Input format

An input hypergraph is a text file containing one hyperedge per line. Vertex
identifiers are positive integers separated by commas. Repeated hyperedge
records are retained; repeated vertices within one record are normalized by the
loader. An optional final command-line argument supplies the total number of
vertices when isolated vertices must be retained.

## Examples

Search for the `(5,5)`-dense set:

```bash
./build/path query dataset/house-bills.txt 5 5 1494
./build/flow query dataset/house-bills.txt 5 5 1494
./build/flow+ query dataset/house-bills.txt 5 5 1494
```

Compute all density levels:

```bash
./build/hgd_flow dataset/house-bills.txt 5 1494
./build/hgd_reverse dataset/house-bills.txt 5 1494
./build/hgd_sandwich flow dataset/house-bills.txt 5 1494
./build/hgd_sandwich reverse dataset/house-bills.txt 5 1494
```

Each decomposition command writes `vertex_id,density_level` pairs to standard
output. Diagnostics and counters are written to standard error.

## Tests

```bash
make test
```

The suite compares optimized implementations against independent subset and
orientation oracles, checks reverse reachability, dynamic updates, Sandwich
residual identities, and boundary cases. Large-scale boundary checks are
available separately:

```bash
make test-scale
```

## Data and experiments

Six small public datasets are included under `dataset/`. The large inputs are
not committed because several files exceed GitHub's size limit. Their sources,
checksums, and normalization rules are documented in `dataset_large/README.md`.
The pinned TM and DBLP releases can be downloaded and converted with:

```bash
python3 -m pip install ijson
python3 scripts/download_public_data.py TM DBLP
python3 scripts/convert_public_hif.py TM
python3 scripts/convert_public_hif.py DBLP
```

Paper experiment drivers are under `scripts/`. Published measurement summaries,
environment records, and source hashes are under `results/`; large per-vertex
outputs and compiled binaries are intentionally excluded. Every new condition
uses one execution unless a preserved historical result explicitly records a
different earlier procedure. Table II contains one checked execution per
dataset/algorithm condition, and `TO` denotes an execution that actually reached
43,200 seconds.

The AMLSim case study uses source files that are not redistributed here. Set
`HGD_CASE_STUDY_DATA` to a directory containing `amlsim_accounts.csv`,
`amlsim_transactions.csv`, and the alert hypergraph before running
`scripts/paper_case_studies_single.py`.

## Paper

The current source and compiled manuscript are under `paper/`. Build it with:

```bash
cd paper
latexmk -pdf -interaction=nonstopmode -halt-on-error hgd_vldbj.tex
```

## Repository structure

- root C++ files: paper algorithms and command-line entry points
- `verified/`: shared implementations, independent references, and tests
- `scripts/`: data preparation and experiment drivers
- `dataset/`: bundled small inputs
- `dataset_large/`: large-data provenance and download instructions
- `results/`: compact measurement records and source hashes
- `paper/`: VLDB Journal manuscript, figures, bibliography, and PDF

Please cite the paper metadata in `CITATION.cff` when using this code.
