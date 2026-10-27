"""Lossless incidence-to-edge conversion of pinned, undirected XGI HIF releases.

Requires ijson (tested 3.6.0). Retains equal edge occurrences and singleton edges,
and declared isolated nodes. Maps labels in deterministic encounter order, recording
both mappings. Only repeated incidences within one edge are removed.
"""
import argparse
from array import array
import hashlib
import json
from pathlib import Path
import ijson

ROOT = Path(__file__).resolve().parents[1]
SPECS = {
    'TM': ('threads-math-sx', '21938817', '756dc8548abfc3333a4fab2cb8fb16be'),
    'DBLP': ('coauth-dblp', '21905305', '40c82ca508836dc484863c4af6224bd8'),
}

def digest(path, algorithm='sha256'):
    h = hashlib.new(algorithm)
    with path.open('rb') as f:
        for b in iter(lambda: f.read(1024 * 1024), b''): h.update(b)
    return h.hexdigest()

def items(path, key):
    with path.open('rb') as f:
        yield from ijson.items(f, key)

def convert(source, target):
    if list(items(source, 'network-type')) != ['undirected']:
        raise ValueError('only explicitly undirected HIF is supported')
    nodes = {}; edge_index = {}; edges = []
    def node(label):
        if label not in nodes: nodes[label] = len(nodes) + 1
        return nodes[label]
    def edge(label):
        if label not in edge_index:
            edge_index[label] = len(edges); edges.append(array('I'))
        return edges[edge_index[label]]
    for record in items(source, 'nodes.item'): node(record['node'])
    declared = len(nodes)
    for record in items(source, 'edges.item'):
        if record['edge'] in edge_index: raise ValueError('duplicate edge identifier')
        edge(record['edge'])
    for record in items(source, 'incidences.item'):
        edge(record['edge']).append(node(record['node']))
    if any(not e for e in edges):
        raise ValueError('empty edge cannot be represented by the text loader')
    L = removed = pairs = 0; rank = 0; small = 2**63; active = bytearray(len(nodes))
    with target.open('w') as f:
        for raw in edges:
            normalized = sorted(set(raw)); size = len(normalized)
            f.write(','.join(map(str, normalized)) + '\n')
            L += size; removed += len(raw) - size; pairs += size * (size - 1)
            rank = max(rank, size); small = min(small, size)
            for v in normalized: active[v - 1] = 1
    for suffix, mapping in [('nodes', nodes), ('edges', edge_index)]:
        with target.with_suffix('.' + suffix + '.jsonl').open('w') as f:
            for label, index in mapping.items():
                f.write(json.dumps([label, index if suffix == 'nodes' else index + 1]) + '\n')
    return dict(n=len(nodes), m=len(edges), L=L, max_size=rank, min_size=small,
                declared_nodes=declared, active_vertices=sum(active),
                removed_incidences=removed, ordered_pair_initializations=pairs,
                sha256=digest(target))

def main():
    p = argparse.ArgumentParser(); p.add_argument('dataset', choices=SPECS)
    args = p.parse_args(); name, record, md5 = SPECS[args.dataset]
    base = ROOT / 'dataset_large'; source = base / 'downloads' / (name + '.json')
    if digest(source, 'md5') != md5: raise ValueError('raw MD5 does not match pinned release')
    target = base / (name + '.txt'); stats = convert(source, target)
    stats.update(dataset=args.dataset, raw_md5=md5, raw_sha256=digest(source),
                 source_url=f'https://zenodo.org/records/{record}/files/{name}.json?download=1',
                 doi='10.5281/zenodo.' + record, converter_sha256=digest(Path(__file__)),
                 mappings={s: digest(target.with_suffix('.' + s + '.jsonl')) for s in ['nodes', 'edges']})
    target.with_suffix('.provenance.json').write_text(json.dumps(stats, indent=2) + '\n')
    print(json.dumps(stats, indent=2), flush=True)

if __name__ == '__main__': main()
