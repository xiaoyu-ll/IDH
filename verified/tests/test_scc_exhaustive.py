#!/usr/bin/env python3
"""Small exhaustive head-set enumeration independent of the min-cost-flow oracle.
Checks uniqueness of IDNs across all egalitarian orientations and equality to
the two published decomposition backends. Intentionally tiny finite domains.
"""
from pathlib import Path
from itertools import combinations, product
import random, subprocess, tempfile
ROOT=Path(__file__).resolve().parents[2]
RNG=random.Random(91733)

def enum_scores(n,edges,delta):
    capacity=[min(delta,len(e)) for e in edges]
    choices=[list(combinations(e,r)) for e,r in zip(edges,capacity)]
    assert all(choices)
    scores=[]
    for selected in product(*choices):
        indeg=[0]*n
        arc=[[] for _ in range(n)]
        for e,head in zip(edges,selected):
            hs=set(head)
            for v in hs:indeg[v]+=1
            for u in e:
                if u not in hs:
                    for v in hs:arc[u].append(v)
        reach=[]
        for src in range(n):
            seen={src}; st=[src]
            while st:
                for nxt in arc[st.pop()]:
                    if nxt not in seen:seen.add(nxt);st.append(nxt)
            reach.append(max(indeg[v] for v in seen))
        if all(reach[v]<=indeg[v]+1 for v in range(n)):
            scores.append(tuple(reach))
    assert scores, 'no egalitarian orientation'
    assert len(set(scores))==1, (n,edges,delta,scores[:10])
    return scores[0],len(scores)

def call(exe,path,delta,n):
    p=subprocess.run([str(ROOT/'build'/exe),str(path),str(delta),str(n)],capture_output=True,text=True,timeout=10)
    assert p.returncode==0,(exe,p.stderr)
    return tuple(int(line.rsplit(',',1)[1]) for line in p.stdout.splitlines())

def main():
    checked=0; orientations=0
    with tempfile.TemporaryDirectory() as td:
        data=Path(td)/'graph.txt'
        for trial in range(100):
            n=1+trial%4
            opts=[e for r in range(1,min(n,3)+1) for e in combinations(range(n),r)]
            edges=[RNG.choice(opts) for _ in range(trial%5)]
            data.write_text(''.join(','.join(str(x+1) for x in e)+'\n' for e in edges))
            for delta in [1,2,3]:
                expected,number=enum_scores(n,edges,delta)
                orientations+=number
                for program in ['hgd_flow','hgd_reverse']:
                    got=call(program,data,delta,n)
                    assert got==expected,(trial,program,n,edges,delta,got,expected)
                    checked+=1
    print(f'PASS {checked} independent C++/exhaustive IDN comparisons; {orientations} egalitarian orientations')

if __name__=='__main__': main()
