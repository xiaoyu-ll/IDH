#ifndef IDH_DECOMPOSITION_COMMON_HPP
#define IDH_DECOMPOSITION_COMMON_HPP
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <queue>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>
using namespace std;

struct DVertex { int indegree=0; bool active=false; vector<int> tails,heads; };
struct DHyperedge { vector<int> vertices,tails,heads; };

class DecompositionEngine {
  vector<DVertex> vs; vector<DHyperedge> es; vector<int> active;
  vector<char> seed,eligible; vector<int> label,parent,parent_edge;
  int max_delta_=0,max_degree_=0;
  unsigned long long transfers_=0,global_builds_=0;
  bool stats_=std::getenv("IDH_STATS")!=nullptr;
  bool fast_partition_=std::getenv("IDH_NO_FAST_PARTITION")==nullptr;
  static vector<int> parse(const string& s){ vector<int> r; string x; stringstream q(s); while(getline(q,x,',')) if(!x.empty()) r.push_back(stoi(x)); return r; }
  static void erase1(vector<int>& a,int x){ auto p=find(a.begin(),a.end(),x); if(p!=a.end()) a.erase(p); }
  void transfer(int head,int tail,int ei){
    ++transfers_;
    --vs[head].indegree; erase1(vs[head].heads,ei); vs[head].tails.push_back(ei); erase1(es[ei].heads,head); es[ei].tails.push_back(head);
    ++vs[tail].indegree; erase1(vs[tail].tails,ei); vs[tail].heads.push_back(ei); erase1(es[ei].tails,tail); es[ei].heads.push_back(tail);
  }
  void local_out(int start,int k){
    queue<int> q; q.push(start);
    while(!q.empty()){
      int t=q.front(); q.pop();
      for(int i=0;i<(int)vs[t].heads.size();){
        int ei=vs[t].heads[i],s=-1;
        for(int v:es[ei].tails) if(eligible[v]&&!seed[v]&&vs[t].indegree-vs[v].indegree>=2){s=v;break;}
        if(s<0){++i;continue;} transfer(t,s,ei); if(vs[s].indegree>=k) seed[s]=true; q.push(s);
      }
    }
  }
  void local_in(int start){
    queue<int> q; q.push(start);
    while(!q.empty()){
      int s=q.front();q.pop();
      for(int i=0;i<(int)vs[s].tails.size();){
        int ei=vs[s].tails[i],t=-1;
        for(int v:es[ei].heads) if(eligible[v]&&!seed[v]&&vs[v].indegree-vs[s].indegree>=2){t=v;break;}
        if(t<0){++i;continue;} transfer(t,s,ei); q.push(t);
      }
    }
  }
  bool global_out(int k){
    ++global_builds_;
    const int inf=numeric_limits<int>::max();
    auto apply_targets=[&](const vector<int>&targets){int applied=0;vector<int>promoted;for(int target:targets){
      vector<pair<int,int>>path;int source=target;while(parent[source]!=source){path.push_back({source,parent_edge[source]});source=parent[source];}
      if(seed[source]||!seed[target]||vs[target].indegree-vs[source].indegree<2)continue;
      reverse(path.begin(),path.end());int u=source;bool valid=true;
      for(auto [w,ei]:path)if(find(es[ei].tails.begin(),es[ei].tails.end(),u)==es[ei].tails.end()||find(es[ei].heads.begin(),es[ei].heads.end(),w)==es[ei].heads.end()){valid=false;break;}else u=w;
      if(!valid)continue;u=source;for(auto [w,ei]:path){transfer(w,u,ei);u=w;}++applied;if(vs[source].indegree>=k){seed[source]=true;promoted.push_back(source);}
    }for(int v:promoted)local_out(v,k);return applied;};

    // Fast blocking batch: every outside vertex remains its own root, so one
    // low-indegree root cannot absorb the forests of all other sources.
    vector<int>targets;
    if(fast_partition_){
      fill(label.begin(),label.end(),inf);queue<int>bfs;
      for(int v:active)if(eligible[v]&&!seed[v]){label[v]=vs[v].indegree;parent[v]=v;parent_edge[v]=-1;bfs.push(v);}
      while(!bfs.empty()){int u=bfs.front();bfs.pop();for(int ei:vs[u].tails)for(int w:es[ei].heads)if(eligible[w]&&label[w]==inf){label[w]=label[u];parent[w]=u;parent_edge[w]=ei;bfs.push(w);}}
      for(int v:active)if(seed[v]&&label[v]!=inf&&vs[v].indegree-label[v]>=2)targets.push_back(v);
      int fast=apply_targets(targets);if(fast)return true;
    }

    // Exact fallback: propagate the minimum reachable outside indegree.  This
    // is needed only to certify or expose paths hidden by the partition batch.
    fill(label.begin(),label.end(),inf);vector<vector<int>>roots(max_degree_+1);
    for(int v:active)if(eligible[v]&&!seed[v])roots[vs[v].indegree].push_back(v);
    targets.clear();
    for(int d=0;d<(int)roots.size();++d)for(int root:roots[d]){if(label[root]<=d)continue;label[root]=d;parent[root]=root;parent_edge[root]=-1;queue<int>q;q.push(root);
      while(!q.empty()){int u=q.front();q.pop();for(int ei:vs[u].tails)for(int w:es[ei].heads)if(eligible[w]&&d<label[w]){label[w]=d;parent[w]=u;parent_edge[w]=ei;q.push(w);}}
    }
    for(int v:active)if(seed[v]&&label[v]!=inf&&vs[v].indegree-label[v]>=2)targets.push_back(v);
    return apply_targets(targets)>0;
  }
  void repair(int v,int k){
    int before=vs[v].indegree;
    for(int i=0;i<(int)vs[v].heads.size();){int ei=vs[v].heads[i],x=-1;for(int u:es[ei].tails)if(seed[u]&&vs[v].indegree-vs[u].indegree>=2){x=u;break;}if(x<0)++i;else transfer(v,x,ei);}
    for(int i=0;i<(int)vs[v].tails.size();){int ei=vs[v].tails[i],x=-1;for(int u:es[ei].heads)if(seed[u]&&vs[u].indegree-vs[v].indegree>=2){x=u;break;}if(x<0)++i;else transfer(x,v,ei);}
    if(before>vs[v].indegree)local_in(v);else if(before<vs[v].indegree)local_out(v,k); if(vs[v].indegree<k)seed[v]=false;
  }
  vector<int> closure()const{
    vector<char> in(vs.size());queue<int>q;for(int v:active)if(eligible[v]&&seed[v]){in[v]=true;q.push(v);}
    while(!q.empty()){int t=q.front();q.pop();for(int ei:vs[t].heads)for(int s:es[ei].tails)if(eligible[s]&&!in[s]){in[s]=true;q.push(s);}}
    vector<int>r;for(int v:active)if(in[v])r.push_back(v);return r;
  }
 public:
  explicit DecompositionEngine(const string& path){
    ifstream f(path);if(!f)throw runtime_error("cannot open dataset: "+path);string line;int mx=0;
    while(f>>line){DHyperedge e;e.vertices=parse(line);for(int v:e.vertices)mx=max(mx,v);max_delta_=max(max_delta_,(int)e.vertices.size());es.push_back(std::move(e));}
    vs.resize(mx+1);vector<int>deg(mx+1);
    for(auto&e:es)for(int v:e.vertices){vs[v].active=true;max_degree_=max(max_degree_,++deg[v]);}
    for(int v=0;v<=mx;++v)if(vs[v].active)active.push_back(v);label.resize(vs.size());parent.resize(vs.size());parent_edge.resize(vs.size());
  }
  int maximum_delta()const{return max_delta_;} int maximum_k_bound()const{return max_degree_;}
  vector<int> all_vertices()const{return active;}
  void orient(int delta){
    for(auto&v:vs){v.indegree=0;v.tails.clear();v.heads.clear();}
    for(int ei=0;ei<(int)es.size();++ei){auto&e=es[ei];e.tails.clear();e.heads.clear();int h=min(delta,(int)e.vertices.size());vector<int>order=e.vertices;
      stable_sort(order.begin(),order.end(),[&](int a,int b){return vs[a].indegree<vs[b].indegree||(vs[a].indegree==vs[b].indegree&&a<b);});
      for(int j=0;j<(int)order.size();++j){int v=order[j];if(j<h){e.heads.push_back(v);vs[v].heads.push_back(ei);++vs[v].indegree;}else{e.tails.push_back(v);vs[v].tails.push_back(ei);}}}
  }
  vector<int> mine_impl(int k,const vector<char>*forced){
    auto started=chrono::steady_clock::now();auto t0=transfers_,g0=global_builds_;
    if(stats_)cerr<<"mine-start k="<<k<<"\n";
    seed.assign(vs.size(),false);for(int v:active)if(eligible[v])seed[v]=vs[v].indegree>=k||(forced&&(*forced)[v]);for(int v:active)if(eligible[v]&&seed[v])local_out(v,k);while(global_out(k)){}
    for(;;){bool changed=false;for(int v:active)if(eligible[v]&&seed[v]&&vs[v].indegree<k&&!(forced&&(*forced)[v])){repair(v,k);changed=true;}bool paths=false;while(global_out(k))paths=true;if(!changed&&!paths)break;}auto result=closure();
    if(stats_)cerr<<"mine-end k="<<k<<" size="<<result.size()<<" transfers="<<(transfers_-t0)<<" global-builds="<<(global_builds_-g0)<<" time="<<chrono::duration<double>(chrono::steady_clock::now()-started).count()<<"\n";
    return result;
  }
 public:
  vector<int> mine(int k){eligible.assign(vs.size(),false);for(int v:active)eligible[v]=true;return mine_impl(k,nullptr);}
  vector<int> mine_between(int k,const vector<int>&lower,const vector<int>&upper){eligible.assign(vs.size(),false);vector<char>forced(vs.size());for(int v:lower)eligible[v]=true;for(int v:upper)forced[v]=true;return mine_impl(k,&forced);}
};

using LayerPtr=shared_ptr<const vector<int>>;
struct DecompositionResult{map<pair<int,int>,LayerPtr>layers;int total=0,computed=0,computed_nonempty=0;};
inline DecompositionResult dsd(DecompositionEngine&e,int md){DecompositionResult r;for(int d=1;d<=md;++d){e.orient(d);r.layers[{d,1}]=make_shared<const vector<int>>(e.all_vertices());++r.total;for(int k=2;k<=e.maximum_k_bound()+1;++k){auto x=e.mine(k);++r.computed;if(x.empty())break;++r.computed_nonempty;r.layers[{d,k}]=make_shared<const vector<int>>(std::move(x));++r.total;}}return r;}
inline DecompositionResult dsd_plus(DecompositionEngine&e,int md){
  DecompositionResult r;for(int d=1;d<=md;++d){e.orient(d);map<int,LayerPtr>c;auto keep=[&](vector<int>x){++r.computed;if(!x.empty())++r.computed_nonempty;return make_shared<const vector<int>>(std::move(x));};auto full=[&](int k){return keep(e.mine(k));};c[1]=make_shared<const vector<int>>(e.all_vertices());int lo=1,hi=2,bound=e.maximum_k_bound();
    while(hi<=bound){c[hi]=full(hi);if(c[hi]->empty())break;lo=hi;hi*=2;}
    if(hi>bound){hi=bound+1;c[hi]=make_shared<const vector<int>>();}
    while(lo+1<hi){int m=(lo+hi)/2;if(!c.count(m))c[m]=full(m);if(c[m]->empty())hi=m;else lo=m;}if(!c.count(lo))c[lo]=full(lo);
    function<void(int,int)>go=[&](int l,int u){if(u-l<=1)return;if(*c[l]==*c[u]){for(int k=l+1;k<u;++k)c[k]=c[l];return;}int m=(l+u)/2;if(!c.count(m))c[m]=keep(e.mine_between(m,*c[l],*c[u]));go(l,m);go(m,u);};go(1,lo);
    for(int k=1;k<=lo;++k){if(!c.count(k))c[k]=keep(e.mine_between(k,*c[1],*c[lo]));r.layers[{d,k}]=c[k];++r.total;}}
  return r;
}
inline unsigned long long dhash(const vector<int>&x){unsigned long long h=1469598103934665603ULL;for(int v:x){h^=(unsigned)v;h*=1099511628211ULL;}return h;}
inline int run_decomposition_cli(int argc,char**argv,bool plus){
  if(argc<2){cerr<<"usage: "<<argv[0]<<" DATASET [MAX_DELTA] [--layers]\n";return 2;}try{DecompositionEngine e(argv[1]);int md=e.maximum_delta();if(argc>=3&&string(argv[2])!="--layers")md=min(md,stoi(argv[2]));bool print=string(argv[argc-1])=="--layers";auto begin=chrono::steady_clock::now();clock_t cpu_begin=clock();auto r=plus?dsd_plus(e,md):dsd(e,md);double wall=chrono::duration<double>(chrono::steady_clock::now()-begin).count(),cpu=double(clock()-cpu_begin)/CLOCKS_PER_SEC;if(print)for(auto&z:r.layers)cout<<z.first.first<<','<<z.first.second<<','<<z.second->size()<<','<<dhash(*z.second)<<'\n';cout<<"algorithm="<<(plus?"DSD+":"DSD")<<" deltas="<<md<<" layers="<<r.total<<" computed="<<r.computed<<" skipped="<<max(0,r.total-r.computed_nonempty)<<" cpu="<<cpu<<"seconds wall="<<wall<<"seconds\n";return 0;}catch(exception&e){cerr<<e.what()<<'\n';return 1;}}
#endif

#ifndef IDH_DSD_PLUS_ENTRY
int main(int argc, char **argv) { return run_decomposition_cli(argc, argv, false); }
#endif
