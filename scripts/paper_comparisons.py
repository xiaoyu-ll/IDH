"""Fresh, output-checked measurements for the two algorithms and four models."""
import csv
import hashlib
import json
from pathlib import Path
import platform
import random
import statistics
import subprocess
import paper_experiments as engine

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'paper_comparison_results'
METHODS=['hgd_flow','hgd_reverse','core','neighbor','cocore','abcore']


def quality(edges,scores):
    # Evaluate nested threshold sets under the SAME fully-contained-edge metric.
    levels=sorted(set(scores))
    best_rho=best_edge_ratio=0.
    for k in levels:
        size=sum(x>=k for x in scores)
        kept=[e for e in edges if min(scores[v] for v in e)>=k]
        best_rho=max(best_rho,sum(map(len,kept))/size)
        best_edge_ratio=max(best_edge_ratio,len(kept)/size)
    return dict(levels=len(levels),kmax=max(scores,default=0),
                best_degree_density=best_rho,best_edge_vertex_ratio=best_edge_ratio)


def main():
    OUT.mkdir(exist_ok=True);(OUT/'runs').mkdir(exist_ok=True)
    engine.OUT=OUT
    build=ROOT/'paper_comparison_build';build.mkdir(exist_ok=True)
    sources=[ROOT/x for x in ['hgd_flow.cpp','hgd_reverse.cpp','scc_cached_plus.cpp',
             'scc_reverse_csr_plus.cpp','scc_all.cpp','verified/paper_baselines.cpp',
             'verified/hgd_optimized.hpp','verified/hgd_exact.cpp',
             'scripts/paper_experiments.py','scripts/paper_comparisons.py']]
    hashes={str(p.relative_to(ROOT)):engine.digest(p) for p in sources}
    stamp=OUT/'source_hashes.json'
    if stamp.exists():assert json.loads(stamp.read_text())==hashes,'changed source: use new results directory'
    stamp.write_text(json.dumps(hashes,indent=2)+'\n')
    for name,src in [('hgd_flow','hgd_flow.cpp'),('hgd_reverse','hgd_reverse.cpp'),('paper_baselines','verified/paper_baselines.cpp')]:
        binary=build/name
        if not binary.exists():
            subprocess.run(['c++','-std=c++17','-O3','-DNDEBUG','-pthread',str(ROOT/src),'-o',str(binary)],check=True)
    compiler=subprocess.check_output(['c++','--version'],text=True)
    environment=dict(platform=platform.platform(),compiler=compiler,
        flags='-std=c++17 -O3 -DNDEBUG -pthread',repeats=3,timeout_s=120,
        cpu='Apple M4 Pro',ram_bytes=25769803776,logical_cpus=12,
        protocol='serial; shuffled method order; end-to-end wall time and per-process wait4 peak RSS')
    (OUT/'environment.json').write_text(json.dumps(environment,indent=2)+'\n')
    rows=[];structural={}
    for ds,(name,n) in engine.SPECS.items():
        path=ROOT/'dataset'/name
        edges=[set(int(v)-1 for v in line.replace(',',' ').split()) for line in path.read_text().splitlines() if line.strip()]
        reference=[int(line.split(',')[1]) for line in (ROOT/'paper_results_final/runs'/f'reference_{ds}_d5.out').read_text().splitlines()]
        outputs={}
        for trial in range(3):
            order=METHODS[:];random.Random(20261020+trial).shuffle(order)
            for method in order:
                if method.startswith('hgd_'):cmd=[str(build/method),str(path),'5',str(n)]
                else:cmd=[str(build/'paper_baselines'),str(path),method,'5' if method in ['cocore','abcore'] else '1',str(n)]
                r=engine.run(f'{ds}_{method}_{trial}',cmd,120)
                assert r['status']=='ok',r
                scores=engine.ids_of(r,n)
                if method.startswith('hgd_'):assert scores==reference,(ds,method)
                if method in outputs:assert scores==outputs[method]
                outputs[method]=scores
                rows.append(dict(r,dataset=ds,method=method,trial=trial,input_sha256=engine.digest(path)))
        structural[ds]={method:dict(quality(edges,outputs[method]),scores=outputs[method]) for method in METHODS}
        (OUT/'measurements.json').write_text(json.dumps(rows,indent=2)+'\n')
        (OUT/'structural.json').write_text(json.dumps(structural,indent=2)+'\n')
    with (OUT/'measurements.csv').open('w',newline='') as f:
        fields=['dataset','method','trial','wall_s','peak_rss_mib','status','output_sha256']
        writer=csv.DictWriter(f,fields,extrasaction='ignore');writer.writeheader();writer.writerows(rows)
    print('PASS: 108 fresh runs; two algorithm outputs agree with released references; baseline repeat outputs agree')


if __name__=='__main__':main()
