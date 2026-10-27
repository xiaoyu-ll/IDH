from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='hgd-dynamic-') as directory:
    subprocess.run([str(ROOT/'build/test_dynamic_boundaries'),
                    str(Path(directory)/'batch.txt')], check=True, timeout=30)
    directory = Path(directory)
    graph = directory/'base.txt'
    graph.write_text('# occurrence IDs skip comments\n1,2\n\n2,3\n4,5\n1,2\n')
    events = directory/'events.txt'
    updates = [('delete', 1), ('insert', {3, 7}), ('insert', {3, 7}),
               ('insert', {8}), ('delete', 0), ('delete', 2), ('delete', 3),
               ('delete', 4), ('delete', 5), ('delete', 6),
               ('insert', {1, 8}), ('delete', 7)]
    events.write_text('# split/new vertices/repeated occurrences/delete all\n' + ''.join(
        f'- {value}\n' if kind == 'delete' else
        '+ ' + ','.join(map(str, sorted(value))) + '\n' for kind, value in updates))
    for delta in [1, 2, 9]:
        edges = [{1, 2}, {2, 3}, {4, 5}, {1, 2}]
        n = 6
        expected = []
        for step, (kind, value) in enumerate(updates, 1):
            if kind == 'delete':
                edges[value] = None
            else:
                edges.append(value)
                n = max(n, max(value))
            active = [e for e in edges if e is not None]
            scores = [0] * n
            for k in range(1, len(active) + 1):
                best, common = None, set(range(1, n + 1))
                for mask in range(1 << n):
                    x = {v + 1 for v in range(n) if mask & (1 << v)}
                    q = sum(max(0, min(delta, len(e)) - len(e - x))
                            for e in active) - (k - 1) * len(x)
                    if best is None or q > best:
                        best, common = q, x
                    elif q == best:
                        common &= x
                for v in common:
                    scores[v - 1] = k
            op = '-' if kind == 'delete' else '+'
            expected.extend(f'{step},{op},{v},{score}' for v, score in enumerate(scores, 1))
        for program in ['hgd_stream', 'hgd_stream_reverse']:
            result = subprocess.run([str(ROOT/'build'/program), str(graph), str(delta),
                                     str(events), '6'], capture_output=True, text=True,
                                    timeout=30)
            assert result.returncode == 0, result.stderr
            assert result.stdout.splitlines() == expected, program
print('PASS: 72 stream update snapshots against independent subset oracle')
