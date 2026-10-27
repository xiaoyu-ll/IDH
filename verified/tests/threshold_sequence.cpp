// Debug harness for reusable max-flow over deliberately non-monotone k sequences.
// Build: g++ -std=c++17 -O2 -pthread verified/tests/threshold_sequence.cpp -o build/threshold_sequence_test
#include "../hgd_optimized.hpp"
int main(int argc,char**argv){
  try {
    if(argc<4)throw std::invalid_argument("usage: INPUT DELTA VERTEX_COUNT [k ...]");
    int delta=std::stoi(argv[2]),n=std::stoi(argv[3]);
    Orientation o(n,delta);load_edges(o,argv[1]);
#ifdef HGD_SPARSE_RESIDUAL_SYNC
    hgdopt::ReusableThresholdNetwork residual(o,true);
#else
    hgdopt::ReusableThresholdNetwork residual(o);
#endif
    for(int i=4;i<argc;++i){
      int k=std::stoi(argv[i]);residual.threshold(k);
      o.validate();
      auto reach=o.reachable_max(o.build());
      std::cout<<k<<":";
      for(int v=0;v<o.n;++v){
        if(o.indeg[v]<k-1 && reach[v]>=k)
          throw std::logic_error("threshold residual reachability certificate failed");
        if(reach[v]>=k)std::cout<<v+1<<",";
      }
      std::cout<<'\n';
    }
  }catch(std::exception const&e){std::cerr<<"[ERROR] "<<e.what()<<'\n';return 1;}
}
