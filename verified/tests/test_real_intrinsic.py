"""Reevaluate all six released hierarchies using full-graph F_delta margins."""
import csv
import hashlib
import io
import json
from pathlib import Path
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'scripts'))
from paper_experiments import SPECS

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

report={}
for ds,(name,n) in SPECS.items():
    graph=ROOT/'dataset'/name
    scores=ROOT/'paper_comparison_results/runs'/f'{ds}_hgd_flow_0.out'
    other=ROOT/'paper_comparison_results/runs'/f'{ds}_hgd_reverse_0.out'
    assert scores.read_bytes()==other.read_bytes()
    result=subprocess.run([str(ROOT/'build/test_real_intrinsic'),str(graph),'5',str(scores),str(n)],
                          check=True,text=True,capture_output=True,timeout=60)
    layers=[{key:int(value) for key,value in row.items()}
            for row in csv.DictReader(io.StringIO(result.stdout))]
    assert sum(row['size'] for row in layers)==n
    report[ds]=dict(input_sha256=digest(graph),scores_sha256=digest(scores),layers=layers)
sources=['verified/hgd_intrinsic.hpp','verified/tests/test_real_intrinsic.cpp',
         'verified/tests/test_real_intrinsic.py','verified/hgd_exact.cpp','verified/hgd_optimized.hpp']
payload=dict(source_hashes={rel:digest(ROOT/rel) for rel in sources},datasets=report)
(ROOT/'validation/intrinsic_real_profiles.json').write_text(json.dumps(payload,indent=2)+'\n')
print(f'PASS: {sum(len(row["layers"]) for row in report.values())} occupied layers on six datasets; '
      'both released IDN arrays; full-graph F differences, strict intervals and total budgets')
