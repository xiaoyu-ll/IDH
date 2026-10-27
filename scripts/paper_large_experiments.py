"""Serial, resumable experiments on the original TC/SA datasets, not replacements.

Usage: python3 scripts/paper_large_experiments.py [--phase static|dynamic|all]
Each successful run saves its complete stdout; timeouts are censored, not times.
The large raw edge files are bundled under dataset_large/.
"""
import argparse
import csv
import hashlib
import json
import platform
from pathlib import Path
import random
import subprocess
import sys

import paper_experiments as engine

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'paper_large_results'
BUILD=ROOT/'paper_large_build'
SPECS={'TC':('trivago.txt',172738),'SA':('stackoverflow-answers.txt',15211989)}
METHODS=['hgd_flow','hgd_reverse','core','neighbor','cocore','abcore']

def digest(path):
    h=hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda:f.read(1024*1024),b''):h.update(block)
    return h.hexdigest()

def audit(path,n):
    m=L=rank=removed=pairwork=0;small=sys.maxsize;seen=bytearray(n)
    with path.open() as f:
        for line in f:
            if not line.strip() or line.lstrip().startswith('#'):continue
            raw=[int(v)-1 for v in line.replace(',',' ').split()]
            edge=set(raw)
            if not edge or min(edge)<0 or max(edge)>=n:raise ValueError('invalid edge')
            for v in edge:seen[v]=1
            size=len(edge);m+=1;L+=size;rank=max(rank,size);small=min(small,size)
            removed+=len(raw)-size;pairwork+=size*(size-1)
    return dict(n=n,m=m,L=L,max_size=rank,min_size=small,
                active_vertices=sum(seen),removed_incidences=removed,
                ordered_pair_initializations=pairwork,sha256=digest(path))

def main():
    p=argparse.ArgumentParser();p.add_argument('--phase',choices=['static','dynamic','all'],default='all')
    p.add_argument('--datasets',nargs='+',choices=list(SPECS),default=list(SPECS))
    p.add_argument('--repeats',type=int,default=3)
    args=p.parse_args()
    OUT.mkdir(exist_ok=True);(OUT/'runs').mkdir(exist_ok=True);BUILD.mkdir(exist_ok=True)
    engine.OUT=OUT
    sources=['hgd_flow.cpp','hgd_reverse.cpp','scc_cached_plus.cpp','scc_reverse_csr_plus.cpp',
             'scc_all.cpp','verified/hgd_optimized.hpp','verified/hgd_exact.cpp',
             'verified/paper_baselines.cpp','paper_dynamic_bench.cpp',
             'scripts/paper_experiments.py','scripts/paper_large_experiments.py']
    hashes={rel:digest(ROOT/rel) for rel in sources}
    stamp=OUT/'source_hashes.json'
    if stamp.exists():assert json.loads(stamp.read_text())==hashes,'source changed; use fresh results directory'
    else:stamp.write_text(json.dumps(hashes,indent=2)+'\n')
    for name,src in [('hgd_flow','hgd_flow.cpp'),('hgd_reverse','hgd_reverse.cpp'),
                     ('paper_baselines','verified/paper_baselines.cpp'),('dynamic','paper_dynamic_bench.cpp')]:
        binary=BUILD/name
        if not binary.exists():
            subprocess.run(['c++','-std=c++17','-O3','-DNDEBUG','-pthread',str(ROOT/src),'-o',str(binary)],check=True)
    env=dict(platform=platform.platform(),compiler=subprocess.check_output(['c++','--version'],text=True),
             flags='-std=c++17 -O3 -DNDEBUG -pthread',repeats=args.repeats,timeout_s=180,
             cpu='Apple M4 Pro',ram_bytes=25769803776,
             protocol='serial; fixed-seed shuffled methods; end-to-end wall time and wait4 peak RSS; full IDN output',
             pair_expansion_budget=200000000,
             pair_policy='neighbor/cocore not run if sum |e|(|e|-1) exceeds budget; not a timeout or OOM')
    (OUT/'environment.json').write_text(json.dumps(env,indent=2)+'\n')
    dsfile=OUT/'datasets.json';datasets=json.loads(dsfile.read_text()) if dsfile.exists() else {}
    for ds in args.datasets:
        name,n=SPECS[ds];path=ROOT/'dataset_large'/name
        if ds not in datasets:datasets[ds]=audit(path,n)
        assert digest(path)==datasets[ds]['sha256']
    dsfile.write_text(json.dumps(datasets,indent=2)+'\n')
    for ds in args.datasets:
        name,n=SPECS[ds];path=ROOT/'dataset_large'/name
        if args.phase in ['static','all']:
            for trial in range(args.repeats):
                methods=METHODS[:];random.Random(20261020+trial).shuffle(methods)
                for method in methods:
                    key=f'{ds}_{method}_{trial}'
                    if method in ['neighbor','cocore'] and datasets[ds]['ordered_pair_initializations']>env['pair_expansion_budget']:
                        record=dict(key=key,dataset=ds,method=method,trial=trial,status='not_run_pair_budget',
                                    input_sha256=datasets[ds]['sha256'])
                        (OUT/'runs'/f'{key}.json').write_text(json.dumps(record,indent=2)+'\n')
                        continue
                    if method.startswith('hgd_'):cmd=[str(BUILD/method),str(path),'5',str(n)]
                    else:cmd=[str(BUILD/'paper_baselines'),str(path),method,'5' if method in ['cocore','abcore'] else '1',str(n)]
                    record=engine.run(key,cmd,180)
                    record.update(dataset=ds,method=method,trial=trial,input_sha256=datasets[ds]['sha256'])
                    (OUT/'runs'/f'{key}.json').write_text(json.dumps(record,indent=2)+'\n')
            proposed=[json.loads((OUT/'runs'/f'{ds}_{method}_{trial}.json').read_text())
                      for method in METHODS[:2] for trial in range(args.repeats)]
            complete=[r for r in proposed if r['status']=='ok']
            if complete:
                assert len({r['output_sha256'] for r in complete})==1,(ds,'unequal IDNs')
                count=0
                with (OUT/'runs'/f'{complete[0]["key"]}.out').open() as f:
                    for count,line in enumerate(f,1):
                        vertex,score=map(int,line.split(','));assert vertex==count and score>=0
                assert count==n
        if args.phase in ['dynamic','all']:
            for operation in ['insert','delete']:
                for fraction in [.01,.05]:
                    for trial in range(args.repeats):
                        key=f'{ds}_{operation}_{fraction}_{trial}'
                        cmd=[str(BUILD/'dynamic'),str(path),'5',str(n),operation,str(fraction),str(721+trial)]
                        record=engine.run(key,cmd,180)
                        record.update(dataset=ds,operation=operation,fraction=fraction,trial=trial,
                                      input_sha256=datasets[ds]['sha256'])
                        if record['status']=='ok':
                            record.update(json.loads((OUT/'runs'/f'{key}.out').read_text()))
                            assert record['equal']
                        (OUT/'runs'/f'{key}.json').write_text(json.dumps(record,indent=2)+'\n')
    records=[json.loads(p.read_text()) for p in sorted((OUT/'runs').glob('*.json'))]
    static=[r for r in records if 'method' in r];dynamic=[r for r in records if 'operation' in r]
    (OUT/'measurements.json').write_text(json.dumps(static,indent=2)+'\n')
    (OUT/'dynamic.json').write_text(json.dumps(dynamic,indent=2)+'\n')
    with (OUT/'measurements.csv').open('w',newline='') as f:
        fields=['dataset','method','trial','status','wall_s','peak_rss_mib']
        writer=csv.DictWriter(f,fields,extrasaction='ignore');writer.writeheader();writer.writerows(static)
    print('PASS: all completed proposed outputs match; dynamic outputs checked against fresh recomputation')

if __name__=='__main__':main()
