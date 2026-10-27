// Independent differential test: reverse degree propagation must equal the
// full factor-graph SCC oracle for ARBITRARY legal orientations (not merely
// equilibrated states). Covers dead edges, singleton/duplicate records and zero vertices.
#include "../hgd_optimized.hpp"
#include <random>
#include <iostream>
int main(int argc, char**argv){
    const int rounds=argc>1?std::stoi(argv[1]):7000;
    std::mt19937 rng(20261019);
    long long checks=0;
    for(int c=0;c<rounds;++c){
        int n=(int)(rng()%15),delta=1+(int)(rng()%19),m=(int)(rng()%35);
        Orientation o(n,delta);
        for(int i=0;i<m && n>0;++i){
            int size=1+(int)(rng()%std::min(n,11));
            std::vector<int> edge;
            for(int j=0;j<size;++j)edge.push_back((int)(rng()%n));
            if(!edge.empty() && rng()%3==0)edge.push_back(edge[0]);
            o.add_edge(edge);
        }
        // Reorient each edge to random heads; deltas may exceed edge size.
        std::fill(o.indeg.begin(),o.indeg.end(),0);
        for(auto &e:o.edges){
            int r=std::min(o.delta,(int)e.vertices.size());
            std::vector<int> ids((int)e.vertices.size());
            std::iota(ids.begin(),ids.end(),0);
            std::shuffle(ids.begin(),ids.end(),rng);
            std::fill(e.is_head.begin(),e.is_head.end(),0);
            for(int j=0;j<r;++j){int p=ids[j];e.is_head[p]=1;++o.indeg[e.vertices[p]];}
        }
        auto validate=[&](){
            o.validate();
            auto reference=o.reachable_max(o.build());
            auto reverse=hgdopt::reachable_max_reverse(o);
            if(reference!=reverse){
                std::cerr<<"Mismatch at case "<<c<<" n="<<o.n<<" delta="<<o.delta<<'\n';
                std::exit(1);
            }
            ++checks;
        };
        validate();
        if(!o.edges.empty()){
            int victim=(int)(rng()%o.edges.size());
            o.remove_edge(victim);
            validate();
        }
        // Change orientation to threshold-stable, not necessarily egalitarian.
        if(!o.edges.empty()){
            int k=(int)(rng()%14);
            hgdopt::threshold_flow(o,k);
            validate();
        }
    }
    std::cout<<"PASS reverse reachability vs independent SCC: "<<checks
             <<" checks over "<<rounds<<" random instances\n";
}
