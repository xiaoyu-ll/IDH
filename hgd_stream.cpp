// Maintains one evolving orientation across multiple update operations.
// Usage: hgd_stream BASE DELTA EVENTS_FILE [VERTEX_COUNT]
// EVENTS_FILE: lines '+ 1,2,3' or '- 7'; deletion IDs are zero-based
// original/inserted edge numbers; IDs are stable even after deletion.
#include "verified/hgd_optimized.hpp"
int main(int argc,char**argv){
    using namespace hgdopt;
    try{
        if(argc<4||argc>5){
            std::cerr<<"Usage: "<<argv[0]<<" BASE DELTA EVENTS_FILE [VERTEX_COUNT]\n";
            return 2;
        }
        int d=parse_integer(argv[2],1,"delta");
        int n=argc>4?parse_integer(argv[4],0,"vertex count"):0;
        Orientation o(n,d);load_edges(o,argv[1]);
        long long calls=0;auto initial=bulk_equilibrate_cached(o,nullptr,&calls);
        std::cerr<<"[STREAM] initial_maxflow_calls="<<calls<<" initial_unit_transfers="<<initial<<"\n";
        std::ifstream f(argv[3]);if(!f)throw std::runtime_error("cannot open events file");
        std::string s;long long lineno=0,step=0;
        while(std::getline(f,s)){
            ++lineno;s=trim(s);if(s.empty()||s[0]=='#')continue;
            char op=s[0];if(op!='+'&&op!='-')throw std::invalid_argument("unknown update operation at line "+std::to_string(lineno));
            auto arg=trim(s.substr(1));
            std::vector<int> changed;
            if(op=='+'){
                auto e=parse_edge(arg);
                greedy_add(o,std::move(e));
                changed=o.edges.back().vertices;
            }else{
                size_t consumed=0;long long id=std::stoll(arg,&consumed);
                if(consumed!=arg.size()||id<0||id>=(long long)o.edges.size()||!o.edges[id].active)
                    throw std::invalid_argument("invalid/deleted edge ID at line "+std::to_string(lineno));
                changed=o.edges[id].vertices;
                o.remove_edge((int)id);
            }
            long long repair_calls=0;
            auto affected=touched_components(o,changed);
            auto transfers=bulk_equilibrate_cached(o,&affected,&repair_calls);
#ifdef HGD_REVERSE_REACH
            auto ids=reachable_max_reverse(o);
#else
            auto ids=o.reachable_max(o.build());
#endif
            ++step;
            for(int v=0;v<(int)ids.size();++v)
                std::cout<<step<<","<<op<<","<<v+1<<","<<ids[v]<<"\n";
            std::cerr<<"[STREAM] step="<<step<<" type="<<op
                     <<" affected_vertices="<<std::count(affected.begin(),affected.end(),1)
                     <<" maxflow_calls="<<repair_calls<<" transferred="<<transfers<<"\n";
        }
        return 0;
    }catch(std::exception const&e){
        std::cerr<<"[ERROR] "<<e.what()<<"\n";return 1;
    }
}
