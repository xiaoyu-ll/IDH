"""Single-column tables and figures for two algorithms and model comparisons."""
import argparse
import json
import os
from pathlib import Path
import statistics
import sys

ROOT=Path(__file__).resolve().parents[1]
os.environ.setdefault('MPLCONFIGDIR',str(ROOT/'tmp/mpl'))
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
from PIL import Image
sys.path.insert(0,str(Path(__file__).parent/'figure_support'))
from setup_style import setup_style
from visual_qa import audit_layout,print_report

DATA=['SC','HC','CP','CH','SB','HB']
METHODS=['hgd_flow','hgd_reverse','core','neighbor','cocore','abcore']
NAMES=['Flow','Reverse','Degree','Neighbor','CoCore','BiCore']
COLOR=['#0072B2','#D55E00','#009E73','#CC79A7','#8C6600','#666666']
MARK=['o','s','^','v','D','P']
RESULT=ROOT/'results/paper_comparison_results'
PREVIOUS=ROOT/'results/paper_results_final'
FIG=ROOT/'paper'


def save(fig,name,export):
    fig.canvas.draw()
    if print_report(audit_layout(fig))=='FAIL':raise RuntimeError(name)
    fig.savefig(FIG/f'{name}.png',dpi=300)
    Image.open(FIG/f'{name}.png').convert('L').save(FIG/f'{name}_gray.png')
    if export:fig.savefig(FIG/f'{name}.pdf')
    plt.close(fig)


def table(caption,label,header,rows):
    return '\n'.join([r'\begin{table}[t]\centering\small',r'\caption{'+caption+r'}\label{'+label+'}',
        r'\setlength{\tabcolsep}{3.5pt}\begin{tabular}{l'+'r'*(len(header)-1)+r'}\toprule',
        ' & '.join(header)+r'\\\midrule',*[' & '.join(map(str,row))+r'\\' for row in rows],
        r'\bottomrule\end{tabular}\end{table}'])


def main():
    p=argparse.ArgumentParser();p.add_argument('--export',action='store_true');args=p.parse_args()
    FIG.mkdir(exist_ok=True,parents=True)
    rows=json.loads((RESULT/'measurements.json').read_text())
    structures=json.loads((RESULT/'structural.json').read_text())
    old=json.loads((PREVIOUS/'measurements.json').read_text())
    dynamic=json.loads((PREVIOUS/'dynamic.json').read_text())
    datasets=json.loads((PREVIOUS/'datasets.json').read_text())
    ablation=json.loads((PREVIOUS/'ablation.json').read_text())
    assert len(rows)==108 and all(r['status']=='ok' for r in rows)
    setup_style(journal='ieee',lang='en',use_sciplots=False)
    plt.rcParams.update({'font.size':7.5,'axes.labelsize':8,'axes.titlesize':8,
        'legend.fontsize':7,'xtick.labelsize':7,'ytick.labelsize':7,
        'savefig.bbox':None,'figure.constrained_layout.use':True})
    for metric,ylabel,name in [('wall_s','End-to-end time (s, log)','runtime'),
                              ('peak_rss_mib','Peak RSS (MiB, log)','memory')]:
        fig,ax=plt.subplots(figsize=(3.48,2.08))
        for j,method in enumerate(METHODS):
            for i,ds in enumerate(DATA):
                vals=[r[metric] for r in rows if r['dataset']==ds and r['method']==method]
                assert len(vals)==3
                ax.scatter(i+(j-2.5)*.115+np.linspace(-.018,.018,3),vals,s=13,marker=MARK[j],
                           color=COLOR[j],label=NAMES[j] if i==0 else None,linewidths=.55)
        ax.set_xticks(range(6),DATA);ax.set_xlabel('Dataset');ax.set_ylabel(ylabel)
        ax.set_yscale('log');ax.grid(axis='y',alpha=.18)
        handles,labels=ax.get_legend_handles_labels()
        fig.legend(handles,labels,loc='outside upper center',ncol=3,frameon=False,
                   columnspacing=.8,handletextpad=.25)
        save(fig,name,args.export)
    fig,ax=plt.subplots(figsize=(3.48,2.05))
    for j,method in enumerate(['scc_cached_plus','scc_reverse_csr_plus']):
        x=[];y=[];lo=[];hi=[]
        for f in [.1,.25,.5,.75,1.]:
            group=[r for r in old if r['kind']=='scale' and r['method']==method and r['fraction']==f]
            vals=[r['wall_s'] for r in group];assert len(vals)==3
            x.append(group[0]['m']/1000);y.append(statistics.median(vals));lo.append(min(vals));hi.append(max(vals))
            ax.scatter([x[-1]]*3,vals,s=10,color=COLOR[j],marker=MARK[j])
        ax.plot(x,y,color=COLOR[j],marker=MARK[j],linestyle=['-','--'][j],markersize=3,label=NAMES[j])
        ax.fill_between(x,lo,hi,color=COLOR[j],alpha=.12)
    ax.set_xlabel('Retained HB edges (thousands)');ax.set_ylabel('End-to-end time (s)')
    ax.set_ylim(bottom=0);ax.legend(frameon=False);ax.grid(axis='y',alpha=.18)
    save(fig,'scaling',args.export)
    for ds in ['SB','HB']:
        fig,ax=plt.subplots(figsize=(3.48,2.05))
        for op,ls,marker in [('insert','-','o'),('delete','--','s')]:
            for j,key in enumerate(['warm_s','fresh_s']):
                ys=[];lo=[];hi=[]
                for percent in [1,5,10]:
                    vals=[r[key] for r in dynamic if r['dataset']==ds and r['operation']==op and abs(r['fraction']*100-percent)<1e-6]
                    assert len(vals)==3
                    ys.append(statistics.median(vals));lo.append(min(vals));hi.append(max(vals))
                    ax.scatter([percent]*3,vals,s=10,color=COLOR[j],marker=marker,
                               facecolors='none' if j else COLOR[j])
                ax.plot([1,5,10],ys,color=COLOR[j],linestyle=ls,marker=marker,markersize=3,
                        markerfacecolor='none' if j else COLOR[j],label=f'{op.title()} '+['repair','fresh'][j])
                ax.fill_between([1,5,10],lo,hi,color=COLOR[j],alpha=.08)
        ax.set_xlabel(f'{ds}: batch size (% of original edges)');ax.set_ylabel('Time (s, log)')
        ax.set_xticks([1,5,10]);ax.set_yscale('log');ax.grid(axis='y',alpha=.18)
        handles,labels=ax.get_legend_handles_labels()
        fig.legend(handles,labels,loc='outside upper center',ncol=2,frameon=False,columnspacing=.8)
        save(fig,f'dynamic_{ds}',args.export)
    tex=[]
    tex.append(table('Normalized datasets. Repeated edge records remain distinct; $L$ counts normalized incidences.',
        'tab:new-data',['Data','$n$','$m$','$L$',r'$\max |e|$'],
        [[ds,f"{datasets[ds]['n']:,}",f"{datasets[ds]['m']:,}",f"{datasets[ds]['L']:,}",datasets[ds]['max_size']] for ds in DATA]))
    search_rows=[]
    for ds in DATA:
        cells=[ds]
        for m in ['path','flow','flow+']:
            group=[r for r in old if r['kind']=='query' and r['dataset']==ds and r['method']==m]
            assert len(group)==3
            values=[r['wall_s'] for r in group if r['status']=='ok']
            if not values:
                assert all(r['status']=='timeout' for r in group)
                cell='TO'
            else:
                assert len(values)==3
                cell=f'{statistics.median(values):.3f}'
            cells.append(cell)
        search_rows.append(cells)
    tex.append(table(r'Single-threshold search time (seconds), $k=5$, $\delta=5$. PATH, FLOW and FLOW+ abbreviate the three DSM methods. TO denotes trials censored at 15 seconds.',
        'tab:search-three',['Data','PATH','FLOW','FLOW+'],search_rows))
    def med(ds,m,metric):return statistics.median(r[metric] for r in rows if r['dataset']==ds and r['method']==m)
    timing=[]
    for ds in DATA:
        cells=[ds]
        for m in METHODS[:2]:
            vals=[r['wall_s'] for r in rows if r['dataset']==ds and r['method']==m]
            cells.append(f'{statistics.median(vals):.3f}')
        timing.append(cells)
    tex.append(table(r'Full decomposition time (seconds), $\delta=5$.',
        'tab:new-all',['Data','Flow-Decomp','Reverse-Decomp'],timing))
    tex.append(table(r'Cross-model runtime (seconds). D: degree core; N: neighborhood core; C: constrained core ($h=5$); B: bipartite core ($\beta=5$). All return one score per original vertex.',
        'tab:external-time',['Data','D','N','C','B'],[[ds]+[f'{med(ds,m,"wall_s"):.3f}' for m in METHODS[2:]] for ds in DATA]))
    tex.append(table(r'Occupied score levels, including zero if present. O$_1$/O$_5$: proposed model at $\delta=1/5$. C fixes $h=5$ and B fixes $\beta=5$. More levels do not imply better prediction.',
        'tab:external-levels',['Data','D','N','C','B',r'O$_1$',r'O$_5$'],
        [[ds]+[structures[ds][m]['levels'] for m in METHODS[2:]]+
         [next(r['layers'] for r in ablation if r['dataset']==ds and r['delta']==1),structures[ds]['hgd_reverse']['levels']] for ds in DATA]))
    tex.append(table('Parameter sweep: highest IDN / occupied levels.',
        'tab:new-delta',['Data',r'$\delta=1$','2','5','10'],
        [[ds]+[f"{next(r for r in ablation if r['dataset']==ds and r['delta']==d)['kmax']} / {next(r for r in ablation if r['dataset']==ds and r['delta']==d)['layers']}" for d in [1,2,5,10]] for ds in DATA]))
    (ROOT/'paper/two_algorithm_tables.tex').write_text('\n\n'.join(tex)+'\n')
    summary={ds:dict(time_ratio=med(ds,'hgd_flow','wall_s')/med(ds,'hgd_reverse','wall_s'),
                    flow_rss=med(ds,'hgd_flow','peak_rss_mib'),reverse_rss=med(ds,'hgd_reverse','peak_rss_mib')) for ds in DATA}
    (RESULT/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    hb=summary['HB'];sb=summary['SB']
    findings=(f"Reverse-Decomp has lower median end-to-end time than Flow-Decomp on all six datasets; the median-time ratios are {sb['time_ratio']:.2f} on SB and {hb['time_ratio']:.2f} on HB. "
        f"On HB, median peak RSS is {hb['reverse_rss']:.2f} MiB for Reverse-Decomp and {hb['flow_rss']:.2f} MiB for Flow-Decomp. "
        "Degree and bipartite cores are faster than both proposed algorithms on SB and HB. Neighborhood and constrained cores also run faster on those two datasets. "
        "These models solve different decomposition tasks; the measurements do not support a universal runtime advantage for the proposed density model.\n")
    (ROOT/'paper/two_algorithm_findings.tex').write_text(findings)
    print(json.dumps(summary,indent=2))


if __name__=='__main__':
    from render_compact import main as compact_main
    compact_main()
