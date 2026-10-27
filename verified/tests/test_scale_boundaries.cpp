#include "../hgd_optimized.hpp"
#include <climits>
using namespace hgdopt;
static void require(bool value){if(!value)throw std::logic_error("scale boundary regression");}
int main(){
    for(bool greedy:{false,true}){
        Orientation duplicates(1,1);
        std::vector<int> raw(100000,0);
        if(greedy)greedy_add(duplicates,std::move(raw));else duplicates.add_edge(std::move(raw));
        require(duplicates.edges[0].vertices.size()==1);
        require(duplicates.edges[0].vertices.capacity()<=2);
        require(duplicates.indeg[0]==1);
    }
    require(balancing_threshold(INT_MAX-2,INT_MAX)==INT_MAX);
    require(balancing_threshold(0,INT_MAX)==1073741824);
    require(balancing_threshold(1500000000,2000000000)==1750000000);
    require(checked_node_count(INT_MAX-3,1,2)==INT_MAX);
    {
        Orientation tiny(2,1);tiny.add_edge({0,1});
        std::vector<uint8_t> wrong(1,1);
        for(bool cached:{false,true}){
            bool rejected=false;
            try{if(cached)bulk_equilibrate_cached(tiny,&wrong);else bulk_equilibrate(tiny,&wrong);}
            catch(const std::invalid_argument&){rejected=true;}
            require(rejected);
        }
    }
    for(auto factors:{size_t(2),std::numeric_limits<size_t>::max()}){
        bool rejected=false;
        try{checked_node_count(INT_MAX-3,factors,2);}catch(const std::overflow_error&){rejected=true;}
        require(rejected);
    }
    // A long directed chain must not consume the C++ call stack in SCC or flow.
    const int n=100000;
    Orientation chain(n,1);
    for(int v=0;v+1<n;++v)chain.add_edge({v,v+1});
    auto scc=chain.reachable_max(chain.build());
    require(scc==reachable_max_reverse(chain));
    for(int level:scc)require(level==1);
    {
        MaxFlow flow(n);
        for(int v=0;v+1<n;++v)flow.add(v,v+1,1);
        require(flow.run(0,n-1)==1);
        require(flow.run(0,n-1)==0);
    }
    long long calls=-1;
    require(bulk_equilibrate_cached(chain,nullptr,&calls)==0&&calls==0);
    // Repeated large forced-edge insert/delete: retained payload must be zero,
    // while occurrence IDs remain stable and never alias a deleted edge.
    Orientation churn(512,512);
    std::vector<int> endpoints(512);std::iota(endpoints.begin(),endpoints.end(),0);
    for(int step=0;step<20000;++step){
        greedy_add(churn,endpoints);
        require(churn.edges.size()==static_cast<size_t>(step)+1);
        churn.remove_edge(step);
        require(!churn.edges[step].active);
        require(churn.edges[step].vertices.capacity()==0&&churn.edges[step].is_head.capacity()==0);
        if(step%1000==0){
            calls=-1;require(bulk_equilibrate_cached(churn,nullptr,&calls)==0&&calls==0);
            for(int level:reachable_max_reverse(churn))require(level==0);
            churn.validate();
        }
    }
    size_t payload=0;
    for(const auto&e:churn.edges)payload+=e.vertices.capacity()*sizeof(int)+e.is_head.capacity();
    require(payload==0);
    std::cout<<"CHURN: historical_incidence_payload=51200000 bytes; retained_deleted_payload="
             <<payload<<" bytes; slot_capacity_bytes="<<churn.edges.capacity()*sizeof(EdgeRec)<<'\n';
    bool rejected=false;try{churn.remove_edge(0);}catch(const std::invalid_argument&){rejected=true;}
    require(rejected);
    churn.add_edge({0,511});
    require(churn.edges.size()==20001&&churn.edges.back().active);
    auto ids=reachable_max_reverse(churn);
    require(ids[0]==1&&ids[511]==1&&ids[1]==0);
    // Many explicit isolates are preserved in outputs and touched masks.
    Orientation isolates(250000,9);isolates.add_edge({0,249999});
    auto mask=touched_components(isolates,{0});
    require(std::count(mask.begin(),mask.end(),1)==2);
    calls=-1;require(bulk_equilibrate_cached(isolates,&mask,&calls)==0&&calls==0);
    auto levels=reachable_max_reverse(isolates);
    require(levels.size()==250000&&levels.front()==1&&levels.back()==1);
    require(std::count(levels.begin(),levels.end(),0)==249998);
    std::cout<<"PASS: 100000-node iterative chain/flow, 20000 large-edge churn cycles, 250000 vertices, integer limits\n";
}
