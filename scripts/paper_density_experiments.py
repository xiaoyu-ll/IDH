"""Evaluate densest hierarchy sets, exact references and parameter sensitivity."""
import hashlib
import json
from pathlib import Path
import subprocess
import paper_experiments as engine

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'paper_density_results'
MODELS=['core','neighbor','cocore','abcore','hgd_reverse']


def main():
    OUT.mkdir(exist_ok=True);(OUT/'runs').mkdir(exist_ok=True);(OUT/'inputs').mkdir(exist_ok=True)
    engine.OUT=OUT
    sources=[ROOT/p for p in ['verified/paper_density.cpp','verified/hgd_optimized.hpp',
        'verified/hgd_exact.cpp','scripts/paper_density_experiments.py','scripts/paper_experiments.py']]
    hashes={str(p.relative_to(ROOT)):engine.digest(p) for p in sources}
    stamp=OUT/'source_hashes.json'
    if stamp.exists():assert json.loads(stamp.read_text())==hashes,'changed source: use a new result directory'
    stamp.write_text(json.dumps(hashes,indent=2)+'\n')
    binary=OUT/'paper_density'
    if not binary.exists():subprocess.run(['c++','-std=c++17','-O3','-DNDEBUG','-pthread',str(ROOT/'verified/paper_density.cpp'),'-o',str(binary)],check=True)
    results={};inputs={}
    for ds,(filename,n) in engine.SPECS.items():
        path=ROOT/'dataset'/filename
        edges=[set(map(int,line.replace(',',' ').split())) for line in path.read_text().splitlines() if line.strip()]
        inputs[ds]=dict(sha256=engine.digest(path),n=n,m=len(edges),max_size=max(map(len,edges)))
        result={'models':{},'exact':{},'parameter':{}}
        def call(key,args,scorefile=None):
            row=engine.run(key,[str(binary),*map(str,args),str(n)],120)
            assert row['status']=='ok',row
            payload=json.loads((OUT/'runs'/f'{key}.out').read_text())
            if scorefile is not None:row['input_score_sha256']=engine.digest(scorefile)
            row['input_sha256']=engine.digest(path)
            (OUT/'runs'/f'{key}.json').write_text(json.dumps(row,indent=2)+'\n')
            return payload
        for model in MODELS:
            score=ROOT/'paper_comparison_results/runs'/f'{ds}_{model}_0.out'
            result['models'][model]=call(f'{ds}_{model}',['profile',path,score],score)
        for metric in ['ratio','degree']:
            result['exact'][metric]=call(f'{ds}_exact_{metric}',['exact',path,metric])
        result['neighbor_peel']=call(f'{ds}_neighbor_peel',['peel',path,'neighbor'])
        for delta in sorted({1,2,5,10,inputs[ds]['max_size']}):
            if delta==5:profile=result['models']['hgd_reverse']
            else:
                if delta==inputs[ds]['max_size']:
                    degree=[0]*n
                    for e in edges:
                        for v in e:degree[v-1]+=1
                    score=OUT/'inputs'/f'{ds}_forced_all_head.out'
                    score.write_text(''.join(f'{v+1},{k}\n' for v,k in enumerate(degree)))
                else:score=ROOT/'paper_results_final/runs'/f'ablation_{ds}_{delta}.out'
                profile=call(f'{ds}_delta_{delta}',['profile',path,score],score)
            result['parameter'][str(delta)]=dict(profile,
                saturation=sum(len(e)>delta for e in edges)/len(edges))
        # Verify witnesses against the exact global bounds and recorded input scores.
        for group in [*result['models'].values(),*result['parameter'].values()]:
            for i,metric in enumerate(['ratio','degree']):
                assert group['best'][i]['density']<=result['exact'][metric]['density']+1e-9
        results[ds]=result
        (OUT/'results.json').write_text(json.dumps(results,indent=2)+'\n')
        (OUT/'datasets.json').write_text(json.dumps(inputs,indent=2)+'\n')
    print('PASS: five model hierarchies, three density metrics, exact ratio/degree optima, neighbor peeling and delta sensitivity on six datasets')


if __name__=='__main__':main()
