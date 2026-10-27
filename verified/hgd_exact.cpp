// Certified reference algorithms for hypergraph (k, delta)-dense decomposition.
// Correctness oriented, NOT benchmark-equivalent to legacy DSM-ALL / DSD+.
// Build: g++ -std=c++17 -O2 -Wall -Wextra -pedantic hgd_exact.cpp -o hgd_exact
// Vertex IDs are 1-based in input/output. Each data line is a hyperedge, comma separated.
// Multiple equal hyperedges are distinct occurrences; repetitions within one edge are deduplicated.
// Usage:
//   hgd_exact all INPUT DELTA [VERTEX_COUNT]
//   hgd_exact query INPUT DELTA K [VERTEX_COUNT]
//   hgd_exact insert INPUT DELTA BATCH_EDGE_FILE [VERTEX_COUNT]
//   hgd_exact delete INPUT DELTA BATCH_ID_FILE [VERTEX_COUNT]
// Output for all/updates: v,idn per line. Delete indices are 0-based original input-line IDs.
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <queue>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
using namespace std;

// Index-bearing containers use int IDs. Check arithmetic before allocation/casts.
static int checked_node_count(int vertices, size_t factors, int terminals=0){
    if(vertices<0 || terminals<0 ||
       factors>static_cast<size_t>(numeric_limits<int>::max()-vertices) ||
       terminals>numeric_limits<int>::max()-vertices-static_cast<int>(factors))
        throw overflow_error("graph node count exceeds int index range");
    return vertices+static_cast<int>(factors)+terminals;
}

struct EdgeRec {
    vector<int> vertices;
    vector<uint8_t> is_head;
    bool active=true;
};

struct Digraph {
    vector<vector<int>> adj, rev;
    explicit Digraph(int n):adj(n),rev(n){}
    void arc(int u,int v){adj[u].push_back(v);rev[v].push_back(u);}
};

class Orientation {
public:
    int n=0, delta=1;
    vector<EdgeRec> edges;
    vector<int> indeg;
    explicit Orientation(int n_, int d):n(n_),delta(d),indeg(n_>=0?n_:0,0){
        if(n<0 || delta<1) throw invalid_argument("bad vertex count or delta");
    }
    void resize_vertices(int nnew){
        if(nnew>n){indeg.resize(nnew,0);n=nnew;}
    }
    void add_edge(vector<int> e){
        if(edges.size()>=static_cast<size_t>(numeric_limits<int>::max()-1))
            throw overflow_error("edge occurrence ID limit reached");
        if(e.empty()) throw invalid_argument("empty hyperedge");
        for(int v:e) if(v<0 || v==numeric_limits<int>::max())
            throw invalid_argument("invalid vertex ID");
        sort(e.begin(),e.end());
        e.erase(unique(e.begin(),e.end()),e.end());
        // erase does not release a raw record's duplicate-heavy allocation.
        if(e.capacity()>2*e.size())vector<int>(e).swap(e);
        resize_vertices(e.back()+1);
        EdgeRec rec; rec.vertices=std::move(e);rec.is_head.assign(rec.vertices.size(),0);
        const int r=min(delta,static_cast<int>(rec.vertices.size()));
        for(int j=0;j<r;++j){rec.is_head[j]=1;++indeg[rec.vertices[j]];}
        edges.push_back(std::move(rec));
    }
    void remove_edge(int i){
        if(i<0 || i>=static_cast<int>(edges.size()) || !edges[i].active)
            throw invalid_argument("invalid or already deleted original edge ID");
        auto &e=edges[i];
        for(size_t j=0;j<e.vertices.size();++j)
            if(e.is_head[j])--indeg[e.vertices[j]];
        e.active=false;
        // Keep the occurrence slot/ID, but do not retain historical incidences.
        // Update callers must record affected endpoints before deletion.
        vector<int>().swap(e.vertices);
        vector<uint8_t>().swap(e.is_head);
    }
    __int128 potential()const{
        __int128 sum=0;
        for(int d:indeg)sum+=(__int128)d*d;
        return sum;
    }
    long long R()const{
        long long cnt=0;
        for(const auto &e:edges)if(e.active)cnt+=min(delta,(int)e.vertices.size());
        return cnt;
    }
    void validate()const{
        vector<int> count(n,0);
        for(const auto&e:edges){
            if(!e.active) continue;
            int heads=0;
            if(e.vertices.size()!=e.is_head.size()) throw logic_error("head bitmap size");
            for(size_t j=0;j<e.vertices.size();++j) if(e.is_head[j]){
                ++count[e.vertices[j]];++heads;
            }
            if(heads!=min(delta,(int)e.vertices.size()))throw logic_error("head capacity violated");
        }
        if(count!=indeg)throw logic_error("indegree invariant violated");
    }
    Digraph build()const{
        Digraph dg(checked_node_count(n,edges.size()));
        for(size_t i=0;i<edges.size();++i){
            const auto &e=edges[i];if(!e.active)continue;
            int f=n+(int)i;
            for(size_t j=0;j<e.vertices.size();++j){
                int v=e.vertices[j];
                if(e.is_head[j])dg.arc(f,v);
                else dg.arc(v,f);
            }
        }
        return dg;
    }
    // Both vertex/factor nodes on a BFS path are unique. Returns the path as vertices/factors.
    vector<int> bfs(const Digraph &g,const vector<int> &starts,int target_min){
        vector<int> parent(g.adj.size(),-1);
        queue<int> q;
        for(int s:starts){parent[s]=s;q.push(s);}
        int found=-1;
        while(!q.empty()&&found<0){
            int u=q.front();q.pop();
            if(u<n && indeg[u]>=target_min && parent[u]!=u){found=u;break;}
            for(int v:g.adj[u]) if(parent[v]==-1){parent[v]=u;q.push(v);}
        }
        if(found<0)return {};
        vector<int> path;
        for(int v=found;;v=parent[v]){
            path.push_back(v);
            if(parent[v]==v)break;
        }
        reverse(path.begin(),path.end());
        return path;
    }
    void swap_path(const vector<int>& p){
        if(p.size()<3||!(p.size()%2)||p.front()>=n||p.back()>=n)
            throw logic_error("invalid alternating path");
        const auto before=potential();
        for(size_t i=1;i+1<p.size();i+=2){
            int u=p[i-1], ei=p[i]-n,v=p[i+1];
            if(ei<0 || ei>=static_cast<int>(edges.size()))throw logic_error("bad factor");
            auto &e=edges[ei];
            auto it_u=lower_bound(e.vertices.begin(),e.vertices.end(),u);
            auto it_v=lower_bound(e.vertices.begin(),e.vertices.end(),v);
            if(it_u==e.vertices.end()||*it_u!=u||it_v==e.vertices.end()||*it_v!=v)
                throw logic_error("edge/path mismatch");
            int iu=(int)(it_u-e.vertices.begin()),iv=(int)(it_v-e.vertices.begin());
            if(e.is_head[iu] || !e.is_head[iv] || !e.active)
                throw logic_error("invalid source/head swap");
            e.is_head[iu]=1;e.is_head[iv]=0;
            ++indeg[u];--indeg[v];
        }
        if(potential()>=before)throw logic_error("non-reversible path swapped");
    }
    // For arbitrary orientation, max indegree of reachable original vertices.
    vector<int> reachable_max(const Digraph &g)const{
        int N=(int)g.adj.size();
        vector<uint8_t> used(N,0);
        vector<int> order;order.reserve(N);
        for(int start=0;start<N;++start)if(!used[start]){
            used[start]=1;
            vector<pair<int,size_t>> st{{start,0}};
            while(!st.empty()){
                int u=st.back().first;
                size_t &j=st.back().second;
                if(j<g.adj[u].size()){
                    int v=g.adj[u][j++];
                    if(!used[v]){used[v]=1;st.emplace_back(v,0);}
                }else{order.push_back(u);st.pop_back();}
            }
        }
        vector<int> comp(N,-1);
        int nc=0;
        for(auto it=order.rbegin();it!=order.rend();++it){
            int s=*it;if(comp[s]>=0)continue;
            vector<int> st{s};comp[s]=nc;
            while(!st.empty()){
                int u=st.back();st.pop_back();
                for(int v:g.rev[u])if(comp[v]<0){comp[v]=nc;st.push_back(v);}
            }
            ++nc;
        }
        vector<int> best(nc,-1);
        vector<size_t> degin(nc,0); // Parallel condensation arcs can exceed int.
        vector<vector<int>> dag(nc);
        for(int v=0;v<n;++v)best[comp[v]]=max(best[comp[v]],indeg[v]);
        for(int u=0;u<N;++u)for(int v:g.adj[u])if(comp[u]!=comp[v]){
            dag[comp[u]].push_back(comp[v]);++degin[comp[v]];
        }
        queue<int> q;for(int c=0;c<nc;++c)if(!degin[c])q.push(c);
        vector<int> topo;topo.reserve(nc);
        while(!q.empty()){
            int c=q.front();q.pop();topo.push_back(c);
            for(int v:dag[c])if(--degin[v]==0)q.push(v);
        }
        if((int)topo.size()!=nc)throw logic_error("SCC condensation cycle");
        for(auto it=topo.rbegin();it!=topo.rend();++it)
            for(int v:dag[*it])best[*it]=max(best[*it],best[v]);
        vector<int> idn(n);
        for(int v=0;v<n;++v)idn[v]=best[comp[v]];
        return idn;
    }
    long long equilibrate(){
        long long swaps=0;
        // Each reversible path strictly decreases sum indegree squared.
        while(true){
            Digraph g=build();
            vector<int> reach=reachable_max(g);
            int s=-1;
            for(int v=0;v<n;++v)if(reach[v]>=indeg[v]+2){s=v;break;}
            if(s<0)break;
            auto p=bfs(g,vector<int>{s},indeg[s]+2);
            if(p.empty())throw logic_error("reachability witness not reconstructed");
            swap_path(p);++swaps;
        }
        validate();return swaps;
    }
    vector<int> full_idn(){
        equilibrate();return reachable_max(build());
    }
    // Correct canonical D_k without requiring globally egalitarian orientation.
    pair<vector<int>,long long> threshold_query(int k){
        if(k<0)throw invalid_argument("k must be nonnegative");
        long long rounds=0,Rbound=R();
        while(true){
            vector<int> low;
            for(int v=0;v<n;++v)if(indeg[v]<=k-2)low.push_back(v);
            if(low.empty())break;
            Digraph g=build();
            auto p=bfs(g,low,k);
            if(p.empty())break;
            swap_path(p);
            if(++rounds>Rbound)throw logic_error("threshold potential bound violated");
        }
        auto reach=reachable_max(build());
        vector<int> ret;
        for(int v=0;v<n;++v)if(reach[v]>=k)ret.push_back(v);
        validate();return {ret,rounds};
    }
};

static string trim(string x){
    auto first=x.find_first_not_of(" \t\r\n");
    if(first==string::npos)return "";
    auto last=x.find_last_not_of(" \t\r\n");
    return x.substr(first,last-first+1);
}
static int parse_integer(const string &text, int minimum, const string &name){
    size_t used=0;
    int value=stoi(text,&used);
    if(used!=text.size() || value<minimum)
        throw invalid_argument(name+" is outside its integer domain");
    return value;
}
static vector<int> parse_edge(const string &line){
    vector<int> out;
    string word;
    for(size_t i=0;i<=line.size();++i){
        char c=(i==line.size()?',':line[i]);
        if(c==','||isspace((unsigned char)c)){
            if(word.empty())continue;
            size_t used=0;
            long long v=stoll(word,&used);
            if(used!=word.size()||v<1||v>numeric_limits<int>::max())
                throw invalid_argument("vertex IDs must be positive integers");
            out.push_back((int)v-1);
            word.clear();
        }else word+=c;
    }
    if(out.empty())throw invalid_argument("empty hyperedge input line");
    return out;
}
static void load_edges(Orientation &o,const string &file){
    ifstream fin(file);
    if(!fin)throw runtime_error("cannot open "+file);
    string s;int line=0;
    while(getline(fin,s)){
        ++line;s=trim(s);
        if(s.empty()||s[0]=='#')continue;
        try{o.add_edge(parse_edge(s));}
        catch(const exception& e){throw runtime_error(file+":"+to_string(line)+": "+e.what());}
    }
}
static vector<int> load_deleted_ids(const string &file){
    ifstream f(file);if(!f)throw runtime_error("cannot open "+file);
    vector<int> res;string line;
    while(getline(f,line)){
        line=trim(line);if(line.empty()||line[0]=='#')continue;
        auto comma=line.find(',');if(comma!=string::npos)line=trim(line.substr(0,comma));
        size_t used=0;long long idx=stoll(line,&used);
        if(used!=line.size()||idx<0||idx>numeric_limits<int>::max())throw invalid_argument("bad deleted edge ID");
        res.push_back((int)idx);
    }
    return res;
}
int main(int argc,char **argv){
    try{
        if(argc<4){
            cerr<<"Usage: hgd_exact all INPUT DELTA [VERTEX_COUNT] | query INPUT DELTA K [VERTEX_COUNT] | insert INPUT DELTA BATCH_EDGE_FILE [VERTEX_COUNT] | delete INPUT DELTA BATCH_ID_FILE [VERTEX_COUNT]\n";
            return 2;
        }
        string mode=argv[1],path=argv[2];
        int delta=parse_integer(argv[3],1,"delta");
        int argn=(mode=="query"||mode=="insert"||mode=="delete")?6:5;
        if((mode=="query"||mode=="insert"||mode=="delete")&&argc<5)throw invalid_argument("missing mode parameter");
        int extra_vertices=0;
        if(argc>argn)throw invalid_argument("too many arguments");
        if(argc>=argn)extra_vertices=parse_integer(argv[argn-1],0,"vertex count");
        Orientation hg(extra_vertices,delta);
        load_edges(hg,path);
        if(mode=="all"){
            auto idn=hg.full_idn();
            for(int v=0;v<hg.n;++v)cout<<v+1<<","<<idn[v]<<"\n";
        }else if(mode=="query"){
            int k=parse_integer(argv[4],0,"k");
            auto [found,swaps]=hg.threshold_query(k);
            for(int v:found)cout<<v+1<<"\n";
            cerr<<"[certified] reversible swaps="<<swaps<<"\n";
        }else if(mode=="insert"){
            // The warm start first balances the existing orientation, then inserts the batch.
            hg.equilibrate();
            load_edges(hg,argv[4]);
            auto idn=hg.full_idn();
            for(int v=0;v<hg.n;++v)cout<<v+1<<","<<idn[v]<<"\n";
        }else if(mode=="delete"){
            hg.equilibrate();
            for(int id:load_deleted_ids(argv[4]))hg.remove_edge(id);
            auto idn=hg.full_idn();
            for(int v=0;v<hg.n;++v)cout<<v+1<<","<<idn[v]<<"\n";
        }else throw invalid_argument("unknown mode: "+mode);
        return 0;
    }catch(const exception &e){
        cerr<<"[ERROR] "<<e.what()<<"\n";
        return 1;
    }
}
