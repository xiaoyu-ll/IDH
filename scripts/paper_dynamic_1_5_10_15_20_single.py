"""Single-launch dynamic experiment for Figure 10 at 1/5/10/15/20 percent."""
import hashlib
import json
import platform
import subprocess
from pathlib import Path

import paper_experiments as engine

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'paper_dynamic_1_5_10_15_20_single_results'
BUILD=ROOT/'paper_dynamic_1_5_10_15_20_single_build'
SPECS={'SB':('senate-bills.txt',294,ROOT/'dataset'),
       'TC':('trivago.txt',172738,ROOT/'dataset_large')}
FRACTIONS=[.01,.05,.10,.15,.20]
OPERATIONS=['insert','delete']
SEED=20261019
TIMEOUT=180

def digest(path):
    h=hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda:f.read(1024*1024),b''):h.update(block)
    return h.hexdigest()

def main():
    OUT.mkdir(exist_ok=True);(OUT/'runs').mkdir(exist_ok=True);BUILD.mkdir(exist_ok=True)
    engine.OUT=OUT
    source=ROOT/'paper_dynamic_bench.cpp'
    header=ROOT/'verified/hgd_optimized.hpp'
    hashes={str(p.relative_to(ROOT)):digest(p) for p in [source,header,Path(__file__)]}
    stamp=OUT/'source_hashes.json'
    if stamp.exists():assert json.loads(stamp.read_text())==hashes,'source changed; use a fresh namespace'
    else:stamp.write_text(json.dumps(hashes,indent=2)+'\n')
    binary=BUILD/'paper_dynamic_bench'
    if not binary.exists():
        subprocess.run(['c++','-std=c++17','-O3','-DNDEBUG','-pthread',str(source),'-o',str(binary)],check=True)
    environment=dict(platform=platform.platform(),compiler=subprocess.check_output(['c++','--version'],text=True),
                     flags='-std=c++17 -O3 -DNDEBUG -pthread',repeats=1,timeout_s=TIMEOUT,
                     cpu='Apple M4 Pro',ram_bytes=25769803776,
                     protocol='serial; one launch per dataset/operation/fraction; end-to-end process measurement; full-array equality inside executable',
                     fractions=FRACTIONS,seed=SEED)
    (OUT/'environment.json').write_text(json.dumps(environment,indent=2)+'\n')
    rows=[]
    for ds,(filename,n,directory) in SPECS.items():
        path=directory/filename
        input_hash=digest(path)
        for operation in OPERATIONS:
            for fraction in FRACTIONS:
                key=f'{ds}_{operation}_{fraction:g}'
                row=engine.run(key,[str(binary),str(path),'5',str(n),operation,str(fraction),str(SEED)],TIMEOUT)
                assert row['status']=='ok',row
                measurement=json.loads((OUT/'runs'/f'{key}.out').read_text())
                assert measurement['equal']
                row.update(dataset=ds,operation=operation,fraction=fraction,trial=0,repeats=1,
                           seed=SEED,input_sha256=input_hash,**measurement)
                (OUT/'runs'/f'{key}.json').write_text(json.dumps(row,indent=2)+'\n')
                rows.append(row)
    assert len(rows)==20 and len({(r['dataset'],r['operation'],r['fraction']) for r in rows})==20
    (OUT/'measurements.json').write_text(json.dumps(rows,indent=2)+'\n')
    print('PASS: 20 single-launch Figure 10 conditions completed with equal outputs')

if __name__=='__main__':main()
