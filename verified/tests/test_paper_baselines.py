"""Subset-enumerated independent oracles for every comparison model."""
import itertools
from pathlib import Path
import random
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[2]
MODELS=['core','neighbor','cocore','abcore']


def oracle(edges,n,mode,param):
    scores=[0]*n
    for mask in range(1,1<<n):
        s={v for v in range(n) if mask>>v&1}
        induced=([e&s for e in edges if len(e&s)>=param] if mode=='abcore'
                 else [e for e in edges if e<=s])
        degrees=[sum(v in e for e in induced) for v in s]
        neighbors=[len(set().union(*(e for e in induced if v in e))-{v}) for v in s]
        if mode=='cocore' and min(neighbors)<param:continue
        level=min(neighbors if mode=='neighbor' else degrees)
        for v in s:scores[v]=max(scores[v],level)
    return scores


def main():
    binary=Path(sys.argv[1]).resolve() if len(sys.argv)>1 else ROOT/'build/paper_baselines'
    rng=random.Random(20261020);checks=0
    with tempfile.TemporaryDirectory(prefix='paper-baseline-oracle-') as td:
        path=Path(td)/'edges.txt'
        for trial in range(160):
            n=rng.randrange(1,8)
            candidates=[{v for v in range(n) if mask>>v&1} for mask in range(1,1<<n)]
            edges=rng.choices(candidates,k=rng.randrange(18))
            path.write_text(''.join(','.join(str(v+1) for v in sorted(e))+'\n' for e in edges))
            for mode in MODELS:
                for param in [1,2,5]:
                    r=subprocess.run([str(binary),str(path),mode,str(param),str(n)],check=True,text=True,capture_output=True)
                    actual=[int(l.split(',')[1]) for l in r.stdout.splitlines()]
                    assert actual==oracle(edges,n,mode,param),(trial,mode,param,edges,actual)
                    checks+=1
        # Within-edge duplicates are removed; duplicate edge records remain.
        path.write_text('1,1,2\n1,2\n')
        r=subprocess.run([str(binary),str(path),'core','1','3'],check=True,text=True,capture_output=True)
        assert r.stdout=='1,2\n2,2\n3,0\n'
    print(f'PASS: {checks} baseline arrays against independent subset enumeration; duplicate incidences/records; isolates and empty input')


if __name__=='__main__':main()
