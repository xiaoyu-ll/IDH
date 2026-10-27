"""Direct full-edge F_5 audit, independent of the timed decomposition backends."""
import json
from pathlib import Path
import subprocess
from paper_large_experiments import digest
from paper_public_experiments import protocol

ROOT=Path(__file__).resolve().parents[1]
RESULT=ROOT/'paper_public_results'
subprocess.run(['make','build/test_real_intrinsic'],cwd=ROOT,check=True)
data=json.loads((RESULT/'datasets.json').read_text())
sources=['verified/hgd_intrinsic.hpp','verified/hgd_optimized.hpp','verified/hgd_exact.cpp',
         'verified/tests/test_real_intrinsic.cpp','scripts/check_public_intrinsic.py']
report=dict(source_hashes={s:digest(ROOT/s) for s in sources},datasets={})
for ds,(name,n) in protocol.SPECS.items():
    graph=ROOT/'dataset_large'/name; scores=RESULT/'runs'/f'{ds}_hgd_reverse_0.out'
    output=ROOT/'validation'/f'public_intrinsic_{ds}.csv'
    result=subprocess.run([str(ROOT/'build/test_real_intrinsic'),str(graph),'5',str(scores),str(n)],
                          capture_output=True,text=True,check=True,timeout=180)
    output.write_text(result.stdout)
    report['datasets'][ds]=dict(input_sha256=digest(graph),scores_sha256=digest(scores),
        profile_sha256=digest(output),layers=len(result.stdout.splitlines())-1)
    print(ds,report['datasets'][ds]['layers'],'layers PASS',flush=True)
(ROOT/'validation/public_intrinsic.json').write_text(json.dumps(report,indent=2)+'\n')
