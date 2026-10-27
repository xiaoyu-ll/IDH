#!/usr/bin/env python3
"""Independent exact-oracle tests for recovered optimization paths.
Run after: make core
Python oracle uses min-cost flow, NOT the C++ threshold flow algorithm.
Also exercises sequential stream updates with a fresh oracle after each event.
"""
from pathlib import Path
import sys,random,subprocess,tempfile,os,json,itertools
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'verified'))
from hgd_verified_reference import compute
rng=random.Random(20261019)
NCASES=int(sys.argv[1]) if len(sys.argv)>1 else 50
count=0
stages={'static_all':0,'threshold_query':0,'insertion':0,'deletion':0,'stream':0,'multi_delta':0}

def oracle(n,edges,d):return compute(n,[tuple(sorted(set(e))) for e in edges],d)['idn']
def data(edges):return ''.join(','.join(str(v+1) for v in e)+'\n' for e in edges)
def checked_run(exe,*args):
    p=subprocess.run([str(ROOT/'build'/exe),*map(str,args)],capture_output=True,text=True,timeout=12)
    if p.returncode:
        raise AssertionError(('program failure',exe,args,p.stderr))
    return p.stdout

def parse_all(output):
    return [int(x.split(',')[1]) for x in output.strip().splitlines() if x.strip()]
def parse_query(output):
    return [int(x)-1 for x in output.splitlines() if x.strip()]

with tempfile.TemporaryDirectory() as tmp:
    tmp=Path(tmp)
    for z in range(NCASES):
        n=z%7
        possible=[list(subset) for r in range(1,n+1) for subset in itertools.combinations(range(n),r)]
        es=[]
        for j in range(rng.randint(0,8)):
            if not possible:break
            e=list(rng.choice(possible))
            if rng.random()<0.30:e.insert(rng.randrange(len(e)+1),rng.choice(e))
            es.append(e)
        f=tmp/'base.csv';f.write_text(data(es))
        for delta in [1,2,4]:
            ref=oracle(n,es,delta)
            for mode in ['hgd_flow','hgd_reverse']:
                got=parse_all(checked_run(mode,f,delta,n))
                assert got==ref,(z,delta,mode,es,got,ref)
                stages['static_all']+=1
            for k in list(range(0,max(ref,default=0)+3)):
                answer=[v for v in range(n) if ref[v]>=k]
                for mode in ['path','flow','flow+']:
                    found=parse_query(checked_run(mode,f,delta,k,n))
                    assert found==answer,(z,delta,k,mode,es,found,answer)
                    stages['threshold_query']+=1
            additions=[]
            if n:
                for j in range(2):
                    e=list(rng.choice(possible))
                    if rng.random()<.5:e.insert(0,e[0])
                    additions.append(e)
                ins=tmp/'insert.csv';ins.write_text(data(additions))
                refadd=oracle(n,es+additions,delta)
                for mode in ['insert']:
                    got=parse_all(checked_run(mode,f,delta,ins,n))
                    assert got==refadd,(z,delta,mode,es,got,refadd)
                    stages['insertion']+=1
            deleted=[]
            if es:
                deleted=sorted(rng.sample(range(len(es)),min(2,len(es))))
                rem=tmp/'delete.csv';rem.write_text(''.join(str(i)+'\n' for i in deleted))
                remaining=[e for i,e in enumerate(es) if i not in deleted]
                refdel=oracle(n,remaining,delta)
                for mode in ['delete']:
                    got=parse_all(checked_run(mode,f,delta,rem,n))
                    assert got==refdel,(z,delta,mode,es,got,refdel)
                    stages['deletion']+=1
            if n:
                # Stream inserts an edge and deletes original edge 0 if present.
                # Then deletes appended edge using its stable hyperedge id.
                events=['+ '+','.join(str(v+1) for v in additions[0])]
                states=[es+additions[:1]]
                if es:
                    events.append('- 0')
                    states.append(es[1:]+additions[:1])
                events.append('- '+str(len(es)))
                states.append(es[1:] if es else [])
                ev=tmp/'stream.csv';ev.write_text('\n'.join(events)+'\n')
                lines=checked_run('hgd_stream',f,delta,ev,n).splitlines()
                assert len(lines)==len(events)*n,(z,'row count',len(lines),len(events)*n)
                for step,edges_after in enumerate(states):
                    got=[int(s.split(',')[3]) for s in lines[step*n:(step+1)*n]]
                    desired=oracle(n,edges_after,delta)
                    assert got==desired,(z,delta,step,events,got,desired)
                    stages['stream']+=1
        if (z+1)%10==0:
            print('PROGRESS',z+1,stages,flush=True)
    print('PASS',json.dumps({'cases':NCASES,'checks':sum(stages.values()),'by_stage':stages},sort_keys=True),flush=True)
