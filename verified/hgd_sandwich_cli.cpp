// The shared sandwich restriction for the two decomposition backends.
#define HGD_REVERSE_CSR
#define HGD_SPARSE_RESIDUAL_SYNC
#include "hgd_sandwich.hpp"
int main(int argc,char**argv){
    try{
        if(argc<4||argc>5)throw std::invalid_argument("usage: hgd_sandwich MODE INPUT DELTA [N]");
        std::string mode=argv[1];
        if(mode!="flow"&&mode!="reverse")throw std::invalid_argument("unknown mode");
        int delta=parse_integer(argv[3],1,"delta");
        int n=argc==5?parse_integer(argv[4],0,"vertex count"):0;
        Orientation o(n,delta);hgdopt::load_greedy(o,argv[2]);
        std::vector<int>ids;hgdsandwich::Stats st;
        ids=hgdsandwich::decompose(hgdsandwich::from_orientation(std::move(o)),mode=="reverse",st);
        for(size_t v=0;v<ids.size();++v)std::cout<<v+1<<","<<ids[v]<<"\n";
        std::cerr<<"{\"regions\":"<<st.regions<<",\"flow_calls\":"<<st.flow_calls
                 <<",\"flow_units\":"<<st.flow_units<<",\"full_query_vertices\":"<<st.full_query_vertices
                 <<",\"query_vertices\":"<<st.query_vertices<<",\"full_query_incidences\":"<<st.full_query_incidences
                 <<",\"query_incidences\":"<<st.query_incidences<<",\"forced_in\":"<<st.forced_in
                 <<",\"forced_out\":"<<st.forced_out<<",\"balanced_vertices\":"<<st.balanced_vertices
                 <<",\"max_depth\":"<<st.max_depth<<"}\n";
        return 0;
    }catch(std::exception const&e){std::cerr<<e.what()<<"\n";return 2;}
}
