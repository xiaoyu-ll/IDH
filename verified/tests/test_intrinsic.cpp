// Independent subset enumeration of F_delta, not an orientation oracle.
#include "../hgd_intrinsic.hpp"
#include <random>
using namespace hgdopt;
static void check(bool ok) { if(!ok) throw std::logic_error("intrinsic regression failed"); }
static int bits(unsigned x) { return __builtin_popcount(x); }
static std::vector<int> mandatory(const Orientation &o) {
    std::vector<int> f(1u<<o.n);
    for(unsigned x=0;x<f.size();++x) for(const auto &e:o.edges) if(e.active) {
        int outside=0;
        for(int v:e.vertices) outside+=!(x&(1u<<v));
        f[x]+=std::max(0,std::min(o.delta,(int)e.vertices.size())-outside);
    }
    return f;
}
static unsigned canonical(const std::vector<int>&f,int k) {
    int best=std::numeric_limits<int>::min();
    unsigned intersection=(unsigned)f.size()-1;
    for(unsigned x=0;x<f.size();++x) {
        int q=f[x]-(k-1)*bits(x);
        if(q>best){best=q;intersection=x;}
        else if(q==best)intersection&=x;
    }
    check(f[intersection]-(k-1)*bits(intersection)==best);
    return intersection;
}
static unsigned level(const std::vector<int>&idn,int k) {
    unsigned mask=0;
    for(int v=0;v<(int)idn.size();++v)if(idn[v]>=k)mask|=1u<<v;
    return mask;
}
static void audit_layers(const Orientation &o,const std::vector<int> &ids,
                         const std::vector<int> &f) {
    for(unsigned x=0;x<f.size();++x) {
        std::vector<uint8_t> inside(o.n);
        for(int v=0;v<o.n;++v)inside[v]=bool(x&(1u<<v));
        check(intrinsic_contribution(o,inside)==f[x]);
    }
    long long size=0,total=0;
    auto layers=intrinsic_layers(o,ids);
    for(const auto &layer:layers) {
        unsigned d=level(ids,layer.level),above=level(ids,layer.level+1);
        check(layer.size==bits(d&~above));
        check(layer.numerator==f[d]-f[above]);
        long long allocated=0;
        for(int v=0;v<o.n;++v)if(ids[v]==layer.level)allocated+=o.indeg[v];
        check(layer.numerator==allocated);
        check(layer.numerator>(layer.level-1)*layer.size);
        check(layer.numerator<=layer.level*layer.size);
        check(layer.density()==double(layer.numerator)/layer.size);
        size+=layer.size;total+=layer.numerator;
    }
    check(size==o.n && total==o.R());
    for(bool bad_set:{false,true}) {
        bool rejected=false;
        try {
            if(bad_set)intrinsic_contribution(o,std::vector<uint8_t>(o.n+1));
            else intrinsic_layers(o,std::vector<int>(o.n+1));
        }catch(const std::invalid_argument&){rejected=true;}
        check(rejected);
    }
}
static void audit(Orientation o) {
    const Orientation initial=o;
    auto f=mandatory(o);
    check(f[0]==0 && f.back()==o.R());
    for(unsigned a=0;a<f.size();++a)for(unsigned b=0;b<f.size();++b)
    {
        check(f[a]+f[b]<=f[a|b]+f[a&b]);
        if((a&b)==a)check(f[a]<=f[b]);
    }
    bulk_equilibrate_cached(o);
    auto ids=reachable_max_reverse(o);
    check(ids==o.reachable_max(o.build()));
    audit_layers(o,ids,f);
    const int upper=max_degree(o)+1;
    for(int k=0;k<=upper;++k) {
        unsigned d=canonical(f,k);
        check(level(ids,k)==d);
        unsigned above=canonical(f,k+1),shell=d&~above;
        check((above&d)==above);
        if(d)check(f[d]>=(k-1)*bits(d)+1);
        if(shell){
            const int marginal=f[d]-f[above];
            check(marginal>(k-1)*bits(shell));
            check(marginal<=k*bits(shell));
        }
        int allocated=0,seeds=0,internal_heads=0,internal_inc=0,volume=0,boundary=0;
        for(int v=0;v<o.n;++v)if(d&(1u<<v)) {
            allocated+=o.indeg[v];seeds+=o.indeg[v]>=k;
        }
        check(allocated==f[d]);
        check(allocated>=(k-1)*bits(d)+seeds);
        for(const auto&e:o.edges)if(e.active) {
            int inside=0;
            for(int v:e.vertices)inside+=bool(d&(1u<<v));
            volume+=inside;
            if(inside==(int)e.vertices.size()) {
                internal_heads+=std::min(o.delta,inside);internal_inc+=inside;
            } else if(inside) ++boundary;
        }
        check(internal_inc>=internal_heads);
        check(internal_heads<=f[d]); // intrinsic theta lies in [0,1]
        check(boundary<=volume-internal_inc);
        for(unsigned x=0;x<f.size();++x) {
            if(x && (x&d)==x) check(f[d]-f[d^x]>(k-1)*bits(x));
            if(!(x&d))check(f[d|x]-f[d]<=(k-1)*bits(x));
        }
        if(o.delta==1 && k>0) {
            // Every vertex of D has >=k fully contained incident edges.
            for(int v=0;v<o.n;++v)if(d&(1u<<v)) {
                int degree=0;
                for(const auto&e:o.edges)if(e.active) {
                    bool contained=true,incident=false;
                    for(int u:e.vertices){contained&=bool(d&(1u<<u));incident|=u==v;}
                    degree+=contained&&incident;
                }
                check(degree>=k);
            }
        }
    }
    // Arbitrary/nonmonotone thresholds; dense and sparse residual syncing.
    for(bool sparse:{false,true}) {
        Orientation state=initial;
        ReusableThresholdNetwork net(state,sparse);
        for(int k:{upper,0,2,1,upper/2,upper,0,3}) {
            auto before=state.indeg;
            auto potential=state.potential();
            auto stats=net.threshold(k);
            state.validate();
            check(stats.flow==0 ? state.potential()==potential : state.potential()<potential);
            for(int v=0;v<state.n;++v) {
                if(before[v]<k-1)check(state.indeg[v]>=before[v]);
                if(before[v]>k-1)check(state.indeg[v]<=before[v]);
            }
            check(level(reachable_max_reverse(state),k)==canonical(f,k));
        }
    }
}
int main() {
    bool empty_rejected=false;
    try{IntrinsicLayer{}.density();}catch(const std::domain_error&){empty_rejected=true;}
    check(empty_rejected);
    audit(Orientation(0,1));
    // Ties must select the smallest maximizing set. A layer's marginal
    // density need not equal its stand-alone F_delta density.
    {
        Orientation o(5,1);
        for(int u=0;u<4;++u)for(int v=u+1;v<4;++v)o.add_edge({u,v});
        o.add_edge({0,4});
        auto f=mandatory(o);
        check(canonical(f,1)==31 && canonical(f,2)==15);
        check(f[31]-5==f[15]-4); // equal Q at k=2, but 15 is smallest
        check(f[31]-f[15]==1 && f[16]==0); // marginal vs stand-alone
        check(f[1]==0); // no false stand-alone density bound inside D_2
        audit(o);
    }
    {
        Orientation o(3,3);o.add_edge({0,0,1});o.add_edge({0,1});
        audit(o);
        auto f=mandatory(o);check(canonical(f,1)==3 && canonical(f,2)==3);
    }
    // A former tail loses its IDN despite its allocated indegree staying zero.
    {
        Orientation o(2,1);o.add_edge({0,1});bulk_equilibrate_cached(o);
        check(o.indeg[1]==0 && reachable_max_reverse(o)[1]==1);
        o.remove_edge(0);equilibrate_affected(o,{0,1});
        check(o.indeg[1]==0 && reachable_max_reverse(o)[1]==0);
        audit(o);
    }
    // All 64 labeled loopless simple graphs on four vertices: C_2k subset D_k.
    for(unsigned graph=0;graph<64;++graph) {
        Orientation o(4,1);
        int bit=0;
        for(int u=0;u<4;++u)for(int v=u+1;v<4;++v,++bit)
            if(graph&(1u<<bit))o.add_edge({u,v});
        audit(o);
        auto f=mandatory(o);
        for(int k=0;k<=4;++k) {
            unsigned core=15;
            while(true) {
                unsigned next=core;
                for(int v=0;v<4;++v)if(core&(1u<<v)) {
                    int degree=0;
                    for(const auto&e:o.edges)
                        if((core&(1u<<e.vertices[0])) && (core&(1u<<e.vertices[1])))
                            degree+=(e.vertices[0]==v || e.vertices[1]==v);
                    if(degree<2*k)next&=~(1u<<v);
                }
                if(next==core)break;
                core=next;
            }
            check((core&canonical(f,k))==core);
        }
    }
    std::mt19937 rng(719);
    for(int trial=0;trial<160;++trial) {
        int n=1+rng()%7;
        Orientation o(n,1+rng()%9);
        for(int i=0,m=rng()%16;i<m;++i) {
            std::vector<int> e;
            for(int v=0;v<n;++v)if(rng()%2)e.push_back(v);
            if(e.empty())e.push_back(rng()%n);
            o.add_edge(e);
        }
        audit(o);
        bulk_equilibrate_cached(o);
        for(int step=0;step<32;++step) {
            auto oldf=mandatory(o),oldids=reachable_max_reverse(o);
            std::vector<int> changed;
            std::vector<int> active;
            for(int i=0;i<(int)o.edges.size();++i)if(o.edges[i].active)active.push_back(i);
            const bool insertion=step%2 || active.empty();
            if(insertion) {
                changed={int(rng()%n),int(rng()%n)};
                greedy_add(o,changed);
                changed=o.edges.back().vertices;
            } else {
                int id=active[rng()%active.size()];
                changed=o.edges[id].vertices;o.remove_edge(id);
            }
            auto mask=touched_components(o,changed);
            auto before=o.edges;
            equilibrate_affected(o,changed);
            for(size_t i=0;i<o.edges.size();++i)if(o.edges[i].active && !mask[o.edges[i].vertices[0]])
                check(o.edges[i].is_head==before[i].is_head);
            auto ids=reachable_max_reverse(o);
            auto f=mandatory(o);
            for(unsigned x=0;x<f.size();++x) {
                int outside=0;
                for(int v:changed)outside+=!(x&(1u<<v));
                int g=std::max(0,std::min(o.delta,(int)changed.size())-outside);
                check(f[x]==oldf[x]+(insertion?g:-g));
            }
            for(int v=0;v<n;++v) {
                const int difference=ids[v]-oldids[v];
                check(insertion?(difference>=0 && difference<=1):
                                (difference<=0 && difference>=-1));
                if(!mask[v])check(ids[v]==oldids[v]);
            }
            audit_layers(o,ids,f);
            for(int k=0;k<=max_degree(o)+1;++k)check(level(ids,k)==canonical(f,k));
        }
    }
    std::cout<<"PASS: 228 intrinsic subset audits (64 exhaustive simple graphs), 5120 dynamic updates; pointwise F +/- g, one-unit IDN bounds, untouched components, direct layer evaluation, allocated layer equality, budget telescoping, ties and residual invariants\n";
}
