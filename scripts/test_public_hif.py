"""Small deterministic converter tests, including declared isolates and duplicates."""
import json
from pathlib import Path
import tempfile
from convert_public_hif import convert

with tempfile.TemporaryDirectory() as d:
    source=Path(d)/'input.json'; target=Path(d)/'edges.txt'
    source.write_text(json.dumps({'network-type':'undirected',
        'nodes':[{'node':'isolated'}],
        'edges':[{'edge':'a'},{'edge':'b'},{'edge':'c'}],
        'incidences':[{'node':'x','edge':'a'},{'node':'x','edge':'a'},
                      {'node':'x','edge':'b'},{'node':'y','edge':'c'},
                      {'node':'x','edge':'c'}]}))
    stats=convert(source,target)
    assert target.read_text()=='2\n2\n2,3\n'
    assert (stats['n'],stats['m'],stats['L'],stats['active_vertices'],stats['removed_incidences'])==(3,3,4,2,1)
    first=target.read_bytes(); assert convert(source,target)==stats and target.read_bytes()==first
    source.write_text(json.dumps({'network-type':'directed'}))
    try: convert(source,target)
    except ValueError: pass
    else: raise AssertionError('directed input accepted')
    source.write_text(json.dumps({'network-type':'undirected','edges':[{'edge':'empty'}]}))
    try: convert(source,target)
    except ValueError: pass
    else: raise AssertionError('empty edge silently lost')
print('PASS: isolates, singleton and equal edges, duplicate incidence, mapping, determinism and invalid inputs')
