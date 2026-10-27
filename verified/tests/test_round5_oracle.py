#!/usr/bin/env python3
"""Independent Python min-cost-flow IDN oracle for published decomposition entrypoints.
Does not call the C++ SCC routine or the C++ threshold-flow implementation.
"""
import itertools, os, random, subprocess, tempfile, sys
from pathlib import Path
root=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(root/'verified'))
from hgd_verified_reference import compute
rng=random.Random(20261019)
cases=int(sys.argv[1]) if len(sys.argv)>1 else 22
checks=0
with tempfile.TemporaryDirectory() as t:
    tmp=Path(t)
    for c in range(cases):
        n=c%9
        subsets=[p for z in range(1,n+1) for p in itertools.combinations(range(n),z)]
        edges=[]
        for i in range(rng.randrange(0,13)):
            if not subsets:break
            e=list(rng.choice(subsets))
            if rng.randrange(3)==0:e+=e[:1]  # duplicate within one hyperedge
            edges.append(e)
        f=tmp/'graph.csv'
        f.write_text(''.join(','.join(str(v+1) for v in e)+'\n' for e in edges))
        for d in (1,2,3,5,n+2):
            expected=compute(n,[tuple(sorted(set(e))) for e in edges],d)['idn']
            for exe in ('hgd_flow','hgd_reverse'):
                p=subprocess.run([str(root/'build'/exe),str(f),str(d),str(n)],capture_output=True,text=True,timeout=20)
                if p.returncode:raise AssertionError((exe,c,d,p.stderr))
                got=[int(s.split(',')[1]) for s in p.stdout.splitlines() if s.strip()]
                assert got==expected,(c,d,exe,edges,got,expected)
                checks+=1
        if n:
            batch=tmp/'events.csv'
            inserted=list(rng.choice(subsets))
            events=['+ '+','.join(str(v+1) for v in inserted)]
            states=[edges+[inserted]]
            if edges:
                events.append('- 0');states.append(edges[1:]+[inserted])
            batch.write_text('\n'.join(events)+'\n')
            p=subprocess.run([str(root/'build'/'hgd_stream_reverse'),str(f),'2',str(batch),str(n)],capture_output=True,text=True,timeout=30)
            if p.returncode:raise AssertionError(('stream',c,p.stderr))
            rows=p.stdout.strip().splitlines()
            assert len(rows)==len(states)*n,(c,rows,states)
            for t,es in enumerate(states):
                got=[int(row.split(',')[3]) for row in rows[t*n:(t+1)*n]]
                want=compute(n,[tuple(sorted(set(e))) for e in es],2)['idn']
                assert got==want,('stream',c,t,got,want)
                checks+=1
    print('PASS independent Python oracle:',checks,'output checks over',cases,'random cases')
