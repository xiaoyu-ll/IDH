"""Independent exhaustive checks of density profiles and closure optima."""
import itertools
import json
from pathlib import Path
import random
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[2]


def stats(edges,selected):
    kept=[e for e in edges if e<=selected]
    pairs=set()
    for e in kept:pairs.update(itertools.combinations(sorted(e),2))
    size=len(selected)
    return [len(kept)/size if size else 0,
            sum(map(len,kept))/size if size else 0,
            2*len(pairs)/size if size else 0]


def main():
    binary=Path(sys.argv[1]).resolve() if len(sys.argv)>1 else ROOT/'build/paper_density'
    rng=random.Random(20261021);checks=0
    with tempfile.TemporaryDirectory(prefix='density-oracle-') as temp:
        inp=Path(temp)/'edges';scores=Path(temp)/'scores'
        for trial in range(100):
            n=rng.randrange(1,8)
            options=[{v for v in range(n) if mask>>v&1} for mask in range(1,1<<n)]
            edges=rng.choices(options,k=rng.randrange(17))
            inp.write_text(''.join(','.join(str(v+1) for v in sorted(e))+'\n' for e in edges))
            labels=[rng.randrange(5) for _ in range(n)]
            scores.write_text(''.join(f'{v+1},{k}\n' for v,k in enumerate(labels)))
            def run(mode,arg):
                return json.loads(subprocess.check_output([str(binary),mode,str(inp),str(arg),str(n)],text=True))
            profile=run('profile',scores)
            expected=[0,0,0]
            for row,k in zip(profile['curve'],sorted(set(labels),reverse=True)):
                selected={v for v,x in enumerate(labels) if x>=k}
                values=stats(edges,selected)
                assert row['threshold']==k and row['vertices']==len(selected)
                assert [row['edges']/row['vertices'],row['weight']/row['vertices'],2*row['pairs']/row['vertices']]==values
                expected=[max(a,b) for a,b in zip(expected,values)]
            for i,best in enumerate(profile['best']):
                assert abs(best['density']-expected[i])<1e-10
                assert abs(stats(edges,{v-1 for v in best['selected']})[i]-best['density'])<1e-10
                checks+=1
            exhaustive=[max(stats(edges,s)[i] for s in options) for i in range(3)]
            for i,metric in enumerate(['ratio','degree']):
                result=run('exact',metric)
                assert result['optimality_certificate']
                assert abs(result['density']-exhaustive[i])<1e-10,(trial,metric,result,exhaustive)
                assert abs(stats(edges,{v-1 for v in result['selected']})[i]-result['density'])<1e-10
                checks+=1
            heuristic=run('peel','neighbor')
            assert not heuristic['optimality_certificate']
            actual=stats(edges,{v-1 for v in heuristic['selected']})[2]
            assert abs(actual-heuristic['density'])<1e-10 and actual<=exhaustive[2]+1e-10
            # Recompute the deterministic peeling sequence without incremental counters.
            selected=set(range(n));best=0.
            while selected:
                best=max(best,stats(edges,selected)[2])
                kept=[e for e in edges if e<=selected]
                v=min(selected,key=lambda x:(len(set().union(*(e for e in kept if x in e))-{x}),x))
                selected.remove(v)
            assert abs(best-actual)<1e-10
            checks+=1
    print(f'PASS: {checks} density/profile/optimality checks on 100 subset-enumerated hypergraphs')


if __name__=='__main__':main()
