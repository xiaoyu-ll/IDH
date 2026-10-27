"""Resume pinned public HIF downloads and verify publisher MD5 before conversion."""
import argparse
from pathlib import Path
import subprocess
from convert_public_hif import SPECS, digest

ROOT=Path(__file__).resolve().parents[1]
SIZES={'TM':175340256,'DBLP':1139118099}

def main():
    p=argparse.ArgumentParser();p.add_argument('datasets',nargs='*',choices=SPECS,default=list(SPECS))
    args=p.parse_args();folder=ROOT/'dataset_large/downloads';folder.mkdir(exist_ok=True,parents=True)
    for ds in args.datasets:
        name,record,md5=SPECS[ds];target=folder/(name+'.json')
        for attempt in range(12):
            size=target.stat().st_size if target.exists() else 0
            if size==SIZES[ds]: break
            if size>SIZES[ds]: raise ValueError('oversized file; preserve and investigate before retrying')
            print(ds,'attempt',attempt+1,'resuming at',size,flush=True)
            result=subprocess.run(['curl','-sS','-L','--fail','--connect-timeout','10','--max-time','600',
                '-C','-',f'https://zenodo.org/records/{record}/files/{name}.json?download=1','-o',str(target)])
            if result.returncode not in [0,28]: raise RuntimeError('download failed; partial file preserved')
        if not target.exists() or target.stat().st_size!=SIZES[ds] or digest(target,'md5')!=md5:
            raise ValueError('incomplete or incorrect raw input; partial file preserved')
        print(ds,'publisher MD5 verified',md5,flush=True)

if __name__=='__main__':main()
