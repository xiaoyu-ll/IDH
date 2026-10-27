// Exact peeling implementations of four comparison models.
// INPUT core|neighbor|cocore|abcore PARAM VERTEX_COUNT; output v,score.
// Strong models delete a whole hyperedge at its first removed endpoint.
// abcore retains a truncated hyperedge while its surviving size >= PARAM.
#include "hgd_optimized.hpp"
#include <unordered_map>
#include <functional>

int main(int argc, char **argv) {
    using namespace hgdopt;
    try {
        if (argc != 5) throw std::invalid_argument("INPUT MODEL PARAM N");
        std::string mode=argv[2];
        if(mode!="core" && mode!="neighbor" && mode!="cocore" && mode!="abcore")
            throw std::invalid_argument("unknown model");
        int param=parse_integer(argv[3],1,"parameter");
        Orientation o(parse_integer(argv[4],0,"n"),1);
        load_edges(o,argv[1]);
        int n=o.n;
        bool pairs=(mode=="neighbor" || mode=="cocore");
        std::vector<std::vector<int>> incidence(n);
        std::vector<std::unordered_map<int,int>> count(pairs?n:0);
        std::vector<int> degree(n), neighbors(n), remaining(o.edges.size()), score(n);
        std::vector<uint8_t> alive(n,1), active(o.edges.size(),1);
        for(int e=0;e<(int)o.edges.size();++e){
            auto const& vs=o.edges[e].vertices;
            remaining[e]=(int)vs.size();
            if(mode=="abcore" && remaining[e]<param){active[e]=0;continue;}
            for(int v:vs){incidence[v].push_back(e);++degree[v];}
            if(pairs)for(int v:vs)for(int u:vs)if(v!=u)++count[v][u];
        }
        if(pairs)for(int v=0;v<n;++v)neighbors[v]=(int)count[v].size();
        using Item=std::pair<int,int>;
        std::priority_queue<Item,std::vector<Item>,std::greater<Item>> heap;
        std::queue<int> forced;
        auto key=[&](int v){return mode=="neighbor"?neighbors[v]:degree[v];};
        for(int v=0;v<n;++v){heap.push({key(v),v});if(mode=="cocore" && neighbors[v]<param)forced.push(v);}
        int level=0;
        while(!heap.empty() || !forced.empty()){
            int v;
            if(!forced.empty()){v=forced.front();forced.pop();if(!alive[v])continue;}
            else{
                auto [d,u]=heap.top();heap.pop();
                if(!alive[u] || d!=key(u))continue;
                level=std::max(level,d);v=u;
            }
            alive[v]=0;score[v]=level;
            for(int e:incidence[v])if(active[e]){
                if(mode=="abcore" && --remaining[e]>=param)continue;
                active[e]=0;
                auto const& vs=o.edges[e].vertices;
                if(pairs)for(int u:vs)if(alive[u]){
                    for(int w:vs)if(w!=u){
                        auto it=count[u].find(w);
                        if(it==count[u].end())throw std::logic_error("missing pair multiplicity");
                        if(--it->second==0){count[u].erase(it);--neighbors[u];}
                    }
                }
                for(int u:vs)if(alive[u]){
                    --degree[u];heap.push({key(u),u});
                    if(mode=="cocore" && neighbors[u]<param)forced.push(u);
                }
            }
        }
        for(int v=0;v<n;++v)std::cout<<v+1<<','<<score[v]<<'\n';
    }catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}
}
