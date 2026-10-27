// Common strong-induced density evaluation and exact weighted closure reference.
// profile INPUT SCORES N; exact INPUT ratio|degree N; peel INPUT neighbor N.
#include "hgd_optimized.hpp"
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <numeric>
#include <iomanip>

using Graph=::Orientation;
using ll=long long;
using Pair=std::pair<ll,int>;
static uint64_t pair_key(int u,int v){if(u>v)std::swap(u,v);return (uint64_t(u)<<32)|uint32_t(v);}

struct Snapshot {int k=0,n=0,m=0;ll weight=0,pairs=0;};
static double value(Snapshot const&s,int metric){return s.n?double(metric==0?s.m:metric==1?s.weight:2*s.pairs)/s.n:0.;}
static void vertices(std::vector<int>const&vs){std::cout<<'[';for(size_t i=0;i<vs.size();++i){if(i)std::cout<<',';std::cout<<vs[i]+1;}std::cout<<']';}
static void record(Snapshot const&s){std::cout<<"{\"threshold\":"<<s.k<<",\"vertices\":"<<s.n<<",\"edges\":"<<s.m<<",\"weight\":"<<s.weight<<",\"pairs\":"<<s.pairs<<'}';}
static void profile(Graph const&o,std::vector<int>const&score){
    std::map<int,std::vector<int>,std::greater<int>> groups,edgegroups;
    for(int v=0;v<o.n;++v)groups[score[v]].push_back(v);
    for(int i=0;i<(int)o.edges.size();++i){int k=std::numeric_limits<int>::max();for(int v:o.edges[i].vertices)k=std::min(k,score[v]);edgegroups[k].push_back(i);}
    std::unordered_set<uint64_t> pairs;
    std::vector<int>selected;std::vector<Snapshot> curve;
    std::vector<std::vector<int>> witness(3);double best[3]={-1,-1,-1};
    Snapshot s;
    for(auto const&group:groups){
        s.k=group.first;selected.insert(selected.end(),group.second.begin(),group.second.end());s.n=(int)selected.size();
        for(int e:edgegroups[s.k]){
            auto const&vs=o.edges[e].vertices;++s.m;s.weight+=(ll)vs.size();
            for(size_t i=0;i<vs.size();++i)for(size_t j=i+1;j<vs.size();++j)pairs.insert(pair_key(vs[i],vs[j]));
        }
        s.pairs=(ll)pairs.size();curve.push_back(s);
        for(int metric=0;metric<3;++metric)if(value(s,metric)>best[metric]){best[metric]=value(s,metric);witness[metric]=selected;}
    }
    std::cout<<"{\"curve\":[";for(size_t i=0;i<curve.size();++i){if(i)std::cout<<',';record(curve[i]);}
    std::cout<<"],\"best\":[";
    for(int i=0;i<3;++i){if(i)std::cout<<',';std::cout<<"{\"density\":"<<std::max(0.,best[i])<<",\"selected\":";vertices(witness[i]);std::cout<<'}';}
    std::cout<<"]}\n";
}

// 64-bit iterative Dinic, separate from the timed decomposition implementation.
struct Flow {
    struct Arc{int to,rev;ll cap;};
    std::vector<std::vector<Arc>>g;std::vector<int>level,it;
    explicit Flow(int n):g(n),level(n),it(n){}
    void add(int u,int v,ll c){int a=(int)g[u].size(),b=(int)g[v].size();g[u].push_back({v,b,c});g[v].push_back({u,a,0});}
    bool bfs(int s,int t){std::fill(level.begin(),level.end(),-1);std::queue<int>q;q.push(s);level[s]=0;while(!q.empty()){int u=q.front();q.pop();for(auto const&a:g[u])if(a.cap && level[a.to]<0){level[a.to]=level[u]+1;q.push(a.to);}}return level[t]>=0;}
    ll send(int s,int t){
        std::vector<int>path{s},chosen;std::vector<ll>low{std::numeric_limits<ll>::max()};
        while(!path.empty()){
            int u=path.back();
            if(u==t){ll f=low.back();for(size_t j=0;j<chosen.size();++j){auto &a=g[path[j]][chosen[j]];a.cap-=f;g[a.to][a.rev].cap+=f;}return f;}
            int &i=it[u];while(i<(int)g[u].size() && (!g[u][i].cap || level[g[u][i].to]!=level[u]+1))++i;
            if(i==(int)g[u].size()){level[u]=-1;path.pop_back();low.pop_back();if(!chosen.empty()){chosen.pop_back();if(!path.empty())++it[path.back()];}}
            else{auto const&a=g[u][i];chosen.push_back(i);path.push_back(a.to);low.push_back(std::min(low.back(),a.cap));}
        }return 0;
    }
    ll maxflow(int s,int t){ll f=0;while(bfs(s,t)){std::fill(it.begin(),it.end(),0);while(ll x=send(s,t))f+=x;}return f;}
};
static ll checked(__int128 v){if(v<0 || v>std::numeric_limits<ll>::max())throw std::overflow_error("density closure capacity overflow");return (ll)v;}
static void exact(Graph const&o,bool weighted){
    ll total=0;for(auto const&e:o.edges)total+=weighted?(ll)e.vertices.size():1;
    ll a=0,b=1,best_weight=0;int rounds=0;std::vector<int>best;
    if(!total){for(int v=0;v<o.n;++v)best.push_back(v);}
    while(total){
        int source=checked_node_count(o.n,o.edges.size(),0),sink=checked_node_count(o.n,o.edges.size(),1);
        Flow net(checked_node_count(o.n,o.edges.size(),2));
        ll budget=checked((__int128)b*total),infinite=checked((__int128)budget+1);
        for(int v=0;v<o.n;++v)net.add(v,sink,a);
        for(int i=0;i<(int)o.edges.size();++i){int factor=o.n+i;auto const&e=o.edges[i];net.add(source,factor,checked((__int128)b*(weighted?(ll)e.vertices.size():1)));for(int v:e.vertices)net.add(factor,v,infinite);}
        ll flow=net.maxflow(source,sink);++rounds;
        if(flow==budget)break;
        if(net.bfs(source,sink))throw std::logic_error("unfinished maxflow");
        std::vector<int>selected;for(int v=0;v<o.n;++v)if(net.level[v]>=0)selected.push_back(v);
        std::vector<uint8_t>in(o.n);for(int v:selected)in[v]=1;
        ll w=0;for(auto const&e:o.edges){bool all=true;for(int v:e.vertices)if(!in[v]){all=false;break;}if(all)w+=weighted?(ll)e.vertices.size():1;}
        if(selected.empty() || (__int128)b*w<=(__int128)a*selected.size())throw std::logic_error("closure did not improve ratio");
        best=std::move(selected);best_weight=w;ll gcd=std::gcd(w,(ll)best.size());a=w/gcd;b=(ll)best.size()/gcd;
    }
    std::cout<<"{\"density\":"<<(best.empty()?0.:double(best_weight)/best.size())<<",\"numerator\":"<<best_weight<<",\"denominator\":"<<best.size()<<",\"rounds\":"<<rounds<<",\"optimality_certificate\":true,\"selected\":";vertices(best);std::cout<<"}\n";
}

static void peel(Graph const&o){
    int n=o.n;std::vector<std::vector<int>>inc(n);std::vector<std::unordered_map<int,int>>counts(n);
    std::vector<int>degree(n),neighbors(n),order(n);std::vector<uint8_t>alive(n,1),active(o.edges.size(),1);
    ll pair_count=0,weight=0;int edge_count=(int)o.edges.size();
    for(int e=0;e<edge_count;++e){auto const&vs=o.edges[e].vertices;weight+=(ll)vs.size();for(int v:vs){inc[v].push_back(e);++degree[v];for(int u:vs)if(u!=v)++counts[v][u];}}
    std::priority_queue<Pair,std::vector<Pair>,std::greater<Pair>>heap;
    for(int v=0;v<n;++v){neighbors[v]=(int)counts[v].size();pair_count+=neighbors[v];heap.push({neighbors[v],v});}
    pair_count/=2;double best=-1;std::vector<int>bestset;int remain=n;
    while(remain){
        double density=2.*pair_count/remain;
        if(density>best){best=density;bestset.clear();for(int v=0;v<n;++v)if(alive[v])bestset.push_back(v);}
        int v;while(true){auto item=heap.top();heap.pop();v=item.second;if(alive[v] && item.first==neighbors[v])break;}
        alive[v]=0;--remain;
        for(int e:inc[v])if(active[e]){
            active[e]=0;--edge_count;auto const&vs=o.edges[e].vertices;weight-=(ll)vs.size();
            // All endpoints were alive before this first deletion. Remove both
            // ordered multiplicities, including the removed endpoint's copy.
            for(int u:vs)for(int w:vs)if(u!=w){auto it=counts[u].find(w);if(it==counts[u].end())throw std::logic_error("missing neighbor support");if(--it->second==0){counts[u].erase(it);--neighbors[u];if(u<w)--pair_count;}}
            for(int u:vs)if(alive[u]){--degree[u];heap.push({neighbors[u],u});}
        }
    }
    std::cout<<"{\"density\":"<<std::max(0.,best)<<",\"optimality_certificate\":false,\"selected\":";vertices(bestset);std::cout<<"}\n";
}

int main(int argc,char**argv){
    using namespace hgdopt;
    try{
        if(argc!=5)throw std::invalid_argument("profile INPUT SCORES N | exact INPUT ratio|degree N | peel INPUT neighbor N");
        std::string mode=argv[1];Graph o(parse_integer(argv[4],0,"n"),1);load_edges(o,argv[2]);std::cout<<std::setprecision(17);
        if(mode=="profile"){
            std::ifstream f(argv[3]);if(!f)throw std::invalid_argument("cannot read scores");std::vector<int>score;std::string line;
            while(std::getline(f,line)){auto p=line.find(',');if(p==std::string::npos || parse_integer(line.substr(0,p),1,"vertex")!=(int)score.size()+1)throw std::invalid_argument("score rows must cover 1..N");score.push_back(parse_integer(line.substr(p+1),0,"score"));}
            if(score.size()!=(size_t)o.n)throw std::invalid_argument("score length differs from vertex universe");profile(o,score);
        }else if(mode=="exact"){
            std::string metric=argv[3];if(metric!="ratio" && metric!="degree")throw std::invalid_argument("unknown exact metric");exact(o,metric=="degree");
        }else if(mode=="peel" && std::string(argv[3])=="neighbor")peel(o);
        else throw std::invalid_argument("unknown density mode");
    }catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}
}
