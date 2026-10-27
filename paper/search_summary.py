"""Never turn a short cutoff or an unfinished trial into a 12-hour TO."""
import statistics

DEADLINE=43200

def single_cell(group):
    assert len(group)==1,'one trial only; do not aggregate repeats'
    row=group[0]
    if row['status']=='ok':
        assert row.get('equal')
        return f'{row["wall_s"]:.3f}'
    if row['status']=='timeout':
        assert row['timeout_s']==DEADLINE and row['wall_s']>=DEADLINE
        return 'TO'
    if row['status']=='error':return 'Error'
    assert row['status'] in ['pending','running','interrupted'],row['status']
    return 'Pending'

def cell(group):
    assert len(group)==3
    for row in group:
        if row['status']=='timeout':
            assert row['timeout_s']==DEADLINE and row['wall_s']>=DEADLINE
    statuses={r['status'] for r in group}
    if statuses=={'ok'}:
        assert all(r.get('equal') for r in group)
        return f'{statistics.median(r["wall_s"] for r in group):.3f}'
    if 'error' in statuses:return 'Error'
    if statuses&{'pending','running'}:return 'Pending'
    if statuses=={'timeout'}:return 'TO'
    assert statuses=={'ok','timeout'},statuses
    return f'TO ({sum(r["status"]=="timeout" for r in group)}/3)'
