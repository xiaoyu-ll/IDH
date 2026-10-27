// Identical updated graphs, warm component repair versus fresh recomputation.
#define HGD_REVERSE_REACH
#define HGD_REVERSE_CSR
#define HGD_SPARSE_RESIDUAL_SYNC
#include "verified/hgd_optimized.hpp"
#include <chrono>
#include <random>
int main(int argc,char**argv){
 using namespace hgdopt;
 try{
  if(argc!=7)throw std::invalid_argument("INPUT DELTA N insert|delete FRACTION SEED");
  Orientation o(parse_integer(argv[3],0,"n"),parse_integer(argv[2],1,"delta"));
  load_greedy(o,argv[1]);bulk_equilibrate_cached(o);
  bool add=std::string(argv[4])=="insert";
  if(!add&&std::string(argv[4])!="delete")throw std::invalid_argument("bad update mode");
  double fraction=std::stod(argv[5]);if(!(fraction>0&&fraction<=1))throw std::invalid_argument("fraction");
  std::vector<int> order(o.edges.size());std::iota(order.begin(),order.end(),0);
  std::mt19937 rng(parse_integer(argv[6],0,"seed"));std::shuffle(order.begin(),order.end(),rng);
  order.resize(std::max<size_t>(1,static_cast<size_t>(fraction*o.edges.size())));
  std::vector<std::vector<int>> additions;
  if(add)for(int id:order)additions.push_back(o.edges[id].vertices);
  using Clock=std::chrono::steady_clock;
  auto start=Clock::now();std::vector<int> changed;
  if(add){for(auto const&e:additions){greedy_add(o,e);changed.insert(changed.end(),e.begin(),e.end());}}
  else for(int id:order){auto e=o.edges[id].vertices;changed.insert(changed.end(),e.begin(),e.end());o.remove_edge(id);}
  auto mask=touched_components(o,changed);
  bulk_equilibrate_cached(o,&mask);auto warm=reachable_max_reverse(o);
  double warm_s=std::chrono::duration<double>(Clock::now()-start).count();
  start=Clock::now();Orientation fresh(o.n,o.delta);
  for(auto const&e:o.edges)if(e.active)greedy_add(fresh,e.vertices);
  bulk_equilibrate_cached(fresh);auto cold=reachable_max_reverse(fresh);
  double cold_s=std::chrono::duration<double>(Clock::now()-start).count();
  if(warm!=cold)throw std::logic_error("dynamic/fresh output mismatch");
  std::cout<<"{\"warm_s\":"<<warm_s<<",\"fresh_s\":"<<cold_s
    <<",\"batch_edges\":"<<order.size()<<",\"affected_vertices\":"<<std::count(mask.begin(),mask.end(),1)
    <<",\"n\":"<<o.n<<",\"equal\":true}\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
