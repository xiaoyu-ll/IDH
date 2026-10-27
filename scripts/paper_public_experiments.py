"""Independent public-data experiment namespace using the unchanged TC/SA protocol.

python scripts/paper_public_experiments.py --datasets TM DBLP --phase all
The imported runner supplies the identical six methods, delta=5, three launches,
180s deadlines, pair budget and complete-array comparisons. Never mixes old runs.
"""
import json
from pathlib import Path
import paper_large_experiments as protocol

ROOT=Path(__file__).resolve().parents[1]
protocol.OUT=ROOT/'paper_public_results'
protocol.BUILD=ROOT/'paper_public_build'
protocol.SPECS={'TM':('threads-math-sx.txt',176445),'DBLP':('coauth-dblp.txt',1930378)}

if __name__=='__main__':
    protocol.OUT.mkdir(exist_ok=True)
    sources=['scripts/paper_public_experiments.py','scripts/convert_public_hif.py']
    hashes={s:protocol.digest(ROOT/s) for s in sources}
    stamp=protocol.OUT/'public_source_hashes.json'
    if stamp.exists(): assert json.loads(stamp.read_text())==hashes,'public source changed'
    else: stamp.write_text(json.dumps(hashes,indent=2)+'\n')
    protocol.main()
