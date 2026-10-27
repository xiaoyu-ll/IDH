"""Direct F_delta layer audit of the original full TC/SA decompositions."""
import hashlib
import json
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[1]
RESULT=ROOT/'paper_large_results'

def digest(path):
    h=hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda:f.read(1024*1024),b''):h.update(block)
    return h.hexdigest()

subprocess.run(['make','build/test_real_intrinsic'],cwd=ROOT,check=True)
data=json.loads((RESULT/'datasets.json').read_text())
sources=['verified/hgd_intrinsic.hpp','verified/hgd_optimized.hpp','verified/hgd_exact.cpp',
         'verified/tests/test_real_intrinsic.cpp','scripts/check_large_intrinsic.py']
report=dict(source_hashes={rel:digest(ROOT/rel) for rel in sources},datasets={})
for ds,name in [('TC','trivago.txt'),('SA','stackoverflow-answers.txt')]:
    graph=ROOT/'dataset_large'/name
    scores=RESULT/'runs'/f'{ds}_hgd_reverse_0.out'
    output=ROOT/'validation'/f'large_intrinsic_{ds}.csv'
    command=[str(ROOT/'build/test_real_intrinsic'),str(graph),'5',str(scores),str(data[ds]['n'])]
    result=subprocess.run(command,capture_output=True,text=True,check=True,timeout=120)
    output.write_text(result.stdout)
    report['datasets'][ds]=dict(input_sha256=digest(graph),scores_sha256=digest(scores),
                               profile_sha256=digest(output),layers=len(result.stdout.splitlines())-1)
    print(f'PASS: {ds}, {report["datasets"][ds]["layers"]} occupied layers; direct F margins, strict intervals and budget conservation')
(ROOT/'validation/large_intrinsic.json').write_text(json.dumps(report,indent=2)+'\n')
