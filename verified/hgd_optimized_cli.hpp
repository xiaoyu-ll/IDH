#pragma once
#ifndef HGD_OPT_MODE
#error HGD_OPT_MODE is required
#endif
#include "hgd_optimized.hpp"

int main(int argc,char **argv){
    using namespace hgdopt;
    try{
        std::string mode=HGD_OPT_MODE;
        int shift=0;
        if(argc>=2){
            std::string f=argv[1];
            if(f=="query"||f=="all"||f=="insert"||f=="delete"){
                if((mode=="PATH"||mode=="DSMALL"||mode=="FLOW"||mode=="FLOWPLUS") && f!="query")
                    throw std::invalid_argument("this executable only supports query");
                if((mode=="DSD"||mode=="DSDPLUS"||mode=="PARA") && f!="all")
                    throw std::invalid_argument("this executable only supports all");
                if((mode=="INSERT"||mode=="INSERT_FULL") && f!="insert")
                    throw std::invalid_argument("this executable only supports insert");
                if((mode=="DELETE"||mode=="DELETE_FULL") && f!="delete")
                    throw std::invalid_argument("this executable only supports delete");
                shift=1;
            }
        }
        const bool query=(mode=="PATH"||mode=="DSMALL"||mode=="FLOW"||mode=="FLOWPLUS");
        const bool updated=(mode=="INSERT"||mode=="DELETE"||mode=="INSERT_FULL"||mode=="DELETE_FULL");
        if(argc<3+shift+(query||updated?1:0)){
            std::cerr<<"Usage: "<<argv[0]<<" [query|all|insert|delete] INPUT DELTA"
                     <<(query?" K":(updated?" BATCH_FILE":""))<<" [VERTEX_COUNT]\n";
            return 2;
        }
        const std::string file=argv[1+shift];
        const int delta=parse_integer(argv[2+shift],1,"delta");
        int total=0;
        int arg=3+shift+(query||updated?1:0);
        if(argc>arg+1)throw std::invalid_argument("too many arguments");
        if(argc>arg) total=parse_integer(argv[arg],0,"vertex count");
        Orientation o(total,delta);
        if(mode=="FLOWPLUS")load_greedy(o,file);
        else load_edges(o,file);
        if(query){
            int k=parse_integer(argv[3+shift],0,"k");
            std::vector<int>found;
            if(mode=="PATH"){
                long long moves=o.equilibrate();
                auto reach=o.reachable_max(o.build());
                for(int v=0;v<o.n;++v)if(reach[v]>=k)found.push_back(v);
                std::cerr<<"[PATH] global_egalitarian_reversals="<<moves<<"\n";
            }else if(mode=="DSMALL"){
                auto x=o.threshold_query(k);
                found=std::move(x.first);
                std::cerr<<"[DSM-ALL] threshold_reversals="<<x.second<<"\n";
            }else{
                FlowStats stats;
                found=flow_set(std::move(o),k,&stats);
                std::cerr<<"["<<mode<<"] maxflow="<<stats.flow
                         <<" changed_incidences="<<stats.swapped
                         <<" network_arcs="<<stats.arcs<<"\n";
            }
            for(int v:found)std::cout<<v+1<<"\n";
            return 0;
        }
        std::vector<int>idn;
        if(mode=="DSD"){
            long long count=0;
            idn=sequential_all(o,&count);
            std::cerr<<"[DSD] sequential_flow_queries="<<count<<"\n";
        } else if(mode=="DSDPLUS"){
            long long calls=0;
            idn=divide_all(o,&calls);
            std::cerr<<"[DSD+] midpoint_flow_queries="<<calls<<"\n";
        } else if(mode=="PARA"){
            int nthr=4;
            if(auto env=std::getenv("HGD_THREADS"))nthr=std::stoi(env);
            long long queries=0;
            idn=parallel_all(o,nthr,&queries);
            std::cerr<<"[PARA] parallel_flow_queries="<<queries<<" threads="<<nthr<<"\n";
        } else if(updated){
            const std::string batch=argv[3+shift];
            const bool add=(mode=="INSERT"||mode=="INSERT_FULL");
            if(mode=="INSERT"||mode=="DELETE"){
                long long repairs=0;
                idn=update_batch(o,batch,add,&repairs);
                std::cerr<<"["<<mode<<"] affected_component_reversals="<<repairs<<"\n";
            }else{
                // Genuine full recomputation comparator: apply edits to an arbitrary
                // orientation, then compute the entire egalitarian orientation anew.
                if(add)load_edges(o,batch);
                else for(int i:load_deleted_ids(batch))o.remove_edge(i);
                bulk_equilibrate_cached(o);
                idn=o.reachable_max(o.build());
            }
        }else throw std::invalid_argument("unknown optimized entrypoint mode");
        for(int v=0;v<(int)idn.size();++v)std::cout<<v+1<<","<<idn[v]<<"\n";
        return 0;
    }catch(std::exception const&e){
        std::cerr<<"[ERROR] "<<e.what()<<"\n";
        return 1;
    }
}
