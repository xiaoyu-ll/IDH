"""Single-run ablation for the shared sandwich accelerator.

Every dataset/backend/accelerator condition is launched exactly once. The
120-second decomposition deadline is unchanged. Historical measurements and
source-hashed binaries are never relabeled or overwritten.
"""
import hashlib,json,os,platform,resource,signal,subprocess,time
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'paper_sandwich_single_v2_results'
BUILD=ROOT/'paper_sandwich_build'
SPECS={'SC':('senate-committees.txt',282),'HC':('house-committees.txt',1290),
       'CP':('contact-primary.txt',242),'CH':('contact-high.txt',327),
       'SB':('senate-bills.txt',294),'HB':('house-bills.txt',1494)}
TIMEOUT=120
def digest(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def write(p,x):
    p=Path(p);tmp=p.with_suffix(p.suffix+'.tmp');tmp.write_text(json.dumps(x,indent=2)+'\n');tmp.replace(p)
def build():
    OUT.mkdir(exist_ok=True);(OUT/'runs').mkdir(exist_ok=True);BUILD.mkdir(exist_ok=True)
    sources=['hgd_flow.cpp','hgd_reverse.cpp','verified/hgd_sandwich_cli.cpp',
             'verified/hgd_sandwich.hpp','verified/hgd_optimized.hpp','verified/hgd_exact.cpp',
             'scripts/paper_sandwich_single.py']
    hashes={x:digest(ROOT/x) for x in sources}
    stamp=OUT/'source_hashes.json'
    if stamp.exists():assert json.loads(stamp.read_text())==hashes,'source changed: use a fresh namespace'
    else:write(stamp,hashes)
    commands={
      'flow_base':['c++','-std=c++17','-O3','-DNDEBUG','-pthread',str(ROOT/'hgd_flow.cpp'),'-o',str(BUILD/'flow_base')],
      'reverse_base':['c++','-std=c++17','-O3','-DNDEBUG','-pthread',str(ROOT/'hgd_reverse.cpp'),'-o',str(BUILD/'reverse_base')],
      'sandwich':['c++','-std=c++17','-O3','-DNDEBUG','-pthread',str(ROOT/'verified/hgd_sandwich_cli.cpp'),'-o',str(BUILD/'sandwich')],
    }
    for command in commands.values():subprocess.run(command,check=True)
    binaries={p.name:digest(p) for p in BUILD.iterdir() if p.is_file()}
    write(OUT/'environment.json',dict(protocol='one serial launch per dataset/backend/accelerator condition; output checked',
      repeats=1,timeout_s=TIMEOUT,delta=5,flags='-std=c++17 -O3 -DNDEBUG -pthread',
      platform=platform.platform(),compiler=subprocess.check_output(['c++','--version'],text=True),binaries=binaries))
def launch(key,command):
    command=list(map(str,command))
    out=OUT/'runs'/f'{key}.out';err=OUT/'runs'/f'{key}.err';start=time.perf_counter()
    with out.open('wb') as so,err.open('wb') as se:
        p=subprocess.Popen(command,stdout=so,stderr=se,start_new_session=True)
        try:
            _,status,usage=os.wait4(p.pid,0) if TIMEOUT is None else wait(p,TIMEOUT)
            code=os.waitstatus_to_exitcode(status);p.returncode=code
            state='ok' if code==0 else 'error'
        except TimeoutError:
            os.killpg(p.pid,signal.SIGKILL);_,status,usage=os.wait4(p.pid,0);code=-9;state='timeout'
    row=dict(key=key,command=command,status=state,wall_s=time.perf_counter()-start,
             peak_rss_mib=usage.ru_maxrss/(1024**2 if platform.system()=='Darwin' else 1024),
             timeout_s=TIMEOUT,returncode=code,output_sha256=digest(out) if state=='ok' else '')
    if state=='error':raise RuntimeError(err.read_text())
    return row
def wait(p,timeout):
    import threading
    expired=threading.Event();timer=threading.Timer(timeout,expired.set);timer.start()
    try:
        while True:
            pid,status,usage=os.wait4(p.pid,os.WNOHANG)
            if pid:return pid,status,usage
            if expired.is_set():raise TimeoutError
            time.sleep(.01)
    finally:timer.cancel()
def main():
    build();rows=[]
    for ds,(filename,n) in SPECS.items():
      source=ROOT/'dataset'/filename;input_hash=digest(source);reference=None
      for backend in ['flow','reverse']:
       for accelerated in [False,True]:
        tag=backend+('_sandwich' if accelerated else '_base');key=f'{ds}_{tag}'
        command=([BUILD/'sandwich',backend,source,'5',n] if accelerated else [BUILD/(backend+'_base'),source,'5',n])
        row=launch(key,command);row.update(dataset=ds,backend=backend,accelerated=accelerated,repeats=1,input_sha256=input_hash)
        if row['status']=='ok':
            if reference is None:reference=row['output_sha256']
            row['equal']=row['output_sha256']==reference
            if accelerated:
                meta=json.loads((OUT/'runs'/f'{key}.err').read_text().strip().splitlines()[-1]);row.update(meta)
        rows.append(row);write(OUT/'measurements.json',rows)
        print(key,row['status'],f'{row["wall_s"]:.3f}s',flush=True)
      completed=[r for r in rows if r['dataset']==ds and r['status']=='ok']
      assert completed and all(r['equal'] for r in completed)
    assert len(rows)==24 and all(r['status'] in {'ok','timeout'} for r in rows)
    write(OUT/'summary.json',{'conditions':24,'repeats':1,'all_completed_outputs_equal':all(r.get('equal',True) for r in rows)})
    print('PASS: 24 single-run conditions recorded in fresh namespace')
if __name__=='__main__':main()
