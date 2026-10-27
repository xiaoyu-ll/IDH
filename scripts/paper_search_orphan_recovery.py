"""Recover one live orphan without restarting or repeating its benchmark.

Darwin's kernel supplies the existing process start in mach-absolute time.
EVFILT_PROC supplies its real exit status even though it is no longer our child.
The restored deadline uses process lifetime, not calendar time or CPU time.
Kernel lifetime excludes the unavailable Python Popen overhead, documented in
the result. No fabricated exit status or elapsed duration is used.
"""
import argparse
from contextlib import closing
import ctypes
import fcntl
import hashlib
import json
import os
from pathlib import Path
import select
import signal
import subprocess
import sys
import time

import paper_search_single_12h as original

ROOT=original.ROOT
OUT=original.OUT
AUDIT=ROOT/'paper_search_single_recovery'
NOTE_EXITSTATUS=0x04000000
RUSAGE_FIELDS='''user_time system_time pkg_idle_wkups interrupt_wkups pageins
wired_size resident_size phys_footprint proc_start_abstime proc_exit_abstime
child_user_time child_system_time child_pkg_idle_wkups child_interrupt_wkups
child_pageins child_elapsed_abstime diskio_bytesread diskio_byteswritten
cpu_time_qos_default cpu_time_qos_maintenance cpu_time_qos_background
cpu_time_qos_utility cpu_time_qos_legacy cpu_time_qos_user_initiated
cpu_time_qos_user_interactive billed_system_time serviced_system_time
logical_writes lifetime_max_phys_footprint instructions cycles billed_energy
serviced_energy interval_max_phys_footprint runnable_time'''.split()

class Usage(ctypes.Structure):
    _fields_=[('uuid',ctypes.c_uint8*16)]+[(s,ctypes.c_uint64) for s in RUSAGE_FIELDS]

class Timebase(ctypes.Structure):
    _fields_=[('numer',ctypes.c_uint32),('denom',ctypes.c_uint32)]

def kernel_usage(pid):
    lib=ctypes.CDLL('/usr/lib/libproc.dylib',use_errno=True)
    result=Usage()
    if lib.proc_pid_rusage(pid,4,ctypes.byref(result))!=0:
        raise OSError(ctypes.get_errno(),'proc_pid_rusage failed',pid)
    return result

def seconds(ticks):
    base=Timebase();lib=ctypes.CDLL('/usr/lib/libSystem.B.dylib')
    assert lib.mach_timebase_info(ctypes.byref(base))==0 and base.denom>0
    return ticks*base.numer/base.denom/1e9

def register(pid):
    queue=select.kqueue()
    event=select.kevent(pid,filter=select.KQ_FILTER_PROC,
        flags=select.KQ_EV_ADD|select.KQ_EV_ENABLE|select.KQ_EV_ONESHOT,
        fflags=select.KQ_NOTE_EXIT|NOTE_EXITSTATUS)
    queue.control([event],0,0)
    return queue

def selftest():
    p=subprocess.Popen([sys.executable,'-c','import time;time.sleep(.2);raise SystemExit(7)'])
    usage=kernel_usage(p.pid)
    elapsed=time.perf_counter()-seconds(usage.proc_start_abstime)
    assert 0<=elapsed<10,elapsed
    with closing(register(p.pid)) as queue:
        events=queue.control(None,1,5)
    assert len(events)==1 and events[0].fflags&NOTE_EXITSTATUS
    assert os.waitstatus_to_exitcode(events[0].data)==7
    assert p.wait()==7
    print('PASS: kernel monotonic lifetime and actual kqueue exit status',flush=True)
    # Also prove NOTE_EXITSTATUS works for an orphan, not merely our own child.
    launcher='import subprocess,sys; p=subprocess.Popen([sys.executable,"-c","import time;time.sleep(.5);raise SystemExit(9)"],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,start_new_session=True); print(p.pid)'
    orphan=int(subprocess.check_output([sys.executable,'-c',launcher],text=True))
    with closing(register(orphan)) as queue:
        events=queue.control(None,1,5)
    assert len(events)==1 and events[0].fflags&NOTE_EXITSTATUS
    assert os.waitstatus_to_exitcode(events[0].data)==9
    print('PASS: orphan exit status observed without restarting or becoming its parent',flush=True)

def audit_write(name,value):
    p=AUDIT/name;tmp=p.with_suffix(p.suffix+'.tmp')
    tmp.write_text(json.dumps(value,indent=2)+'\n');tmp.replace(p)

def recover(pid):
    AUDIT.mkdir(exist_ok=True)
    with (OUT/'.runner.lock').open('a') as lock:
        fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        rows=json.loads((OUT/'measurements.json').read_text())
        active=[r for r in rows if r['status']=='running']
        assert len(active)==1
        row=active[0]
        try:os.kill(row['runner_pid'],0)
        except ProcessLookupError:pass
        else:raise RuntimeError('Original runner is still alive; do not attach a second coordinator')
        command=subprocess.check_output(['/bin/ps','-p',str(pid),'-o','command='],text=True).strip()
        assert command==' '.join(row['command']),(command,row['command'])
        assert os.getpgid(pid)==pid,'unexpected process group'
        for method,h in json.loads((ROOT/'paper_search_large_results/binary_hashes.json').read_text()).items():
            assert original.digest(original.BUILD/method)==h
        usage=kernel_usage(pid)
        start_ticks=usage.proc_start_abstime
        start=seconds(start_ticks)
        assert start_ticks>0 and 0<time.perf_counter()-start<original.TIMEOUT
        assert not (OUT/'runs'/f'{row["key"]}.json').exists()
        metadata=dict(key=row['key'],child_pid=pid,previous_runner_pid=row['runner_pid'],
            recovery_pid=os.getpid(),attached_at=time.time(),kernel_start_abstime=start_ticks,
            kernel_elapsed_s_at_attach=time.perf_counter()-start,timeout_s=original.TIMEOUT,
            source_sha256=original.digest(Path(__file__)),
            timing_note='Recovered kernel-process lifetime on mach_absolute_time; excludes original Python Popen overhead; observed exit via kqueue NOTE_EXITSTATUS; not restarted')
        # Arm the kernel exit event before updating the ledger; no polling or
        # child wait-status inference is needed after the original parent died.
        with closing(register(pid)) as queue:
            audit_write('attachment.json',metadata)
            row.update(previous_runner_pid=row['runner_pid'],runner_pid=os.getpid(),
                recovered_child_pid=pid,recovery_audit='paper_search_single_recovery/attachment.json')
            original.write('measurements.json',rows)
            print('Attached without restart:',row['key'],pid,'elapsed',metadata['kernel_elapsed_s_at_attach'],flush=True)
            expired=False
            while True:
                remaining=original.TIMEOUT-(time.perf_counter()-start)
                if remaining<=0:
                    # Protect against PID reuse before the one allowed cutoff.
                    current=kernel_usage(pid)
                    assert current.proc_start_abstime==start_ticks
                    os.killpg(pid,signal.SIGKILL);expired=True
                    events=queue.control(None,1,60)
                    assert events,'timeout kill has no observed exit event'
                else:events=queue.control(None,1,min(remaining,60))
                if events:break
            finish=time.perf_counter()
            event=events[0]
            assert event.ident==pid and event.fflags&NOTE_EXITSTATUS
            code=os.waitstatus_to_exitcode(event.data)
            elapsed=finish-start
            status='timeout' if expired else ('ok' if code==0 else 'error')
            output=OUT/'runs'/f'{row["key"]}.out'
            if status=='ok':
                output_hash=original.digest(output)
                assert output_hash==row['expected_query_sha256']
            else:output_hash=''
            if expired:assert elapsed>=original.TIMEOUT and code==-signal.SIGKILL
            result=dict(key=row['key'],command=row['command'],status=status,wall_s=elapsed,
                returncode=code,timeout_s=original.TIMEOUT,output_sha256=output_hash,
                peak_rss_mib=None,recovered_process=True,timing_note=metadata['timing_note'],
                kernel_start_abstime=start_ticks,exit_status_source='kqueue NOTE_EXITSTATUS')
            original.write(f'runs/{row["key"]}.json',result)
            row.update(result,origin=OUT.name,output_relative=str(output.relative_to(ROOT)))
            if status=='ok':row['equal']=True
            original.write('measurements.json',rows)
            audit_write('outcome.json',result)
            print('Recovered outcome:',status,elapsed,code,flush=True)
            if status=='error':raise RuntimeError('Orphan benchmark failed; no TO assigned')
    # Only the still-unstarted SA condition can now be launched by the unchanged
    # original single-run runner. Resolved conditions will never be repeated.
    original.main()

if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--self-test',action='store_true')
    parser.add_argument('--pid',type=int)
    args=parser.parse_args()
    if args.self_test:selftest()
    else:
        assert args.pid is not None
        recover(args.pid)
