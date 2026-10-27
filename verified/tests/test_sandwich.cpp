#include "../hgd_sandwich.hpp"
#include <random>
using namespace hgdsandwich;
static void check(bool ok){if(!ok)throw std::logic_error("sandwich theorem regression");}
static long long F(Region const&r,unsigned mask){
    long long f=0;for(auto const&e:r.edges){int outside=0;for(int v:e.vertices)outside+=!(mask&(1u<<v));f+=std::max(0,e.budget-outside);}return f;
}
static unsigned canonical(Region const&r,int k){
    long long best=std::numeric_limits<long long>::min();unsigned out=(1u<<r.size())-1;
    for(unsigned x=0;x<(1u<<r.size());++x){
        long long q=F(r,x)-static_cast<long long>(k-1)*__builtin_popcount(x);
        if(q>best){best=q;out=x;}else if(q==best)out&=x;
    }return out;
}
static unsigned mask(std::vector<uint8_t>const&a){unsigned z=0;for(size_t i=0;i<a.size();++i)if(a[i])z|=1u<<i;return z;}
static void audit(Region r){
    r.validate();auto g=r.graph();auto L=bound(r,g,true,false),U=bound(r,g,false,false);
    check(L==bound(r,g,true,true));check(U==bound(r,g,false,true));
    int K=r.load.empty()?0:*std::max_element(r.load.begin(),r.load.end());
    for(int k=0;k<=K+1;++k){
        unsigned d=canonical(r,k);std::vector<uint8_t>A(r.size()),B(r.size());
        for(int v=0;v<r.size();++v){A[v]=L[v]>=k;B[v]=U[v]>=k;check(!A[v]||bool(d&(1u<<v)));check(!(d&(1u<<v))||B[v]);}
        auto z=restrict_region(r,A,B).region;
        for(unsigned y=0;y<(1u<<z.size());++y){
            unsigned old=mask(A);int j=0;
            for(int v=0;v<r.size();++v)if(B[v]&&!A[v]){if(y&(1u<<j))old|=1u<<v;++j;}
            check(F(z,y)==F(r,old)-F(r,mask(A)));
        }
        // The residual identity also holds for arbitrary, non-closed A,B;
        // preserved head bits need closed masks, tested above.
    }
    for(bool reverse:{false,true}){
        Stats stats;auto ids=decompose(r,reverse,stats);
        for(int k=0;k<=K+1;++k){unsigned x=0;for(int v=0;v<r.size();++v)if(ids[v]>=k)x|=1u<<v;check(x==canonical(r,k));}
    }
}
int main(int argc,char**){
    std::mt19937 gen(20261022);int cases=0;
    const bool sanitizer_mode=argc>1;
    // Arbitrary legal orientations and heterogeneous residual budgets, not
    // only near-balanced greedy starts. Repeated occurrences and isolates stay.
    for(int n=0;n<=7;++n)for(int rep=0;rep<(sanitizer_mode?3:220);++rep){
        Region r;r.original.resize(n);std::iota(r.original.begin(),r.original.end(),0);r.load.assign(n,0);
        int m=n?gen()%13:0;
        for(int i=0;i<m;++i){
            std::vector<int>vs;for(int v=0;v<n;++v)if(gen()%2)vs.push_back(v);
            if(vs.empty())vs.push_back(gen()%n);
            int b=1+gen()%vs.size();std::vector<int>ord(vs.size());std::iota(ord.begin(),ord.end(),0);std::shuffle(ord.begin(),ord.end(),gen);
            Edge e{vs,std::vector<uint8_t>(vs.size(),0),b};
            for(int j=0;j<b;++j){e.head[ord[j]]=1;++r.load[vs[ord[j]]];}
            r.edges.push_back(e);
        }
        audit(r);++cases;
    }
    // All incidence orientations for every simple edge on three vertices,
    // and every legal budget, including exact ties at the integer threshold.
    for(unsigned a=1;a<(sanitizer_mode?2u:8u);++a)for(unsigned b=1;b<(sanitizer_mode?2u:8u);++b)
      for(unsigned h=1;h<8;++h)if((h&a)==h)
       for(unsigned j=1;j<8;++j)if((j&b)==j){
        Region r;r.original={0,1,2};r.load={0,0,0};
        for(auto pair:{std::make_pair(a,h),std::make_pair(b,j)}){
            Edge e;for(int v=0;v<3;++v)if(pair.first&(1u<<v)){e.vertices.push_back(v);e.head.push_back(bool(pair.second&(1u<<v)));if(e.head.back())++r.load[v];}
            e.budget=__builtin_popcount(pair.second);r.edges.push_back(e);
        }audit(r);++cases;
    }
    // Long paths: no recursive DFS; many retained isolated vertices.
    const int chain=sanitizer_mode?100:100000;
    const int universe=sanitizer_mode?250:250000;
    Orientation o(universe,1);for(int v=0;v<chain;++v)o.add_edge({v,v+1});
    auto r=from_orientation(std::move(o));Stats st;auto ids=decompose(r,true,st);
    for(int v=0;v<universe;++v)check(ids[v]==(v<=chain?1:0));
    bool rejected=false;try{restrict_region(r,{1},{0});}catch(std::invalid_argument const&){rejected=true;}check(rejected);
    std::cout<<"PASS: "<<cases<<" subset-oracle cases, both backends, all thresholds, residual identities, "
             <<chain+1<<"-node path, "<<universe<<" retained vertices\n";
}
