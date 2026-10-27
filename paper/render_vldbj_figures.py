import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
sys.path.insert(0,str(Path(__file__).resolve().parent/"figure_support"))
import argparse
import hashlib
import json
from pathlib import Path
import statistics
from search_summary import single_cell as search_cell

from render_two_algorithms import ROOT,DATA,METHODS,NAMES,COLOR,MARK,plt,np,setup_style,save,table

from PIL import Image
from matplotlib.ticker import MaxNLocator
from visual_qa import audit_layout,print_report
def save(fig,name,export):
    fig.canvas.draw()
    if print_report(audit_layout(fig))=='FAIL': raise RuntimeError(name)
    outputs=[Path(__file__).parent]
    for out in outputs:
        out.mkdir(parents=True,exist_ok=True)
        fig.savefig(out/f'{name}.png',dpi=300)
        Image.open(out/f'{name}.png').convert('L').save(out/f'{name}_gray.png')
        if export: fig.savefig(out/f'{name}.pdf')
    plt.close(fig)

def add_panel_labels(fig,style='ieee',x_offset_pt=0,y_offset_pt=4,ha='left',fontsize=7):
    """Physical-offset layout_tools convention, bundled without a host dependency."""
    assert style=='ieee'
    if len(fig.axes)==1:return
    for i,ax in enumerate(fig.axes):
        label=f'({chr(ord("a")+i)})'
        ax.annotate(label,(0,1),xycoords='axes fraction',xytext=(x_offset_pt,y_offset_pt),
                    textcoords='offset points',fontsize=fontsize,fontweight='bold',
                    color='black',ha=ha,va='bottom',annotation_clip=False)

PROJECT=ROOT/'results'
LARGE=['TC','SA','TM','DBLP']
DISPLAY_DATA=[dataset for dataset in DATA if dataset!='SC']
SANDWICH_METHODS=['hgd_flow_sandwich','hgd_reverse_sandwich']
ALL_NAMES=NAMES+['Flow-S','Reverse-S']
ALL_COLOR=COLOR+['#56B4E9','#E69F00']
ALL_MARK=MARK+['X','*']
PAIR_HEIGHT=1.70
LARGE_HEIGHT=1.85
HIERARCHY_HEIGHT=2.20
MANIFEST={'summary':'retained methods use one median point from three launches except the single DBLP Flow completion run; Sandwich executions and scalability use one launch',
          'pair_inches':[3.30,PAIR_HEIGHT],'points':[],'figures':[],'legends':{}}

def read(directory,file):return json.loads((PROJECT/directory/file).read_text())
def med(values):return statistics.median(values)
def point(figure,panel,dataset,method,condition,value,replicates=3,status='ok'):
    MANIFEST['points'].append(dict(figure=figure,panel=panel,dataset=dataset,
        method=method,condition=condition,value=value,replicates=replicates,status=status))

def pair():
    fig,axes=plt.subplots(1,2,figsize=(3.30,PAIR_HEIGHT),layout='constrained')
    fig.set_constrained_layout_pads(w_pad=.045,h_pad=.045,wspace=.04,hspace=.02)
    for ax in axes:
        ax.grid(axis='y',alpha=.17);ax.tick_params(pad=1.5,length=2,width=.6)
        for spine in ax.spines.values():spine.set_linewidth(.6)
    return fig,axes

def finish(fig,name,export,legend=True,legend_ncol=None):
    if legend:
        seen={}
        for ax in fig.axes:
            for h,l in zip(*ax.get_legend_handles_labels()):seen.setdefault(l,h)
        one_row=legend_ncol is not None
        fig.legend(list(seen.values()),list(seen),loc='outside upper center',
                   ncol=legend_ncol or (4 if len(seen)>6 else (3 if len(seen)>4 else 2)),
                   frameon=False,fontsize=6 if one_row else None,
                   columnspacing=.30 if one_row else .65,
                   handletextpad=.12 if one_row else .2,
                   handlelength=.9 if one_row else 1.3,borderpad=.05 if one_row else .1)
    fig.canvas.draw()
    legends=list(fig.legends)+[ax.get_legend() for ax in fig.axes if ax.get_legend() is not None]
    MANIFEST['legends'][name]=[t.get_text() for legend in legends for t in legend.get_texts()]
    add_panel_labels(fig,style='ieee',x_offset_pt=0,y_offset_pt=4,ha='left',fontsize=7)
    MANIFEST['figures'].append(name)
    save(fig,name,export)

def comparison_bars(panels,name,metric,ylabel,export):
    """Place all displayed datasets in one axis, with each baseline beside its acceleration."""
    fig,ax=plt.subplots(figsize=(3.30,PAIR_HEIGHT),layout='constrained')
    fig.set_constrained_layout_pads(w_pad=.045,h_pad=.045,wspace=.03,hspace=.025)
    methods=['hgd_flow','hgd_flow_sandwich','hgd_reverse','hgd_reverse_sandwich']
    labels=['Flow','Flow-S','Reverse','Reverse-S']
    colors=['#0072B2','#56B4E9','#D55E00','#E69F00']
    hatches=['','//','', '\\\\']
    width=.78/len(methods)
    datasets=[ds for _,group,_ in panels for ds in group]
    has_timeout=False
    for j,method in enumerate(methods):
        xs=[];ys=[]
        for i,ds in enumerate(datasets):
            rows,_,sandwich_rows=next(panel for panel in panels if ds in panel[1])
            if method in SANDWICH_METHODS:
                backend='flow' if method=='hgd_flow_sandwich' else 'reverse'
                group=[r for r in sandwich_rows if r['dataset']==ds and r['backend']==backend and r['accelerated']]
                assert len(group)==1
            else:
                group=[r for r in rows if r['dataset']==ds and r['method']==method]
                assert len(group) in {1,3}
            statuses={r['status'] for r in group}
            if statuses=={'ok'} or (metric=='peak_rss_mib' and statuses=={'timeout'}):
                value=group[0][metric] if len(group)==1 else med([r[metric] for r in group])
                xs.append(i+(j-(len(methods)-1)/2)*width);ys.append(value)
                point(name,0,ds,method,metric,value,len(group),
                      status='timeout' if statuses=={'timeout'} else 'ok')
            elif statuses=={'timeout'} and metric=='wall_s':
                has_timeout=True
                xs.append(i+(j-(len(methods)-1)/2)*width);ys.append(group[0]['timeout_s'])
                point(name,0,ds,method,metric,group[0]['timeout_s'],len(group),status='timeout')
            else:
                assert statuses=={'timeout'},statuses
        ax.bar(xs,ys,width=width*.92,color=colors[j],edgecolor='black',linewidth=.28,
               hatch=hatches[j],label=labels[j],zorder=2)
    ax.set_xticks(range(len(datasets)),datasets);ax.tick_params(axis='x',labelsize=6.5)
    ax.set_xlabel('Dataset');ax.set_ylabel(ylabel);ax.set_yscale('log');ax.minorticks_off()
    ax.grid(axis='y',alpha=.17,zorder=0)
    if metric=='wall_s' and has_timeout:
        ax.axhline(180,color='.45',linestyle=':',linewidth=.6)
    finish(fig,name,export,legend_ncol=4)

def large_method_bars(large,sandwich_rows,sa_pair_rows,metric,ylabel,name,export):
    """Compare all eight methods in one grouped bar chart."""
    fig,ax=plt.subplots(figsize=(3.30,LARGE_HEIGHT),layout='constrained')
    fig.set_constrained_layout_pads(w_pad=.045,h_pad=.045,wspace=.03,hspace=.03)
    methods=['core','neighbor','cocore','abcore','hgd_flow','hgd_flow_sandwich',
             'hgd_reverse','hgd_reverse_sandwich']
    labels=['Degree','Neighbor','CoCore','BiCore','Flow','Flow-S','Reverse','Reverse-S']
    colors=[COLOR[METHODS.index(m)] for m in methods[:4]]+[
            '#0072B2','#56B4E9','#D55E00','#E69F00']
    hatches=['//','\\\\','xx','..','','--','++','oo']
    width=.88/len(methods)
    for j,(method,label,color,hatch) in enumerate(zip(methods,labels,colors,hatches)):
        for i,ds in enumerate(LARGE):
            if ds=='SA' and method in {'neighbor','cocore'}:
                group=[r for r in sa_pair_rows if r['method']==method]
            elif method in SANDWICH_METHODS:
                backend='flow' if method=='hgd_flow_sandwich' else 'reverse'
                group=[r for r in sandwich_rows if r['dataset']==ds and
                       r['backend']==backend and r['accelerated']]
            else:
                group=[r for r in large if r['dataset']==ds and r['method']==method]
            assert len(group) in {1,3}
            statuses={r['status'] for r in group}
            assert statuses in ({'ok'},{'oom'}),(ds,method,statuses)
            value=group[0][metric] if len(group)==1 else med([r[metric] for r in group])
            x=i+(j-(len(methods)-1)/2)*width
            failed=statuses=={'oom'}
            ax.bar([x],[value],width=width*.93,color='white' if failed else color,
                   edgecolor=color if failed else 'black',linewidth=.55 if failed else .25,
                   hatch='////' if failed else hatch,label=label if i==0 else None,zorder=2)
            if failed:
                ax.text(x,value*1.08,'OOM',ha='center',va='bottom',rotation=90,fontsize=5.2)
            point(name,0,ds,method,metric,value,len(group),status='oom' if failed else 'ok')
    ax.set_xticks(range(len(LARGE)),LARGE);ax.set_xlabel('Dataset')
    ax.set_ylabel(ylabel);ax.set_yscale('log')
    ax.set_ylim((.08,1000) if metric=='wall_s' else (30,10000))
    ax.minorticks_off();ax.grid(axis='y',alpha=.17,zorder=0)
    ax.legend(loc='upper center',bbox_to_anchor=(.5,1.16),ncol=8,frameon=False,
              fontsize=5.2,columnspacing=.30,handlelength=.82,handletextpad=.14)
    finish(fig,name,export,legend=False)

def hierarchy_quality(structures,export):
    name='compact_hierarchy_quality';models=['core','neighbor','cocore','abcore','hgd_reverse']
    labels=['Degree','Neighbor','CoCore','BiCore',r'$(k,\delta)$-dense'];fig,axes=plt.subplots(3,1,figsize=(3.30,HIERARCHY_HEIGHT),layout='constrained')
    fig.set_constrained_layout_pads(w_pad=.04,h_pad=.04,wspace=.02,hspace=.03)
    metrics=[]
    for ds in DISPLAY_DATA:
        row=[]
        for method in models:
            values=structures[ds][method]['scores'];levels=sorted(set(values))
            sets=[{i for i,v in enumerate(values) if v>=k} for k in levels]
            similarity=1.0 if len(sets)<2 else sum(len(sets[i]&sets[i+1])/len(sets[i]|sets[i+1]) for i in range(len(sets)-1))/(len(sets)-1)
            row.append((structures[ds][method]['kmax']+1,structures[ds][method]['levels'],similarity))
        metrics.append(row)
    width=.82/len(models);colors=[COLOR[METHODS.index(m)] for m in models]
    for panel,(ax,index,ylabel,log) in enumerate(zip(axes,[0,1,2],['Total','Nonempty','Jaccard'],[True,True,False])):
        for j,(method,label,color) in enumerate(zip(models,labels,colors)):
            vals=[metrics[i][j][index] for i in range(len(DISPLAY_DATA))]
            xs=np.arange(len(DISPLAY_DATA))+(j-2)*width
            ax.bar(xs,vals,width=width*.94,color=color,edgecolor='black',linewidth=.28,hatch=['//','\\\\','xx','..',''][j],label=label)
            for ds,value in zip(DISPLAY_DATA,vals):point(name,panel,ds,method,['total','nonempty','consistency'][index],value,1)
        ax.set_xticks(range(len(DISPLAY_DATA)),DISPLAY_DATA);ax.set_ylabel(ylabel,fontsize=6.5,labelpad=2);ax.grid(axis='y',alpha=.17,zorder=0)
        ax.tick_params(axis='y',labelsize=6,pad=1)
        if panel<2:ax.tick_params(axis='x',labelbottom=False)
        if log:
            ax.set_yscale('log');ax.minorticks_off();ax.set_yticks([1,1000] if panel==0 else [1,100])
        else:
            ax.set_ylim(0,1.05);ax.yaxis.set_major_locator(MaxNLocator(3))
    axes[-1].set_xlabel('Dataset')
    finish(fig,name,export,legend_ncol=len(labels))

def scalability(rows,export):
    name='compact_scalability';fig,axes=pair();labels={'flow':'Flow','reverse':'Reverse','flow_s':'Flow-S','reverse_s':'Reverse-S'}
    styles={'flow':('-','o',COLOR[0]),'reverse':('--','s',COLOR[1]),'flow_s':(':','X','#56B4E9'),'reverse_s':('-.','*','#E69F00')}
    for panel,(ax,dimension) in enumerate(zip(axes,['vertices','edges'])):
        for method in ['flow','reverse','flow_s','reverse_s']:
            group=sorted([r for r in rows if r['dimension']==dimension and r['method']==method],key=lambda r:r['fraction'])
            assert len(group)==5 and all(r['status']=='ok' and r['equal'] for r in group)
            xs=[100*r['fraction'] for r in group];ys=[r['wall_s'] for r in group];ls,marker,color=styles[method]
            ax.plot(xs,ys,linestyle=ls,marker=marker,color=color,markersize=2.8,linewidth=.75,label=labels[method])
            for r in group:point(name,panel,'HB',method,dimension+str(r['fraction']),r['wall_s'],1)
        ax.set_xticks([20,40,60,80,100]);ax.set_xlabel('Vary '+dimension+' (%)');ax.set_ylabel('Time (s)');ax.set_ylim(bottom=0);ax.grid(axis='y',alpha=.17)
    finish(fig,name,export,legend_ncol=len(labels))

def search_scalability(rows,export):
    name='compact_search_scalability';fig,axes=pair()
    labels={'path':'DSM-PATH','flow':'DSM-FLOW','flow+':'DSM-FLOW+'}
    styles={'path':('-','o','#0072B2'),'flow':('--','s','#D55E00'),'flow+':(':','D','#009E73')}
    for panel,(ax,dimension) in enumerate(zip(axes,['vertices','edges'])):
        for method in ['path','flow','flow+']:
            group=sorted([r for r in rows if r['dimension']==dimension and r['method']==method],key=lambda r:r['fraction'])
            assert len(group)==5 and all(r['status']=='ok' and r['equal'] and r['repeats']==1 for r in group)
            xs=[100*r['fraction'] for r in group];ys=[r['wall_s'] for r in group];ls,marker,color=styles[method]
            ax.plot(xs,ys,linestyle=ls,marker=marker,color=color,markersize=2.8,linewidth=.75,label=labels[method])
            for r in group:point(name,panel,'HC',method,dimension+str(r['fraction']),r['wall_s'],1)
        ax.set_xticks([20,40,60,80,100]);ax.set_xlabel('Vary '+dimension+' (%)')
        ax.set_ylabel('Time (s, log)');ax.set_yscale('log');ax.minorticks_off();ax.grid(axis='y',alpha=.17)
    finish(fig,name,export,legend_ncol=3)

def dynamic_panel(ax,rows,ds,name,panel,operations=('insert','delete')):
    fractions=sorted({r['fraction'] for r in rows if r['dataset']==ds})
    styles={
        ('insert','warm_s'):('-', 'o', COLOR[0], 'Insertion (inc.)'),
        ('insert','fresh_s'):('--','s', COLOR[0], 'Insertion (full)'),
        ('delete','warm_s'):('-', '^', COLOR[1], 'Deletion (inc.)'),
        ('delete','fresh_s'):('--','D', COLOR[1], 'Deletion (full)'),
    }
    for operation in ['insert','delete']:
        if operation not in operations:continue
        for key in ['warm_s','fresh_s']:
            ys=[]
            for fraction in fractions:
                group=[r for r in rows if r['dataset']==ds and r['operation']==operation and r['fraction']==fraction]
                assert len(group) in {1,3} and all(r.get('status','ok')=='ok' and r['equal'] for r in group)
                value=group[0][key] if len(group)==1 else med([r[key] for r in group]);ys.append(value)
                point(name,panel,ds,operation+'_'+key,fraction,value,len(group))
            ls,marker,color,label=styles[(operation,key)]
            ax.plot([100*f for f in fractions],ys,color=color,linestyle=ls,marker=marker,
                    markersize=2.6,linewidth=.7,markerfacecolor='white',markeredgewidth=.65,
                    label=label)
    ticks=[100*f for f in fractions]
    ax.set_xticks(ticks,[f'{v:g}%' for v in ticks]);ax.set_xlabel('Updated hyperedges')
    ax.set_ylabel('Time (s)');ax.set_ylim(bottom=0)
    ax.set_title(ds if len(operations)>1 else ds+' '+operations[0],fontsize=8,pad=6)

def main():
    p=argparse.ArgumentParser();p.add_argument('--export',action='store_true')
    args=p.parse_args()
    setup_style(journal='ieee',lang='en',use_sciplots=False)
    plt.rcParams.update({'font.size':8,'axes.labelsize':8,'axes.titlesize':8,
        'legend.fontsize':7,'xtick.labelsize':7,'ytick.labelsize':7,
        'savefig.bbox':None,'figure.constrained_layout.use':True})
    small=read('paper_comparison_results','measurements.json')
    large=read('paper_large_results','measurements.json')+read('paper_public_results','measurements.json')
    completion=read('paper_dblp_flow_completion_single_results','measurements.json')
    assert len(completion)==1 and completion[0]['status']=='ok' and completion[0]['equal']
    large=[r for r in large if not (r['dataset']=='DBLP' and r['method']=='hgd_flow')]+completion
    datasets=read('paper_results_final','datasets.json')|read('paper_large_results','datasets.json')|read('paper_public_results','datasets.json')
    old=read('paper_results_final','measurements.json');ablation=read('paper_results_final','ablation.json')
    sandwich_small=read('paper_sandwich_single_v2_results','measurements.json')
    sandwich_integrated=read('paper_sandwich_integration_single_results','measurements.json')
    comparison_panels=[(small,DISPLAY_DATA,sandwich_small),
                       (large,LARGE,[r for r in sandwich_integrated if r['kind']=='large'])]
    comparison_bars(comparison_panels,'compact_small_time','wall_s','Time (s, log)',args.export)
    comparison_bars(comparison_panels,'compact_small_memory','peak_rss_mib','RSS (MB, log)',args.export)
    sa_pair=read('paper_sa_pair_single_v2_results','measurements.json')
    sandwich_large=[r for r in sandwich_integrated if r['kind']=='large']
    large_method_bars(large,sandwich_large,sa_pair,'wall_s','Time (s, log)',
                      'compact_large_methods',args.export)
    large_method_bars(large,sandwich_large,sa_pair,'peak_rss_mib','RSS (MB, log)',
                      'compact_large_methods_memory',args.export)
    structures=read('paper_comparison_results','structural.json')
    hierarchy_quality(structures,args.export)
    scalability(read('paper_scalability_single_v2_results','measurements.json'),args.export)
    search_scalability(read('paper_search_scalability_single_results','measurements.json'),args.export)
    rows=read('paper_dynamic_1_5_10_15_20_single_results','measurements.json')
    fig,axes=pair();name='compact_dynamic_selected'
    for panel,(ax,ds) in enumerate(zip(axes,['SB','TC'])):dynamic_panel(ax,rows,ds,name,panel)
    finish(fig,name,args.export,legend_ncol=4)
    MANIFEST['source_sha256']=hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
    (Path(__file__).parent/'plot_points.json').write_text(json.dumps(MANIFEST,indent=2)+'\n')
    print('PASS: merged four-method bar comparisons, three hierarchy-quality panels and two-dimensional scalability rendered; no table or measurement writes')

if __name__=='__main__':main()
