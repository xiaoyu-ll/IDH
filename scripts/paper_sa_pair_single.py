"""Run the two previously excluded SA pair-based decompositions once each.

The 180-second deadline is the unchanged large-decomposition protocol. Results
live in a fresh namespace and never replace the historical budget exclusions.
"""
import hashlib
import json
import os
import platform
import signal
import subprocess
import threading
import time
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'paper_sa_pair_single_v2_results'
BUILD=ROOT/'paper_sa_pair_single_v2_build'
SOURCE=ROOT/'verified/paper_baselines.cpp'
HEADER=ROOT/'verified/hgd_optimized.hpp'
INPUT=ROOT/'dataset_large/stackoverflow-answers.txt'
N=15211989
DEADLINE=180

def digest(path):
    h=hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda:f.read(1024*1024),b''):h.update(block)
    return h.hexdigest()

def run(method):
    key=f'SA_{method}_single'
    record_path=OUT/'runs'/f'{key}.json'
    if record_path.exists():
        return json.loads(record_path.read_text())
    stdout_path=record_path.with_suffix('.out')
    stderr_path=record_path.with_suffix('.err')
    command=[str(BUILD/'paper_baselines'),str(INPUT),method,
             '5' if method=='cocore' else '1',str(N)]
    start=time.perf_counter()
    with stdout_path.open('wb') as stdout, stderr_path.open('wb') as stderr:
        process=subprocess.Popen(command,cwd=ROOT,stdout=stdout,stderr=stderr,
                                 start_new_session=True)
        expired=threading.Event()
        def stop():
            expired.set()
            try:os.killpg(process.pid,signal.SIGKILL)
            except ProcessLookupError:pass
        timer=threading.Timer(DEADLINE,stop);timer.start()
        try:
            _,wait_status,usage=os.wait4(process.pid,0)
            returncode=os.waitstatus_to_exitcode(wait_status)
            process.returncode=returncode
        finally:
            timer.cancel()
    wall_s=time.perf_counter()-start
    stderr_text=stderr_path.read_text(errors='replace')
    if expired.is_set():status='timeout'
    elif returncode==0:status='ok'
    elif 'bad_alloc' in stderr_text or returncode in {-6,-9}:status='oom'
    else:status='error'
    record=dict(key=key,dataset='SA',method=method,trial=0,repeats=1,
                command=command,status=status,wall_s=wall_s,returncode=returncode,
                timeout_s=DEADLINE,
                peak_rss_mib=usage.ru_maxrss/(1024**2 if platform.system()=='Darwin' else 1024),
                input_sha256=digest(INPUT),output_sha256=digest(stdout_path) if status=='ok' else '',
                stderr=stderr_text[-1000:])
    record_path.write_text(json.dumps(record,indent=2)+'\n')
    print(key,status,f'{wall_s:.3f}s',f'{record["peak_rss_mib"]:.1f} MiB',flush=True)
    return record

def main():
    OUT.mkdir(exist_ok=True);(OUT/'runs').mkdir(exist_ok=True);BUILD.mkdir(exist_ok=True)
    source_hashes={str(p.relative_to(ROOT)):digest(p) for p in [SOURCE,HEADER,Path(__file__)]}
    stamp=OUT/'source_hashes.json'
    if stamp.exists():
        assert json.loads(stamp.read_text())==source_hashes,'source changed; use a fresh namespace'
    else:stamp.write_text(json.dumps(source_hashes,indent=2)+'\n')
    binary=BUILD/'paper_baselines'
    if not binary.exists():
        subprocess.run(['c++','-std=c++17','-O3','-DNDEBUG','-pthread',str(SOURCE),'-o',str(binary)],check=True)
    environment=dict(platform=platform.platform(),compiler=subprocess.check_output(['c++','--version'],text=True),
                     flags='-std=c++17 -O3 -DNDEBUG -pthread',repeats=1,timeout_s=DEADLINE,
                     cpu='Apple M4 Pro',ram_bytes=25769803776,
                     protocol='serial; one launch per method; end-to-end wall time; full score output')
    (OUT/'environment.json').write_text(json.dumps(environment,indent=2)+'\n')
    rows=[run(method) for method in ['neighbor','cocore']]
    (OUT/'measurements.json').write_text(json.dumps(rows,indent=2)+'\n')
    print('PASS: both previously excluded SA conditions received one actual launch')

if __name__=='__main__':main()
