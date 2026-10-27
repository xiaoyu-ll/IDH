"""Independent exact-integer verifier; imports no solver code.

Each nonblank, noncomment input line is an edge. Repeated vertices within
an edge are deduplicated; repeated edge lines remain separate edges.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re


def require(condition, message):
    if not condition:
        raise ValueError(message)


def verify(data, cert, expected_delta):
    edges = []
    for line in data.decode().splitlines():
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        tokens = [t for t in re.split(r'[,\s]+', line) if t]
        require(tokens and all(re.fullmatch(r'\+?[0-9]+', t) for t in tokens),
                'invalid input vertex')
        edge = set(map(int, tokens))
        require(min(edge) > 0 and max(edge) <= 2147483647, 'vertex out of range')
        edges.append(edge)
    delta, k = cert['delta'], cert['k']
    require(type(delta) is int and delta == expected_delta and delta > 0,
            'delta mismatch')
    require(type(k) is int and k >= 0, 'invalid k')
    require(len(cert['heads']) == len(edges), 'edge count mismatch')
    degrees = Counter()
    for index, (edge, heads) in enumerate(zip(edges, cert['heads'])):
        require(isinstance(heads, list) and all(type(v) is int for v in heads),
                f'invalid heads at edge {index}')
        require(len(set(heads)) == len(heads) == min(delta, len(edge)),
                f'head budget/uniqueness at edge {index}')
        require(set(heads) <= edge, f'head outside edge {index}')
        degrees.update(heads)
    upper = max(degrees.values(), default=0)
    require(upper <= k, 'orientation exceeds claimed bound')
    witness = cert['witness']
    require(isinstance(witness, list) and all(type(v) is int for v in witness),
            'invalid witness')
    x = set(witness)
    vertices = set().union(*edges) if edges else set()
    require(len(x) == len(witness) and x <= vertices, 'invalid witness vertices')
    mandatory = sum(max(0, min(delta, len(e)) - len(e - x)) for e in edges)
    surplus = mandatory - (k - 1) * len(x)
    require((k == 0 and not edges) or (x and surplus > 0),
            'witness does not prove lower bound')
    return dict(verified=True, delta=delta, kmax=k, vertices=len(vertices),
                edges=len(edges), incidences=sum(map(len, edges)),
                witness_size=len(x), mandatory_heads=mandatory,
                lower_bound_surplus=surplus, orientation_max=upper,
                dataset_sha256=hashlib.sha256(data).hexdigest())


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path)
    parser.add_argument('certificate', type=Path)
    parser.add_argument('delta', type=int)
    args = parser.parse_args()
    try:
        print(json.dumps(verify(args.input.read_bytes(),
                                json.loads(args.certificate.read_text()), args.delta),
                         indent=2))
    except (ValueError, KeyError, TypeError, OSError) as exc:
        parser.exit(1, f'INVALID: {exc}\n')
