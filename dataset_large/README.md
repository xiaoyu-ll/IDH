# Large datasets and public releases

TC and SA are the original instances used by the experiments.
Only endpoint repetitions inside a record are normalized; equal records remain
distinct occurrences. The algorithms retain every original vertex ID.

- `trivago.txt`: obtained from the Cornell Hypergraph Repository.
  TC: 172,738 vertices, 233,202 records, 726,861 incidences, maximum edge size 85.
  SHA256 `fd8240ee24b2896085e0e22439e2b70981c847e93e8f1d2332f9ebc15a1b15f8`.
- `stackoverflow-answers.txt`: extracted without modification from
  `stackoverflow-answers/hyperedges-stackoverflow-answers.txt` in the Cornell
  distribution.
  SA: 15,211,989 vertices, 1,103,243 records, 26,109,177 incidences, maximum
  edge size 61,315.
  SHA256 `f0f350d3e77caa6f938fb6dc9dec5e25f218058b55c200bcf0ddc459e9faf31c`.

The source dataset pages already cited by the earlier manuscript are
https://www.cs.cornell.edu/~arb/data/trivago-clicks/ and
https://www.cs.cornell.edu/~arb/data/stackoverflow-answers/ . No replacement
dataset was used to generate the TC/SA measurements. All reported statistics are recomputed from these files.
The AR endpoint file is not present in the recovered folders; label files alone
are not enough to rerun AR, so no legacy AR results are reused.

Run `python3 scripts/paper_large_experiments.py` from the repository root.
Compact source/input hashes and measurement summaries are retained under
`results/`; large output arrays are not stored in Git.

## Additional public inputs (2026-10-20)

The user subsequently authorized separate suitable public datasets. These do not
replace TC or SA and do not reuse legacy AR measurements.

- Math Stack Exchange threads (TM), XGI HIF v0.1:
  https://zenodo.org/records/21938817/files/threads-math-sx.json?download=1
  DOI 10.5281/zenodo.21938817; official MD5 `756dc8548abfc3333a4fab2cb8fb16be`.
  Actual input: n=176,445, m=719,792, L=1,610,393, edge sizes 1--21.
- DBLP coauthorship, XGI HIF v0.2:
  https://zenodo.org/records/21905305/files/coauth-dblp.json?download=1
  DOI 10.5281/zenodo.21905305; official MD5 `40c82ca508836dc484863c4af6224bd8`.
  Actual input: n=1,930,378, m=3,700,681, L=10,326,773, edge sizes 1--280.

Both are curated by Nicholas Landry and originate from Benson, Abebe, Schaub,
Jadbabaie and Kleinberg, *Simplicial closure and higher-order link prediction*,
PNAS (2018), DOI 10.1073/pnas.1800683115. Timestamps are not used in the static
hypergraph or occurrence-based batch benchmarks.

Raw HIF files are in `downloads/`. `scripts/download_public_data.py TM DBLP`
supports bounded resumable curl downloads and verifies publisher MD5 values.
Run `scripts/convert_public_hif.py TM` and
`scripts/convert_public_hif.py DBLP` with ijson 3.6.0 installed. The converter
first checks the official MD5, then deterministically maps labels, retains every
edge occurrence (including singleton and equal records), and removes only repeated
incidences within one edge. No author-count cutoff or component filtering is used.
Explicit isolated node records, if supplied, are retained. Directed input and
unrepresentable empty edges fail explicitly. Normalized text, both ID mappings,
raw/normalized SHA256 and actual counts are bundled alongside each input.
The actual DBLP HIF counts differ from the old counts in the descriptive page;
only the downloaded file's recomputed counts are used by the paper.

The public-data measurements are independent of the TC/SA measurements. Reruns use
`scripts/paper_public_experiments.py`; correctness profiles use
`scripts/check_public_intrinsic.py`. Preserve existing result directories before
remeasuring, since successful records are resumable.
