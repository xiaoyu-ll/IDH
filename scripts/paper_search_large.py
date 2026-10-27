"""Fill Table II on all four large inputs using the original search protocol.

Serial end-to-end launches, k=delta=5, three trials, 15-second deadlines.
Every completed output must equal the threshold of the released full IDNs.
Results are resumable but source/input/reference changes are rejected.
"""
import hashlib
import json
from pathlib import Path
import platform
import random
import subprocess

import paper_experiments as engine

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'paper_search_large_results'
BUILD=ROOT/'paper_search_large_build'
SPECS={'TC':('trivago.txt',172738,'paper_large_results'),
       'SA':('stackoverflow-answers.txt',15211989,'paper_large_results'),
       'TM':('threads-math-sx.txt',176445,'paper_public_results'),
       'DBLP':('coauth-dblp.txt',1930378,'paper_public_results')}
METHODS=['path','flow','flow+']

def digest(path):
    h=hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda:f.read(1024*1024),b''):h.update(block)
    return h.hexdigest()

def frozen(name,value):
    path=OUT/name
    if path.exists():assert json.loads(path.read_text())==value,(name,'changed; use a fresh result directory')
    else:path.write_text(json.dumps(value,indent=2)+'\n')

def main():
    OUT.mkdir(exist_ok=True);(OUT/'runs').mkdir(exist_ok=True);BUILD.mkdir(exist_ok=True)
    engine.OUT=OUT
    sources=['path.cpp','flow.cpp','flow+.cpp','verified/hgd_optimized.hpp',
             'verified/hgd_optimized_cli.hpp','scripts/paper_experiments.py','scripts/paper_search_large.py']
    frozen('source_hashes.json',{s:digest(ROOT/s) for s in sources})
    for method in METHODS:
        if not (BUILD/method).exists():
            subprocess.run(['c++','-std=c++17','-O3','-DNDEBUG','-pthread',str(ROOT/f'{method}.cpp'),'-o',str(BUILD/method)],check=True)
    frozen('binary_hashes.json',{m:digest(BUILD/m) for m in METHODS})
    frozen('environment.json',dict(platform=platform.platform(),compiler=subprocess.check_output(['c++','--version'],text=True),
        flags='-std=c++17 -O3 -DNDEBUG -pthread',repeats=3,k=5,delta=5,timeout_s=15,
        protocol='serial end-to-end wall time including startup, loading, computation, output; warm filesystem cache; fixed-seed shuffled methods; no CPU affinity'))
    references={}
    for ds,(filename,n,folder) in SPECS.items():
        data=json.loads((ROOT/folder/'datasets.json').read_text())[ds]
        assert digest(ROOT/'dataset_large'/filename)==data['sha256']
        reference=ROOT/folder/'runs'/f'{ds}_hgd_reverse_0.out'
        record=json.loads(reference.with_suffix('.json').read_text())
        reference_hash=digest(reference)
        assert record['status']=='ok' and reference_hash==record['output_sha256']
        h=hashlib.sha256();count=0
        with reference.open() as f:
            for count,line in enumerate(f,1):
                vertex,score=map(int,line.split(','));assert vertex==count and score>=0
                if score>=5:h.update(f'{vertex}\n'.encode())
        assert count==n
        references[ds]=dict(n=n,input_sha256=data['sha256'],reference=str(reference.relative_to(ROOT)),
            reference_sha256=reference_hash,expected_query_sha256=h.hexdigest())
    frozen('references.json',references)
    rows=[]
    for ds,(filename,n,folder) in SPECS.items():
        for trial in range(3):
            methods=METHODS.copy();random.Random(20261020+trial).shuffle(methods)
            for method in methods:
                key=f'query_{ds}_{method}_{trial}'
                row=engine.run(key,[str(BUILD/method),str(ROOT/'dataset_large'/filename),'5','5',str(n)],15)
                row.update(kind='query',dataset=ds,method=method,trial=trial,k=5,delta=5,
                           input_sha256=references[ds]['input_sha256'])
                assert row['status'] in ['ok','timeout'],row
                if row['status']=='ok':
                    assert digest(OUT/'runs'/f'{key}.out')==row['output_sha256']==references[ds]['expected_query_sha256'],key
                    row['equal']=True
                (OUT/'runs'/f'{key}.json').write_text(json.dumps(row,indent=2)+'\n')
                rows.append(row)
                (OUT/'measurements.json').write_text(json.dumps(rows,indent=2)+'\n')
    print('PASS: 36 large-data search trials; every completed query matches full reference IDNs',flush=True)

if __name__=='__main__':main()
