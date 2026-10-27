#!/usr/bin/env python3
"""Stress exactness after *each* successive max-flow with arbitrary thresholds.
Oracle: independent Python min-cost-flow canonical IDNs; repeated hyperedges,
repeated incidences, isolates, singletons, k=0/large, nonmonotone thresholds.
"""
from pathlib import Path
import itertools,random,subprocess,sys,tempfile
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'verified'))
from hgd_verified_reference import compute
rng=random.Random(62517)
checked=0
with tempfile.TemporaryDirectory() as td:
  path=Path(td)/'graph.csv'
  for trial in range(70):
    n=trial%7
    options=[list(s) for r in range(1,n+1) for s in itertools.combinations(range(n),r)]
    edges=[]
    for j in range(rng.randrange(0,8)):
      if not options:break
      e=list(rng.choice(options))
      if rng.random()<.30:e.append(rng.choice(e))
      edges.append(e)
    path.write_text(''.join(','.join(str(x+1) for x in edge)+'\n' for edge in edges))
    for delta in (1,2,4):
      ref=compute(n,[tuple(sorted(set(e))) for e in edges],delta)['idn']
      choices=[0,1,2,max(ref,default=0),max(ref,default=0)+1,
               max(ref,default=0)+3]
      seq=choices[:]+[rng.choice(choices) for _ in range(9)]
      rng.shuffle(seq)
      binary=sys.argv[1] if len(sys.argv)>1 else 'threshold_sequence_test'
      cmd=[str(ROOT/'build'/binary),str(path),str(delta),str(n)]
      cmd +=list(map(str,seq))
      p=subprocess.run(cmd,text=True,capture_output=True,timeout=12)
      if p.returncode:raise AssertionError((trial,delta,p.stderr))
      lines=p.stdout.splitlines()
      assert len(lines)==len(seq)
      for k,line in zip(seq,lines):
        prefix,items=line.split(':')
        assert int(prefix)==k
        got=[int(v)-1 for v in items.split(',') if v]
        expected=[v for v,score in enumerate(ref) if score>=k]
        assert got==expected,(trial,delta,k,edges,got,expected)
        checked+=1
print('PASS',checked,'independent nonmonotone reusable-flow threshold certificates')
