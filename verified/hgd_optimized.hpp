#pragma once
// Distinct, correctness-checked implementations of several density-decomposition strategies.
// Underlying orientation and SCC operations use the independent, regression-tested reference
// from hgd_exact.cpp; command-line algorithms below do NOT all dispatch to its main().
#define main hgd_reference_unused_main
#include "hgd_exact.cpp"
#undef main
#include <atomic>
#ifdef HGD_PROFILE
#include <chrono>
#endif
#include <cstdlib>
#include <functional>
#include <numeric>
#include <memory>
#include <thread>
#include <unordered_set>

namespace hgdopt {
#ifdef HGD_PROFILE
inline double seconds_reach=0, seconds_flow=0, seconds_sync=0;
inline long long count_reach=0;
#endif

struct FlowStats {
    long long flow = 0;
    long long swapped = 0;
    long long nodes = 0;
    long long arcs = 0;
};
struct MaxFlow {
    struct Arc {int to,rev,cap,base;};
    std::vector<std::vector<Arc>> g;
    std::vector<int> level,ptr;
#ifdef HGD_REUSE_DINIC_STACKS
    std::vector<int> scratch_vertices, scratch_arcs, scratch_minima;
#endif
    // Optional mutation log for a persistent network. The existing Arc::base
    // field doubles as an incidence tag *only* when tracking is enabled.
    std::vector<uint8_t> touched_mark;
    std::vector<int> touched_ids;
    bool track_incidence = false;
    explicit MaxFlow(int n):g(n),level(n),ptr(n){}
    void enable_incidence_log(size_t count) {
        if(count>static_cast<size_t>(std::numeric_limits<int>::max()))
            throw std::overflow_error("too many incidence tags");
        touched_mark.assign(count,0);
        touched_ids.clear();
        track_incidence=true;
    }
    std::vector<int> take_incidence_log() {
        std::vector<int> result;
        result.swap(touched_ids);
        for(int id:result)touched_mark[id]=0;
        return result;
    }
    void record_incidence(int tag) {
        if(!track_incidence || tag<0)return;
        if(static_cast<size_t>(tag)>=touched_mark.size())
            throw std::logic_error("invalid tracked incidence tag");
        if(!touched_mark[tag]){touched_mark[tag]=1;touched_ids.push_back(tag);}
    }
    // Returns adjacency index of forward arc, for extracting unit exchanges.
    int add(int u,int v,int cap) {
        if(cap<0) throw std::invalid_argument("negative capacity");
        if(g[u].size()>=static_cast<size_t>(std::numeric_limits<int>::max()) ||
           g[v].size()>=static_cast<size_t>(std::numeric_limits<int>::max()))
            throw std::overflow_error("flow adjacency index overflow");
        const int a=(int)g[u].size(), b=(int)g[v].size();
        g[u].push_back({v,b,cap,cap});
        g[v].push_back({u,a,0,0});
        return a;
    }
    bool bfs(int s,int t) {
        std::fill(level.begin(),level.end(),-1);
        std::queue<int>q;level[s]=0;q.push(s);
        while(!q.empty()) {
            int v=q.front();q.pop();
            for(auto const&a:g[v]) if(a.cap>0&&level[a.to]<0){level[a.to]=level[v]+1;q.push(a.to);}
        }
        return level[t]>=0;
    }
    int send(int s,int t,int limit) {
        // Iterative blocking-flow DFS: no stack overflow on long alternating paths.
#ifdef HGD_REUSE_DINIC_STACKS
        auto &vertices=scratch_vertices;
        auto &chosen=scratch_arcs;
        auto &minima=scratch_minima;
        vertices.clear();chosen.clear();minima.clear();
        vertices.push_back(s);minima.push_back(limit);
#else
        std::vector<int> vertices{s},chosen;
        std::vector<int> minima{limit};
#endif
        while(!vertices.empty()){
            int v=vertices.back();
            if(v==t){
                int pushed=minima.back();
                for(size_t j=0;j<chosen.size();++j){
                    int u=vertices[j],i=chosen[j];
                    Arc &a=g[u][i];
                    record_incidence(a.base);
                    a.cap-=pushed;g[a.to][a.rev].cap+=pushed;
                }
                return pushed;
            }
            bool advanced=false;
            for(int &i=ptr[v];i<(int)g[v].size();++i){
                Arc const&a=g[v][i];
                if(a.cap>0&&level[a.to]==level[v]+1){
                    chosen.push_back(i);
                    vertices.push_back(a.to);
                    minima.push_back(std::min(minima.back(),a.cap));
                    advanced=true;break;
                }
            }
            if(!advanced){
                vertices.pop_back();minima.pop_back();
                if(chosen.empty()) break;
                int prev=vertices.back();++ptr[prev]; chosen.pop_back();
            }
        }
        return 0;
    }
    long long run(int s,int t){
        long long f=0;
        while(bfs(s,t)){
            std::fill(ptr.begin(),ptr.end(),0);
            while(int d=send(s,t,std::numeric_limits<int>::max())) f+=d;
        }
        return f;
    }
};

// Equivalent to a synchronous batch of head-tail swaps in a unit-incidence
// reorientation network. Unlike the pseudocode of Algorithm 2, each incidence
// is processed exactly once, and no saturated triple is enumerated.
inline FlowStats threshold_flow(Orientation &o, int k) {
    if(k<0)throw std::invalid_argument("negative k");
    FlowStats stats;
    if(k<=1) { // no deficient (degree <k-1) vertices, so no balancing needed
        return stats;
    }
    // Hyperedges of size <= delta have every incidence forced to be a head.
    // They cannot participate in a head-tail exchange, so their factors and
    // incidence arcs are omitted from the *flow* network. Their fixed heads
    // remain included in indeg[] and in the final IDN calculation.
    const int t=k-1;
    std::vector<int> factor_id(o.edges.size(),-1);
    int movable=0;
    for(int e=0;e<(int)o.edges.size();++e){
        const auto &h=o.edges[e];
        if(h.active && (int)h.vertices.size()>o.delta){
            checked_node_count(o.n,static_cast<size_t>(movable)+1,2);
            factor_id[e]=o.n+movable++;
        }
    }
    const int N=checked_node_count(o.n,movable,2),src=N-2,snk=N-1;
    MaxFlow net(N);
    struct Info {int vertex,edge,pos,src_arc;bool prior_head;};
    std::vector<Info> refs;
    for(int e=0;e<(int)o.edges.size();++e){
        auto const &h=o.edges[e];
        if(!h.active||factor_id[e]<0)continue;
        const int factor=factor_id[e];
        for(int p=0;p<(int)h.vertices.size();++p){
            int v=h.vertices[p];
            bool head=(bool)h.is_head[p];
            int u=head?factor:v,w=head?v:factor;
            int ai=net.add(u,w,1);
            refs.push_back({v,e,p,ai,head});
            ++stats.arcs;
        }
    }
    for(int u=0;u<o.n;++u){
        if(o.indeg[u]<t){net.add(src,u,t-o.indeg[u]);++stats.arcs;}
        if(o.indeg[u]>t){net.add(u,snk,o.indeg[u]-t);++stats.arcs;}
    }
    stats.nodes=N;
    stats.flow=net.run(src,snk);
    std::vector<int> balance(o.n,0);
    for(auto const &r:refs){
        const int origin=r.prior_head?factor_id[r.edge]:r.vertex;
        auto const &a=net.g[origin][r.src_arc];
        bool transferred=(a.base-a.cap)!=0;
        if(transferred){
            o.edges[r.edge].is_head[r.pos]=(uint8_t)!r.prior_head;
            balance[r.vertex]+=r.prior_head?-1:1;
            ++stats.swapped;
        }
    }
    for(int v=0;v<o.n;++v) o.indeg[v]+=balance[v];
    o.validate();
    // Max-flow optimality implies no directed path from d<k-1 to d>=k
    // in the final orientation. This is precisely the threshold-stability
    // needed to evaluate the unique D_{k,delta}; it does NOT assert global
    // egalitarianity for this one threshold.
    const auto reach=o.reachable_max(o.build());
    for(int u=0;u<o.n;++u){
        if(o.indeg[u]<k-1 && reach[u]>=k){
            throw std::logic_error("flow certificate failed: remaining reversible threshold path");
        }
    }
    return stats;
}

// Reusable threshold flow network for *successive* balancing operations.
// Every movable incidence owns one persistent residual pair (vertex, factor).
// A pair's two residual capacities are complementary, and encode the current
// head decision without rebuilding either the network or its adjacency lists.
// Source/sink arcs are reset between independent max-flow computations.
// PRECONDITION: Orientation is not externally modified while this object lives.
class ReusableThresholdNetwork {
    struct IncidenceRef {int v, edge, pos, fwd_index;};
    Orientation &o;
    int src,snk;
    MaxFlow net;
    std::vector<IncidenceRef> refs;
    std::vector<int> source_index, sink_index;
    bool sparse_sync = false;
    static int total_nodes(const Orientation &o) {
        int count=0;
        for(auto const&e:o.edges)if(e.active && (int)e.vertices.size()>o.delta)++count;
        return checked_node_count(o.n,count,2);
    }
public:
    explicit ReusableThresholdNetwork(Orientation &ori, bool log_sparse=false)
        : o(ori), src(total_nodes(ori)-2), snk(src+1), net(snk+1),
          source_index(ori.n),sink_index(ori.n),sparse_sync(log_sparse) {
        int factor=o.n;
        for(int ei=0;ei<(int)o.edges.size();++ei){
            auto const &e=o.edges[ei];
            if(!e.active || (int)e.vertices.size()<=o.delta)continue;
            for(int p=0;p<(int)e.vertices.size();++p){
                if(refs.size()>=static_cast<size_t>(std::numeric_limits<int>::max()))
                    throw std::overflow_error("too many movable incidence references");
                const int v=e.vertices[p];
                const bool head=e.is_head[p]!=0;
                const int ai=net.add(v,factor,head?0:1);
                auto &a=net.g[v][ai];
                net.g[factor][a.rev].cap=head?1:0;
                if(sparse_sync) {
                    const int tag=static_cast<int>(refs.size());
                    a.base=tag;
                    net.g[factor][a.rev].base=tag;
                }
                refs.push_back({v,ei,p,ai});
            }
            ++factor;
        }
        if(factor!=src)throw std::logic_error("factor count mismatch");
        for(int v=0;v<o.n;++v){
            source_index[v]=net.add(src,v,0);
            sink_index[v]=net.add(v,snk,0);
            if(sparse_sync) {
                auto &s=net.g[src][source_index[v]];
                net.g[v][s.rev].base=-1;s.base=-1;
                auto &t=net.g[v][sink_index[v]];
                net.g[snk][t.rev].base=-1;t.base=-1;
            }
        }
        if(sparse_sync)net.enable_incidence_log(refs.size());
    }
    FlowStats threshold(int k){
        if(k<0)throw std::invalid_argument("negative k");
        FlowStats st;
        st.nodes=snk+1;
        st.arcs=(long long)refs.size()+2LL*o.n;
        if(k<=1)return st;
        const int target=k-1;
        for(int v=0;v<o.n;++v){
            auto &source=net.g[src][source_index[v]];
            source.cap=std::max(0,target-o.indeg[v]);
            net.g[v][source.rev].cap=0;
            auto &sink=net.g[v][sink_index[v]];
            sink.cap=std::max(0,o.indeg[v]-target);
            net.g[snk][sink.rev].cap=0;
        }
#ifdef HGD_PROFILE
        const auto tp_start=std::chrono::steady_clock::now();
#endif
        st.flow=net.run(src,snk);
#ifdef HGD_PROFILE
        const auto tp_run=std::chrono::steady_clock::now();
        seconds_flow+=std::chrono::duration<double>(tp_run-tp_start).count();
#endif
        std::vector<int>diff(o.n,0);
        const auto sync_one = [&](int id){
            auto const &r=refs[id];
            auto const &a=net.g[r.v][r.fwd_index];
            if(a.cap!=0 && a.cap!=1)throw std::logic_error("incidence capacity corrupt");
            if(net.g[a.to][a.rev].cap!=1-a.cap)
                throw std::logic_error("non-complementary incidence residual capacities");
            const bool head_now=(a.cap==0);
            const bool head_old=o.edges[r.edge].is_head[r.pos]!=0;
            if(head_now!=head_old){
                o.edges[r.edge].is_head[r.pos]=(uint8_t)head_now;
                diff[r.v] += head_now?1:-1;
                ++st.swapped;
            }
        };
        if(sparse_sync) {
            for(int id:net.take_incidence_log()) sync_one(id);
        } else {
            for(int id=0;id<(int)refs.size();++id) sync_one(id);
        }
        for(int v=0;v<o.n;++v)o.indeg[v]+=diff[v];
#ifdef HGD_PROFILE
        seconds_sync+=std::chrono::duration<double>(std::chrono::steady_clock::now()-tp_run).count();
#endif
        // Dinic has exhausted all source-to-sink residual paths. The new head
        // assignment therefore contains no low-to-high threshold path. Exact
        // equivalence is regression-tested against the non-reusable reference.
        return st;
    }
};

// Exact reverse reachability, without materializing the factor digraph or SCCs.
// Process all possible destination vertices in nonincreasing indegree order.
// When a vertex is reached for the first time in the reverse digraph, that
// destination has the maximum indegree among all vertices it can reach.
// Hyperedge factors are visited once, traversing from a head to each tail.
// With counting buckets, runtime is O(n + M + L), extra memory O(n+M+J),
// where M is the number of stored occurrence slots (including tombstones),
// L is active incidence count, and J is movable head incidence count.
// In a static graph M=m; long update histories must not be analyzed using
// only the number of currently active edges.
inline std::vector<int> reachable_max_reverse(const Orientation &o) {
    const int n=o.n;
#ifdef HGD_REVERSE_CSR
    // Contiguous head-incidence CSR avoids O(n) separate vector objects and
    // allocator fragmentation (particularly important for millions of isolates).
    std::vector<int> head_offsets((size_t)n+1,0);
    for(int eid=0;eid<(int)o.edges.size();++eid){
        const auto &e=o.edges[eid];
        if(!e.active || (int)e.vertices.size()<=o.delta)continue;
        for(size_t p=0;p<e.vertices.size();++p)
            if(e.is_head[p]){
                int &count=head_offsets[(size_t)e.vertices[p]+1];
                if(count==std::numeric_limits<int>::max())
                    throw std::overflow_error("CSR incidence count overflow");
                ++count;
            }
    }
    for(int v=0;v<n;++v){
        if(head_offsets[v]>std::numeric_limits<int>::max()-head_offsets[v+1])
            throw std::overflow_error("CSR incidence offset overflow");
        head_offsets[v+1]+=head_offsets[v];
    }
    std::vector<int> head_edge_ids((size_t)head_offsets[n]);
    std::vector<int> cursor=head_offsets;
    for(int eid=0;eid<(int)o.edges.size();++eid){
        const auto &e=o.edges[eid];
        if(!e.active || (int)e.vertices.size()<=o.delta)continue;
        for(size_t p=0;p<e.vertices.size();++p)
            if(e.is_head[p])head_edge_ids[cursor[e.vertices[p]]++]=eid;
    }
#else
    std::vector<std::vector<int>> head_to_edges(n);
    for(int eid=0;eid<(int)o.edges.size();++eid){
        const auto &e=o.edges[eid];
        if(!e.active || (int)e.vertices.size()<=o.delta)continue;
        for(size_t p=0;p<e.vertices.size();++p)
            if(e.is_head[p])head_to_edges[e.vertices[p]].push_back(eid);
    }
#endif
    // Stable counting sort by nondecreasing indegree, scanned backwards.
    const int max_load=o.indeg.empty()?0:*std::max_element(o.indeg.begin(),o.indeg.end());
    std::vector<int> offset((size_t)max_load+1,0);
    for(int v=0;v<n;++v)++offset[o.indeg[v]];
    int run=0;
    for(int &z:offset){int count=z;z=run;run+=count;}
    std::vector<int> order(n);
    for(int v=0;v<n;++v)order[offset[o.indeg[v]]++]=v;
    std::vector<int> best(n,-1);
    std::vector<uint8_t> factor_seen(o.edges.size(),0);
    std::vector<int> pending;
    pending.reserve(std::min(n,4096));
    for(int ix=n-1;ix>=0;--ix){
        const int seed=order[ix];
        if(best[seed]!=-1)continue;
        const int value=o.indeg[seed];
        best[seed]=value;
        pending.clear();pending.push_back(seed);
        for(size_t qi=0;qi<pending.size();++qi){
            const int v=pending[qi];
            const auto advance=[&](int eid){
                if(factor_seen[eid])return;
                factor_seen[eid]=1;
                const auto &e=o.edges[eid];
                for(size_t p=0;p<e.vertices.size();++p){
                    if(e.is_head[p])continue;
                    const int tail=e.vertices[p];
                    if(best[tail]==-1){best[tail]=value;pending.push_back(tail);}
                }
            };
#ifdef HGD_REVERSE_CSR
            for(int j=head_offsets[v];j<head_offsets[v+1];++j)
                advance(head_edge_ids[j]);
#else
            for(int eid:head_to_edges[v])advance(eid);
#endif
        }
    }
    return best;
}

// Overflow-safe equivalent of max(low+2, (low+high+1)/2).
inline int balancing_threshold(int low,int high){
    if(low<0 || high<low || high-low<2)
        throw std::invalid_argument("pivot requires an indegree gap of at least two");
    return std::max(low+2,low+(high-low)/2+(high-low)%2);
}

// Strictly exact balancing with persistent incidence topology. Unlike the
// faster-to-write alternative, this does not reconstruct a million-incidence
// Dinic network for every threshold; source/sink capacities alone change.
inline long long bulk_equilibrate_cached(Orientation &o,
                                         std::vector<uint8_t> const *restrict_to=nullptr,
                                         long long *flow_calls=nullptr,
                                         long long *incidence_changes=nullptr){
    if(restrict_to && restrict_to->size()!=static_cast<size_t>(o.n))
        throw std::invalid_argument("restricted component bitmap size mismatch");
    // An already balanced graph needs no flow topology (common after deletions
    // or when all incidences are forced). Allocate only on the first witness.
    std::unique_ptr<ReusableThresholdNetwork> net;
    long long moved=0, calls=0, changed=0;
    while(true){
#ifdef HGD_PROFILE
        const auto reach_start=std::chrono::steady_clock::now();
#endif
#ifdef HGD_REVERSE_REACH
        const auto reach=reachable_max_reverse(o);
#else
        const auto reach=o.reachable_max(o.build());
#endif
#ifdef HGD_PROFILE
        seconds_reach+=std::chrono::duration<double>(std::chrono::steady_clock::now()-reach_start).count();
        ++count_reach;
#endif
        int source=-1,largest_gap=1;
        for(int v=0;v<o.n;++v){
            if(restrict_to && !(*restrict_to)[v])continue;
            int gap=reach[v]-o.indeg[v];
            if(gap>largest_gap){largest_gap=gap;source=v;}
        }
        if(source<0)break;
        int k=balancing_threshold(o.indeg[source],reach[source]);
        if(!net){
#ifdef HGD_SPARSE_RESIDUAL_SYNC
            net=std::make_unique<ReusableThresholdNetwork>(o,true);
#else
            net=std::make_unique<ReusableThresholdNetwork>(o);
#endif
        }
        auto before=o.potential();
        auto st=net->threshold(k);
        if(st.flow<=0 || o.potential()>=before)
            throw std::logic_error("cached threshold did not strictly decrease potential");
        ++calls;moved+=st.flow;changed+=st.swapped;
    }
    o.validate();
    // A final reachability test is implicit in the last zero-gap iteration.
#ifdef HGD_PROFILE
    std::cerr<<"[PROFILE] scc_reach_s="<<seconds_reach<<" dinic_s="<<seconds_flow
             <<" incidence_sync_s="<<seconds_sync<<" reach_calls="<<count_reach<<'\n';
#endif
    if(flow_calls)*flow_calls=calls;
    if(incidence_changes)*incidence_changes=changed;
    return moved;
}

// In greedy initialization, ties are deterministic and equal repeated vertices
// in one hyperedge are deduplicated before selecting heads.
inline void greedy_add(Orientation &o,std::vector<int> vs){
    if(o.edges.size()>=static_cast<size_t>(std::numeric_limits<int>::max()-1))
        throw std::overflow_error("edge occurrence ID limit reached");
    if(vs.empty())throw std::invalid_argument("empty edge");
    for(int v:vs)if(v<0 || v==std::numeric_limits<int>::max())
        throw std::invalid_argument("invalid vertex ID");
    std::sort(vs.begin(),vs.end());vs.erase(std::unique(vs.begin(),vs.end()),vs.end());
    if(vs.capacity()>2*vs.size())std::vector<int>(vs).swap(vs);
    o.resize_vertices(vs.back()+1);
    std::vector<int> order((int)vs.size());std::iota(order.begin(),order.end(),0);
    int r=std::min(o.delta,(int)vs.size());
    std::partial_sort(order.begin(),order.begin()+r,order.end(),[&](int a,int b){
        int da=o.indeg[vs[a]],db=o.indeg[vs[b]];
        return da!=db? da<db:vs[a]<vs[b];
    });
    EdgeRec e;e.vertices=std::move(vs);e.is_head.assign(e.vertices.size(),0);
    for(int i=0;i<r;++i){int p=order[i];e.is_head[p]=1;++o.indeg[e.vertices[p]];}
    o.edges.push_back(std::move(e));
}
inline void load_greedy(Orientation &o,std::string const&path){
    std::ifstream f(path);if(!f)throw std::runtime_error("cannot open "+path);
    std::string s;int line=0;
    while(std::getline(f,s)){
        ++line;s=trim(s);if(s.empty()||s[0]=='#')continue;
        try{greedy_add(o,parse_edge(s));}
        catch(std::exception const&e){throw std::runtime_error(path+":"+std::to_string(line)+": "+e.what());}
    }
}
inline int max_degree(Orientation const&o){
    std::vector<int>deg(o.n,0);
    for(auto const &e:o.edges)if(e.active)for(int v:e.vertices)++deg[v];
    return deg.empty()?0:*std::max_element(deg.begin(),deg.end());
}
// Batched balancing: a threshold flow can move many contribution units.
// Select an actual reachable gap, then solve all compatible exchanges at once.
inline long long bulk_equilibrate(Orientation &o,
                                 std::vector<uint8_t> const *restrict_to=nullptr,
                                 long long *flow_calls=nullptr){
    if(restrict_to && restrict_to->size()!=static_cast<size_t>(o.n))
        throw std::invalid_argument("restricted component bitmap size mismatch");
    long long moved=0,calls=0;
    while(true){
        auto reach=o.reachable_max(o.build());
        int source=-1,largest_gap=1;
        for(int v=0;v<o.n;++v){
            if(restrict_to && !(*restrict_to)[v])continue;
            int gap=reach[v]-o.indeg[v];
            if(gap>largest_gap){largest_gap=gap;source=v;}
        }
        if(source<0)break;
        int k=balancing_threshold(o.indeg[source],reach[source]);
        const auto before=o.potential();
        auto st=threshold_flow(o,k);
        if(st.flow<=0 || o.potential()>=before)
            throw std::logic_error("bulk balancing failed strict potential descent");
        moved+=st.flow;++calls;
    }
    o.validate();
    if(flow_calls)*flow_calls=calls;
    return moved;
}
inline std::vector<int> flow_set(Orientation o,int k,FlowStats *statistics=nullptr){
    auto st=threshold_flow(o,k);
    if(statistics)*statistics=st;
    auto reach=o.reachable_max(o.build());
    std::vector<int>ans;
    for(int v=0;v<o.n;++v)if(reach[v]>=k)ans.push_back(v);
    return ans;
}
// Layer-by-layer DSD; distinct from the divide-and-conquer midpoint strategy.
inline std::vector<int> sequential_all(Orientation const&base,long long *queries=nullptr){
    const int upper=max_degree(base);
    std::vector<int>scores(base.n,0);
    long long count=0;
    for(int k=1;k<=upper;++k){
        auto members=flow_set(base,k);++count;
        if(members.empty())break;
        for(int v:members)scores[v]=k;
    }
    if(queries)*queries=count;
    return scores;
}
inline std::vector<int> divide_all(Orientation const&base,long long *num_calls=nullptr){
    int hi=max_degree(base)+1;
    std::vector<int> out(base.n,0);
    std::vector<int> dlo(base.n);std::iota(dlo.begin(),dlo.end(),0);
    std::vector<int> dhi;
    long long calls=0;
    std::function<void(int,std::vector<int>const&,int,std::vector<int>const&)> rec;
    rec=[&](int lo,std::vector<int>const&low,int high,std::vector<int>const&up){
        if(high-lo<=1||low==up)return;
        const int mid=lo+(high-lo)/2;
        auto midway=flow_set(base,mid);++calls;
        // Binary membership on disjoint intervals; assign upper-bound scores
        // at all successful queried thresholds, independent of recursion order.
        for(int v:midway)out[v]=std::max(out[v],mid);
        rec(lo,low,mid,midway);
        rec(mid,midway,high,up);
    };
    rec(0,dlo,hi,dhi);
    if(num_calls)*num_calls=calls;
    return out;
}
// Divide-and-conquer IDNs with ONE mutable orientation and ONE residual
// reorientation network across all midpoint queries. Each threshold query
// remains exact on the current orientation; only the order changes.
inline std::vector<int> divide_all_cached(Orientation base,
                                           long long *num_calls=nullptr,
                                           long long *flow_units=nullptr){
    const int hi=max_degree(base)+1;
    std::vector<int> out(base.n,0);
    std::vector<int> low(base.n);std::iota(low.begin(),low.end(),0);
    std::vector<int> upper;
    long long calls=0,flow=0;
    ReusableThresholdNetwork net(base);
    std::function<void(int,std::vector<int>const&,int,std::vector<int>const&)> rec;
    rec=[&](int l,std::vector<int>const &below,int h,std::vector<int>const &above){
        if(h-l<=1||below==above)return;
        const int mid=l+(h-l)/2;
        auto st=net.threshold(mid);++calls;flow+=st.flow;
#ifdef HGD_REVERSE_REACH
        auto reach=reachable_max_reverse(base);
#else
        auto reach=base.reachable_max(base.build());
#endif
        std::vector<int> here;
        for(int v=0;v<base.n;++v)if(reach[v]>=mid){
            here.push_back(v);out[v]=std::max(out[v],mid);
        }
        // A max-flow threshold certificate: no vertex below mid-1 can
        // still reach a vertex of load >= mid. This makes the query exact.
        for(int v=0;v<base.n;++v)
            if(base.indeg[v]<mid-1 && reach[v]>=mid)
                throw std::logic_error("cached DSD+ threshold certificate failed");
        rec(l,below,mid,here);
        rec(mid,here,h,above);
    };
    rec(0,low,hi,upper);
    if(num_calls)*num_calls=calls;
    if(flow_units)*flow_units=flow;
    return out;
}

inline std::vector<int> parallel_all(Orientation const&base,int threads,long long *queries=nullptr){
    const int upper=max_degree(base);
    std::vector<int>scores(base.n,0);
    if(upper==0){if(queries)*queries=0;return scores;}
    threads=std::max(1,std::min(threads,upper));
    std::atomic<long long> next{1};
    std::vector<std::vector<int>> locals(threads,std::vector<int>(base.n,0));
    std::vector<std::thread>jobs;
    for(int t=0;t<threads;++t)jobs.emplace_back([&,t](){
        while(true){
            long long candidate=next.fetch_add(1);if(candidate>upper)break;
            int k=static_cast<int>(candidate);
            auto members=flow_set(base,k);
            for(int v:members)locals[t][v]=std::max(locals[t][v],k);
        }
    });
    for(auto &j:jobs)j.join();
    for(auto const &lv:locals)for(int v=0;v<base.n;++v)scores[v]=std::max(scores[v],lv[v]);
    if(queries)*queries=upper;
    return scores;
}
inline std::vector<uint8_t> touched_components(Orientation const&o,std::vector<int> const &modified){
    std::vector<std::vector<int>> v2e(o.n);
    for(int ei=0;ei<(int)o.edges.size();++ei){
        if(!o.edges[ei].active)continue;
        for(int v:o.edges[ei].vertices)v2e[v].push_back(ei);
    }
    std::vector<uint8_t> used(o.n,0),edge_seen(o.edges.size(),0);
    std::queue<int>q;
    for(int v:modified)if(v>=0&&v<o.n&&!used[v]){used[v]=1;q.push(v);}
    while(!q.empty()){
        int v=q.front();q.pop();
        for(int ei:v2e[v])if(!edge_seen[ei]){
            edge_seen[ei]=1;
            for(int u:o.edges[ei].vertices)if(!used[u]){used[u]=1;q.push(u);}
        }
    }
    return used;
}
inline long long equilibrate_affected(Orientation &o,std::vector<int> const&changed,
                                       long long *flow_calls=nullptr){
    auto affected=touched_components(o,changed);
    // Untouched weak components were egalitarian and their incidences have not changed.
    return bulk_equilibrate_cached(o,&affected,flow_calls);
}
inline std::vector<int> update_batch(Orientation &o,std::string const&batch,bool insertion,long long *moves=nullptr){
    // Validate the entire batch before changing even the initial orientation.
    // Input errors must not leave a persistent caller with a partial update.
    std::vector<std::vector<int>> additions;
    std::vector<int> deletions;
    std::vector<int>changed;
    if(insertion){
        std::ifstream f(batch);if(!f)throw std::runtime_error("cannot open "+batch);
        std::string s;
        while(std::getline(f,s)){
            s=trim(s);if(s.empty()||s[0]=='#')continue;
            additions.push_back(parse_edge(s));
        }
    }else{
        deletions=load_deleted_ids(batch);
        std::unordered_set<int> seen;
        for(int id:deletions)
            if(id<0||id>=(int)o.edges.size()||!o.edges[id].active||!seen.insert(id).second)
                throw std::invalid_argument("invalid or duplicate deletion ID");
    }
    bulk_equilibrate_cached(o);
    if(insertion){
        for(auto const &vs:additions){
            // Orient new incidence toward the least loaded vertices, a locally
            // inexpensive warm-start heuristic; exactness comes from repair.
            greedy_add(o,vs);
            auto &newedge=o.edges.back();
            changed.insert(changed.end(),newedge.vertices.begin(),newedge.vertices.end());
        }
    }else{
        for(int id:deletions){
            for(int v:o.edges[id].vertices)changed.push_back(v);
            o.remove_edge(id);
        }
    }
    long long repairs=equilibrate_affected(o,changed);
    if(moves)*moves=repairs;
#ifdef HGD_REVERSE_REACH
    return reachable_max_reverse(o);
#else
    return o.reachable_max(o.build());
#endif
}
} // namespace hgdopt
