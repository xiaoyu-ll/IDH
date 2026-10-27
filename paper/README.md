# VLDB Journal manuscript

`hgd_vldbj.tex` is the current two-column VLDB Journal manuscript. The checked
build is 21 pages and stays below the journal's 25-page limit.

Build with:

```bash
latexmk -pdf -interaction=nonstopmode -halt-on-error hgd_vldbj.tex
```

The directory contains the class and bibliography style files required by the
manuscript, all referenced PDF figures, the bibliography database, the compiled
PDF, plotting scripts, plotted-point manifests, and a release consistency test.

Recreate all current plots with `python3 render_vldbj_figures.py --export` from
this directory. The script reads the compact ledgers under `../results/`.

To validate the checked PDF and source:

```bash
python3 test_vldbj_release.py
```

The paper reports three search algorithms, two decomposition algorithms, their
Sandwich-accelerated executions, insertion/deletion maintenance, scalability,
large-data comparisons, and two case studies. Table II uses a single checked
search for every condition; a timeout is recorded only after 43,200 seconds.
