"""Single-run Table II with a real 12-hour deadline and automatic final export.

Each of the 30 dataset/method conditions uses trial 0, never a median. Verified
completed first trials are retained; short cutoffs and interrupted runs restart.
One benchmark runs at a time. Only after all conditions resolve are the paper
and complete release regenerated, checked and packaged.
"""
import fcntl
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time

import paper_experiments as engine
from paper_search_large import digest

ROOT=Path(__file__).resolve().parents[1]
WORKSPACE=ROOT.parents[1]
OUT=ROOT/'paper_search_single_results'
BUILD=ROOT/'paper_search_large_build'
TIMEOUT=43200

def write(name,value):
    p=OUT/name;tmp=p.with_suffix(p.suffix+'.tmp')
    tmp.write_text(json.dumps(value,indent=2)+'\n');tmp.replace(p)

def prepare():
    sources=['path.cpp','flow.cpp','flow+.cpp','verified/hgd_optimized.hpp',
             'verified/hgd_optimized_cli.hpp','scripts/paper_experiments.py',
             'scripts/paper_search_large.py','scripts/paper_search_single_12h.py']
    hashes={s:digest(ROOT/s) for s in sources}
    stamp=OUT/'source_hashes.json'
    if stamp.exists():assert json.loads(stamp.read_text())==hashes,'source changed'
    else:write('source_hashes.json',hashes)
    if (OUT/'measurements.json').exists():return json.loads((OUT/'measurements.json').read_text())
    previous=json.loads((ROOT/'paper_search_12h_results/measurements.json').read_text())
    assert json.loads((ROOT/'paper_search_large_results/binary_hashes.json').read_text())=={m:digest(BUILD/m) for m in ['path','flow','flow+']}
    refs=json.loads((ROOT/'paper_search_12h_results/references.json').read_text())
    for ds,ref in refs.items():
        source=next(r for r in previous if r['dataset']==ds)
        assert digest(Path(source['command'][1]))==ref['input_sha256']
        assert digest(ROOT/ref['reference'])==ref['reference_sha256']
    write('references.json',refs)
    write('environment.json',dict(timeout_s=TIMEOUT,repeats=1,k=5,delta=5,
        policy='one actual first-trial time per dataset/method; completed first trials retained; no medians; short cutoffs and interruptions rerun',
        flags='-std=c++17 -O3 -DNDEBUG -pthread',compiler=subprocess.check_output(['c++','--version'],text=True)))
    rows=[]
    for source in previous:
        if source['trial']!=0:continue
        row={k:source[k] for k in ['key','dataset','method','kind','k','delta','command',
            'input_sha256','expected_query_sha256']}
        row.update(trial=0,repeats=1,effective_timeout_s=TIMEOUT)
        if source['status']=='ok':
            assert digest(ROOT/source['output_relative'])==source['output_sha256']==row['expected_query_sha256']
            row.update({k:source[k] for k in ['wall_s','output_sha256','equal','origin','output_relative']})
            row['status']='ok'
        else:row.update(status='pending',superseded_status=source['status'])
        rows.append(row)
    assert len(rows)==30
    write('measurements.json',rows);return rows

def publish(rows):
    if any(r['status'] not in ['ok','timeout'] for r in rows):return
    python=sys.executable
    for script in ['test_search_deadline.py']:
        subprocess.run([python,str(WORKSPACE/'TKDE/scripts'/script)],check=True)
    subprocess.run([python,str(WORKSPACE/'TKDE/scripts/render_compact.py'),'--tables-only'],check=True)
    subprocess.run([python,str(WORKSPACE/'TKDE/scripts/test_compact_release.py')],check=True)
    latex=shutil.which('latexmk') or '/Library/TeX/texbin/latexmk'
    subprocess.run([latex,'-pdf','-interaction=nonstopmode','-halt-on-error','hgd_tkde.tex'],cwd=WORKSPACE/'TKDE',check=True)
    # The bundle is generated only after all measurements and LaTeX finish.
    target=WORKSPACE/'delivery/hgd_tkde_table2_single_12h_20261020.pdf'
    shutil.copy2(WORKSPACE/'TKDE/hgd_tkde.pdf',target)
    subprocess.run([python,str(WORKSPACE/'package_delivery.py')],check=True)
    write('publication.json',dict(status='built_pending_visual_review',pdf=str(target),finished_at=time.time()))

def main():
    OUT.mkdir(exist_ok=True);(OUT/'runs').mkdir(exist_ok=True)
    with (OUT/'.runner.lock').open('w') as lock:
        fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        rows=prepare();engine.OUT=OUT
        order={'TC':0,'TM':1,'DBLP':2,'SB':3,'HB':4,'SA':5}
        for row in sorted(rows,key=lambda r:order.get(r['dataset'],-1)):
            if row['status'] in ['ok','timeout']:continue
            row.update(status='running',started_at=time.time(),runner_pid=os.getpid())
            write('measurements.json',rows)
            try:result=engine.run(row['key'],row['command'],TIMEOUT)
            except BaseException as error:
                row.update(status='interrupted' if isinstance(error,KeyboardInterrupt) else 'error',
                           failure_type=type(error).__name__,finished_at=time.time())
                write('measurements.json',rows);raise
            if result['status']=='ok':
                assert digest(OUT/'runs'/f'{row["key"]}.out')==result['output_sha256']==row['expected_query_sha256']
                row['equal']=True
            else:assert result['status']=='timeout' and result['wall_s']>=TIMEOUT and result['timeout_s']==TIMEOUT
            row.update(result);row['origin']=OUT.name
            row['output_relative']=str((OUT/'runs'/f'{row["key"]}.out').relative_to(ROOT))
            write('measurements.json',rows)
        publish(rows)
        print('PASS: 30 single-run conditions resolved; no repeated trials or medians',flush=True)

if __name__=='__main__':main()
