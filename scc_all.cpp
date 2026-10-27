// Full IDN decomposition from a globally egalitarian orientation followed by
// one SCC condensation / reachability-max dynamic program.
// Usage: scc_all INPUT DELTA [VERTEX_COUNT]
#include "verified/hgd_optimized.hpp"
#include <chrono>
int main(int argc, char **argv) {
    using namespace hgdopt;
    try {
        if(argc < 3 || argc > 4) {
            std::cerr << "Usage: " << argv[0] << " INPUT DELTA [VERTEX_COUNT]\n";
            return 2;
        }
        size_t pos = 0;
        int delta = std::stoi(argv[2],&pos);
        if(pos!=std::string(argv[2]).size() || delta <= 0)
            throw std::invalid_argument("delta must be a positive integer");
        int vertex_count = 0;
        if(argc == 4) {
            pos = 0;
            vertex_count = std::stoi(argv[3],&pos);
            if(pos!=std::string(argv[3]).size() || vertex_count < 0)
                throw std::invalid_argument("invalid vertex count");
        }
        auto start=std::chrono::steady_clock::now();
        Orientation o(vertex_count,delta);
#ifdef HGD_SCC_GREEDY_INITIALIZATION
        load_greedy(o,argv[1]);
#else
        load_edges(o,argv[1]);
#endif
        auto loaded=std::chrono::steady_clock::now();
        long long flow_calls = 0;
        long long transfers = 0, incidence_changes = 0;
#ifdef HGD_SCC_CACHE_FLOW
        transfers=bulk_equilibrate_cached(o,nullptr,&flow_calls,&incidence_changes);
#else
        transfers=bulk_equilibrate(o,nullptr,&flow_calls);
#endif
        auto balanced=std::chrono::steady_clock::now();
#ifdef HGD_REVERSE_REACH
        auto idn=reachable_max_reverse(o);
#else
        auto idn=o.reachable_max(o.build());
#endif
        auto finished=std::chrono::steady_clock::now();
        for(int v=0; v<(int)idn.size(); ++v)
            std::cout << v+1 << ',' << idn[v] << '\n';
        using sec=std::chrono::duration<double>;
#ifdef HGD_REVERSE_REACH
#ifdef HGD_SCC_GREEDY_INITIALIZATION
        std::cerr << "[Reverse-Reach+] vertices=";
#else
        std::cerr << "[Reverse-Reach] vertices=";
#endif
#else
#ifdef HGD_SCC_GREEDY_INITIALIZATION
        std::cerr << "[SCC-ALL+] vertices=";
#else
        std::cerr << "[SCC-ALL] vertices=";
#endif
#endif
        std::cerr << o.n <<" edges="<<o.edges.size()
                  <<" load_s="<<sec(loaded-start).count()
                  <<" equilibrium_s="<<sec(balanced-loaded).count()
#ifdef HGD_REVERSE_REACH
                  <<" reverse_reach_s="<<sec(finished-balanced).count()
#else
                  <<" scc_s="<<sec(finished-balanced).count()
#endif
                  <<" maxflow_calls="<<flow_calls
                  <<" transferred_units="<<transfers
                  <<" changed_incidences="<<incidence_changes<<'\n';
        return 0;
    } catch (const std::exception &exc) {
        std::cerr << "[ERROR] " << exc.what() << '\n';
        return 1;
    }
}
