"""Sequential, resumable, checksum-checked paper experiments on supplied data.

Fresh build in paper_build prevents accidentally benchmarking stale -O2 binaries.
Each subprocess has a process-group deadline. Timeouts are censored, never times.
"""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import platform
import random
import re
import signal
import subprocess
import time
import threading

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT/'paper_results_final'
SPECS = {'SC':('senate-committees.txt',282), 'HC':('house-committees.txt',1290),
         'CP':('contact-primary.txt',242), 'CH':('contact-high.txt',327),
         'SB':('senate-bills.txt',294), 'HB':('house-bills.txt',1494)}
METHODS = ['path','dsm_all','flow','flow+','dd','dd+','scc_cached_plus',
           'scc_reverse_csr_plus','paper_dynamic_bench']


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(key, command, timeout=15):
    record = OUT/'runs'/f'{key}.json'
    if record.exists():
        return json.loads(record.read_text())
    output = record.with_suffix('.out')
    error = record.with_suffix('.err')
    start = time.perf_counter()
    with output.open('wb') as stdout, error.open('wb') as stderr:
        p = subprocess.Popen(command, cwd=ROOT, stdout=stdout, stderr=stderr, start_new_session=True)
        expired=threading.Event()
        def stop():
            expired.set()
            try:os.killpg(p.pid,signal.SIGKILL)
            except ProcessLookupError:pass
        timer=threading.Timer(timeout,stop);timer.start()
        try:
            _,wait_status,usage=os.wait4(p.pid,0)
            code=os.waitstatus_to_exitcode(wait_status);p.returncode=code
            status='timeout' if expired.is_set() else ('ok' if code==0 else 'error')
        except BaseException:
            stop();p.wait();raise
        finally:timer.cancel()
    result = dict(key=key,command=list(map(str,command)),status=status,
                  wall_s=time.perf_counter()-start,returncode=code,timeout_s=timeout,
                  peak_rss_mib=usage.ru_maxrss/(1024**2 if platform.system()=='Darwin' else 1024),
                  output_sha256=digest(output) if status=='ok' else '')
    record.write_text(json.dumps(result,indent=2)+'\n')
    print(key,status,f'{result["wall_s"]:.3f}s',flush=True)
    if status=='error':raise RuntimeError(f'{key}: {error.read_text()}')
    return result


def output_of(row):
    return (OUT/'runs'/f'{row["key"]}.out').read_text()


def ids_of(row,n):
    rows=[tuple(map(int,l.split(','))) for l in output_of(row).splitlines()]
    assert [v for v,k in rows]==list(range(1,n+1)),row['key']
    assert all(k>=0 for v,k in rows)
    return [k for v,k in rows]


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repeats',type=int,default=3)
    args=parser.parse_args()
    OUT.mkdir(exist_ok=True);(OUT/'runs').mkdir(exist_ok=True);(OUT/'inputs').mkdir(exist_ok=True)
    build=ROOT/'paper_build';build.mkdir(exist_ok=True)
    source_files=sorted(list(ROOT.glob('*.cpp'))+list((ROOT/'verified').glob('*.hpp'))+
                        list((ROOT/'verified').glob('*.cpp'))+[Path(__file__)])
    hashes={str(p.relative_to(ROOT)):digest(p) for p in source_files}
    stamp=OUT/'source_hashes.json'
    if stamp.exists() and json.loads(stamp.read_text())!=hashes:
        raise RuntimeError('Source changed: use a new result directory, never mix runs.')
    stamp.write_text(json.dumps(hashes,indent=2)+'\n')
    compiler=subprocess.check_output(['c++','--version'],text=True)
    for method in METHODS:
        executable=build/method
        if not executable.exists():
            subprocess.run(['c++','-std=c++17','-O3','-DNDEBUG','-pthread',str(ROOT/f'{method}.cpp'),
                            '-o',str(executable)],check=True)
    env=dict(platform=platform.platform(),machine=platform.machine(),compiler=compiler,
             flags='-std=c++17 -O3 -DNDEBUG -pthread',repeats=args.repeats,
             cpu='Apple M4 Pro',ram_bytes=25769803776,logical_cpus=12,
             protocol='serial subprocesses; startup, loading, computation and output included; warm filesystem cache; no CPU affinity',
             started=time.strftime('%Y-%m-%d %H:%M:%S %z'))
    (OUT/'environment.json').write_text(json.dumps(env,indent=2)+'\n')
    graphs={};stats={};all_rows=[]
    for ds,(name,n) in SPECS.items():
        path=ROOT/'dataset'/name
        raw=[[int(v) for v in re.split(r'[,\s]+',l.strip()) if v] for l in path.read_text().splitlines()
             if l.strip() and not l.lstrip().startswith('#')]
        edges=[sorted(set(e)) for e in raw];graphs[ds]=edges
        stats[ds]=dict(n=n,m=len(edges),L=sum(map(len,edges)),max_size=max(map(len,edges)),
                       min_size=min(map(len,edges)),mean_size=sum(map(len,edges))/len(edges),
                       removed_incidences=sum(map(len,raw))-sum(map(len,edges)),sha256=digest(path))
    (OUT/'datasets.json').write_text(json.dumps(stats,indent=2)+'\n')
    references={}
    # Canonical full-data outputs are also the oracle for single-threshold calls.
    for ds,(name,n) in SPECS.items():
        ref=run(f'reference_{ds}_d5',[str(build/'scc_reverse_csr_plus'),str(ROOT/'dataset'/name),'5',str(n)],120)
        assert ref['status']=='ok'
        references[ds]=ids_of(ref,n)
    for ds,(name,n) in SPECS.items():
        path=ROOT/'dataset'/name
        for trial in range(args.repeats):
            jobs=[('query',m) for m in ['path','dsm_all','flow','flow+']]+[
                 ('all',m) for m in ['dd','dd+','scc_cached_plus','scc_reverse_csr_plus']]
            random.Random(1000+trial).shuffle(jobs)
            for kind,method in jobs:
                cmd=[str(build/method),str(path),'5']+(['5'] if kind=='query' else [])+[str(n)]
                row=run(f'{kind}_{ds}_{method}_{trial}',cmd,15)
                row.update(kind=kind,dataset=ds,method=method,trial=trial,delta=5)
                if row['status']=='ok':
                    if kind=='all':assert ids_of(row,n)==references[ds],row['key']
                    else:assert [int(v) for v in output_of(row).split()]==[
                        i+1 for i,k in enumerate(references[ds]) if k>=5],row['key']
                all_rows.append(row)
    # Parameter variation: exact deterministic layer counts, not timing claims.
    ablation=[]
    for ds,(name,n) in SPECS.items():
        for delta in [1,2,5,10]:
            row=run(f'ablation_{ds}_{delta}',[str(build/'scc_reverse_csr_plus'),str(ROOT/'dataset'/name),str(delta),str(n)],120)
            assert row['status']=='ok'
            ids=ids_of(row,n)
            ablation.append(dict(dataset=ds,delta=delta,kmax=max(ids),layers=len(set(ids)),
                                 top_size=ids.count(max(ids)),sha256=row['output_sha256']))
    (OUT/'ablation.json').write_text(json.dumps(ablation,indent=2)+'\n')
    # Nested, fixed-seed edge samples on HB; vertex universe is held fixed.
    ds='HB';n=SPECS[ds][1];edges=graphs[ds].copy();random.Random(20261019).shuffle(edges)
    for fraction in [.1,.25,.5,.75,1.]:
        sample=OUT/'inputs'/f'HB_{fraction}.txt';subset=edges[:int(len(edges)*fraction)]
        sample.write_text(''.join(','.join(map(str,e))+'\n' for e in subset))
        expected=None
        for trial in range(args.repeats):
            methods=['scc_cached_plus','scc_reverse_csr_plus'];random.Random(trial).shuffle(methods)
            for method in methods:
                row=run(f'scale_HB_{fraction}_{method}_{trial}',[str(build/method),str(sample),'5',str(n)],60)
                row.update(kind='scale',dataset='HB',fraction=fraction,m=len(subset),method=method,trial=trial,delta=5)
                if row['status']=='ok':
                    ids=ids_of(row,n)
                    if expected is None:expected=ids
                    assert ids==expected
                all_rows.append(row)
    # The seed fixes update occurrences across repeats; repeats measure timing noise.
    dynamic=[]
    for ds in ['SC','HC','SB','HB']:
        name,n=SPECS[ds]
        for operation in ['insert','delete']:
            for fraction in [.01,.05,.1]:
                for trial in range(args.repeats):
                    row=run(f'dynamic_{ds}_{operation}_{fraction}_{trial}',
                            [str(build/'paper_dynamic_bench'),str(ROOT/'dataset'/name),'5',str(n),operation,str(fraction),'20261019'],120)
                    assert row['status']=='ok'
                    measurement=json.loads(output_of(row));assert measurement['equal']
                    dynamic.append(dict(dataset=ds,operation=operation,fraction=fraction,trial=trial,**measurement))
    (OUT/'dynamic.json').write_text(json.dumps(dynamic,indent=2)+'\n')
    (OUT/'measurements.json').write_text(json.dumps(all_rows,indent=2)+'\n')
    fields=['kind','dataset','method','trial','delta','fraction','m','status','wall_s','peak_rss_mib','timeout_s','output_sha256']
    with (OUT/'measurements.csv').open('w') as f:
        w=csv.DictWriter(f,fieldnames=fields,extrasaction='ignore');w.writeheader();w.writerows(all_rows)
    print('PAPER EXPERIMENTS COMPLETE: all successful outputs checked',flush=True)


if __name__=='__main__':main()
