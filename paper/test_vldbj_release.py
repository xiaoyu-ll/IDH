"""Read-only consistency and layout gates for the VLDBJ migration."""
from pathlib import Path
import hashlib,json,re,subprocess
from pypdf import PdfReader
V=Path(__file__).resolve().parent
R=V.parent
s=(V/'hgd_vldbj.tex').read_text()
log=(V/'hgd_vldbj.log').read_text()
assert r'\documentclass[twocolumn]{svjour3}' in s
assert r'Hongchao Qin\textsuperscript{*}\thanks{\textsuperscript{*}Corresponding author.}' in s
assert r'\section*{Statements and Declarations}' in s
assert r'\stitle{Funding.}' in s
assert r'\stitle{Competing interests.}' in s
assert '2025ZD0123403' in s and '62572055' in s and '62372342' in s
assert not any(x in s for x in ['IEEEbiography','IEEEkeywords','IEEEtran',r'\vskip -'])
assert s.count(r'\begin{table}')==3
assert s.count(r'\begin{algorithm}')==8
assert r'\begin{figure*}' not in s and r'\begin{table*}' not in s
assert 'INSERT-EXACT' not in s and 'DELETE-EXACT' not in s
assert 'fig:sandwich-ablation' not in s and 'compact_sandwich.pdf' not in s
assert len(PdfReader(V/'hgd_vldbj.pdf').pages)<=25
assert not any(x in log for x in ['undefined','Overfull','! LaTeX Error'])
for name,h in json.loads((V/'template_provenance.json').read_text())['unmodified_vendor_sha256'].items():
    assert hashlib.sha256((V/name).read_bytes()).hexdigest()==h
body=s[s.index(r'\begin{document}'):]
assert not re.search(r'\bSC\b',body)
assert r'\kw{HC} & 1,290 & 341 & 11,843 & 81' in s
assert r'\kw{DBLP} & 1,930,378 & 3,700,681 & 10,326,773 & 280' in s
assert 'HC & 2.073 & 0.005 & 0.005' in s and r'\kw{DBLP} & TO & 6.581 & 6.500' in s
new=json.loads((V/'plot_points.json').read_text())
variants={'hgd_flow_sandwich','hgd_reverse_sandwich'}
assert len(new['points'])==321 and len([p for p in new['points'] if p['method'] in variants])==52
for figure in ['compact_small_time','compact_small_memory']:
    assert new['legends'][figure]==['Flow','Flow-S','Reverse','Reverse-S']
    assert {p['dataset'] for p in new['points'] if p['figure']==figure}=={'HC','CP','CH','SB','HB','TC','SA','TM','DBLP'}
assert len([p for p in new['points'] if p['figure']=='compact_small_memory' and p['dataset']=='DBLP'])==4
assert len([p for p in new['points'] if p['dataset']=='DBLP' and p['method']=='hgd_flow' and
            p['status']=='ok' and p['replicates']==1])==4
assert not {'compact_large_time','compact_large_memory'} & set(new['figures'])
large_points=[p for p in new['points'] if p['figure']=='compact_large_methods']
assert len(large_points)==32
assert {p['dataset'] for p in large_points}=={'TC','SA','TM','DBLP'}
assert new['legends']['compact_large_methods']==[
    'Degree','Neighbor','CoCore','BiCore','Flow','Flow-S','Reverse','Reverse-S']
assert {(p['dataset'],p['method'],p['status']) for p in large_points if p['status']!='ok'}=={
    ('SA','neighbor','oom'),('SA','cocore','oom')}
assert all(sum(p['dataset']==ds and p['method']==method for p in large_points)==1
           for ds in {'TC','SA','TM','DBLP'}
           for method in {'hgd_flow','hgd_flow_sandwich','hgd_reverse','hgd_reverse_sandwich'})
large_memory=[p for p in new['points'] if p['figure']=='compact_large_methods_memory']
assert len(large_memory)==32 and {p['dataset'] for p in large_memory}=={'TC','SA','TM','DBLP'}
assert new['legends']['compact_large_methods_memory']==new['legends']['compact_large_methods']
assert {(p['dataset'],p['method'],p['status']) for p in large_memory if p['status']!='ok'}=={
    ('SA','neighbor','oom'),('SA','cocore','oom')}
assert len([p for p in new['points'] if p['figure']=='compact_hierarchy_quality'])==75
assert len([p for p in new['points'] if p['figure']=='compact_scalability'])==40
assert len([p for p in new['points'] if p['figure']=='compact_search_scalability'])==30
assert new['legends']['compact_search_scalability']==['DSM-PATH','DSM-FLOW','DSM-FLOW+']
assert {p['dataset'] for p in new['points'] if p['figure']=='compact_dynamic_selected'}=={'SB','TC'}
assert new['legends']['compact_dynamic_selected']==[
    'Insertion (inc.)','Insertion (full)','Deletion (inc.)','Deletion (full)']
dynamic_points=[p for p in new['points'] if p['figure']=='compact_dynamic_selected']
assert len(dynamic_points)==40 and all(p['replicates']==1 for p in dynamic_points)
assert {p['condition'] for p in dynamic_points}=={.01,.05,.10,.15,.20}
assert {p['condition'] for p in new['points'] if p['figure']=='compact_hierarchy_quality'}=={'total','nonempty','consistency'}
assert new['legends']['compact_hierarchy_quality'][-1]==r'$(k,\delta)$-dense'
assert 'compact_density' not in new['figures']
for f in new['figures']:
    p=PdfReader(V/(f+'.pdf')).pages[0]
    assert abs(float(p.mediabox.width)-3.30*72)<.01
    expected_height={'compact_hierarchy_quality':2.20,'compact_large_methods':1.85,
                     'compact_large_methods_memory':1.85}.get(f,1.70)
    assert abs(float(p.mediabox.height)-expected_height*72)<.01
sandwich=json.loads((V/'sandwich_plot_points.json').read_text())
assert len(sandwich['points'])==24
assert len({(p['dataset'],p['backend'],p['condition']) for p in sandwich['points']})==24
assert all(p['repeats']==1 and p['status']=='ok' for p in sandwich['points'])
for name in re.findall(r'\\includegraphics(?:\[[^\]]*\])?\{([^}]+)\}',s):
    assert '/' not in name and (V/name).exists()
scale=json.loads((R/'results/paper_scalability_single_v2_results/measurements.json').read_text())
assert len(scale)==40 and len({r['key'] for r in scale})==40
assert all(r['repeats']==1 and r['timeout_s']==60 and r['status']=='ok' and r['equal'] for r in scale)
search_scale=json.loads((R/'results/paper_search_scalability_single_results/measurements.json').read_text())
assert len(search_scale)==30 and len({r['key'] for r in search_scale})==30
assert all(r['repeats']==1 and r['timeout_s']==60 and r['status']=='ok' and r['equal'] for r in search_scale)
cases=json.loads((R/'results/paper_case_study_single_results/measurements.json').read_text())
assert cases['protocol']=='one execution per model and case-study condition'
assert set(cases['fraud_top50'])=={'Proposed','Degree','Neighbor'}
assert set(cases['legislative_prediction'])=={'Proposed','Degree','Neighbor'}
assert len(cases['runs'])==12 and all(r['status']=='ok' for r in cases['runs'])
assert 'tab:case-legislative' not in s and 'fig:case-senate' in s
assert 'Exp-11: Legislative co-sponsorship community discovery.' in s
assert 'compact_large_methods.pdf' in s and 'fig:large-methods' in s
assert 'compact_large_methods_memory.pdf' in s and 'fig:large-methods-memory' in s
fraud_table=s[s.index(r'\caption{Top-50 fraud screening'):s.index(r'\stitle{Exp-11:')]
for model in [r'$(k,\delta)$-Dense','Degree core','Neighbor core']:
    assert model in fraud_table
rows=json.loads((R/'results/paper_search_single_results/measurements.json').read_text())
assert len(rows)==30 and sum(r['status']=='ok' for r in rows)==27
assert sum(r['status']=='timeout' for r in rows)==3
displayed_rows=[r for r in rows if r['dataset']!='SC']
assert len(displayed_rows)==27 and sum(r['status']=='ok' for r in displayed_rows)==24
assert sum(r['status']=='timeout' for r in displayed_rows)==3
completion=json.loads((R/'results/paper_dblp_flow_completion_single_results/measurements.json').read_text())
assert len(completion)==1
completion=completion[0]
assert completion['status']=='ok' and completion['repeats']==1 and completion['deadline_s'] is None
assert completion['wall_s']>180 and completion['peak_rss_mib']>0
assert completion['output_rows']==1930378 and completion['equal']
runtime_point=[p for p in new['points'] if p['figure']=='compact_small_time' and
               p['dataset']=='DBLP' and p['method']=='hgd_flow'][0]
memory_point=[p for p in new['points'] if p['figure']=='compact_small_memory' and
              p['dataset']=='DBLP' and p['method']=='hgd_flow'][0]
assert runtime_point['value']==completion['wall_s']
assert memory_point['value']==completion['peak_rss_mib']
sa_pair=json.loads((R/'results/paper_sa_pair_single_v2_results/measurements.json').read_text())
assert len(sa_pair)==2 and {r['method'] for r in sa_pair}=={'neighbor','cocore'}
assert all(r['dataset']=='SA' and r['repeats']==1 and r['timeout_s']==180 and
           r['status']=='oom' and r['returncode']==-9 and r['wall_s']<180 and
           r['peak_rss_mib']>0 for r in sa_pair)
dynamic=json.loads((R/'results/paper_dynamic_1_5_10_15_20_single_results/measurements.json').read_text())
assert len(dynamic)==20 and len({(r['dataset'],r['operation'],r['fraction']) for r in dynamic})==20
assert {r['fraction'] for r in dynamic}=={.01,.05,.10,.15,.20}
assert all(r['repeats']==1 and r['status']=='ok' and r['equal'] for r in dynamic)
# The journal manuscript is deliberately condensed relative to the historical
# TKDE source.  Guard the central definitions and the requested unnumbered-math
# policy directly instead of requiring the two manuscripts to have identical
# inline-math sequences.
for fragment in [r'F_\delta(X)=\sum_{e\in E}', r'\rho_\delta(X)=\frac{F_\delta(X)}{|X|}',
                 r'F_\delta(A)+F_\delta(B)\le F_\delta(A\cup B)+F_\delta(A\cap B)',
                 r'F_\delta(X)-(k-1)|X|',
                 r'\begin{definition}[Capped Hyperedge Contribution]',
                 r'\begin{definition}[Capped Density]',
                 r'\begin{theorem}[Uniqueness of Dense Set]']:
    assert fragment in s
assert 'Uniqueness of Dense Sets' not in s
assert 'Equivalence of Capped Hyperedge Contribution and Orientation Definitions' not in s
assert 'thm:canonical' not in s
assert not re.search(r'\bintrinsic\b',s,re.I)
assert not re.search(r'\bmargins?\b|\bmarginal\b',s,re.I)
assert not re.search(r'\bclosures?\b',s,re.I)
assert not re.search(r'\b(?:fix|fixed|fixes)\b',body,re.I)
assert not re.search(r'\bthresholds?\b',body,re.I)
assert r'\tag{' not in s
assert not re.search(r'\\begin\{(?:equation|align|gather|multline)\}',s)
print('PASS: vendor hashes, page limit, three tables, eight algorithms, large-dataset runtime and memory comparisons, central definitions, unnumbered math, concise captions, scalability runs and Senate community case study')
