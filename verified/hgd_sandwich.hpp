#pragma once
// Optional exact region restriction for the two paper decomposition backends.
// Historical benchmark kernels are deliberately unchanged. Residual budgets
// are per edge: replacing them with min(delta, size) is NOT correct.
#include "hgd_optimized.hpp"

namespace hgdsandwich {
using hgdopt::MaxFlow;
struct Edge {
    std::vector<int> vertices;
    std::vector<uint8_t> head;
    int budget=0;
};
struct Region {
    std::vector<int> original,load;
    std::vector<Edge> edges;
    int size()const{return static_cast<int>(original.size());}
    long long incidences()const{
        long long z=0;for(auto const&e:edges)z+=e.vertices.size();return z;
    }
    void validate()const{
        std::vector<int>d(size(),0);
        for(auto const&e:edges){
            if(e.vertices.empty()||e.head.size()!=e.vertices.size()||e.budget<1||
               e.budget>static_cast<int>(e.vertices.size()))throw std::logic_error("residual budget domain");
            int heads=0;
            for(size_t j=0;j<e.vertices.size();++j){
                int v=e.vertices[j];
                if(v<0||v>=size()||e.head[j]>1)throw std::logic_error("residual incidence domain");
                if(e.head[j]){++heads;++d[v];}
            }
            if(heads!=e.budget)throw std::logic_error("residual orientation budget");
        }
        if(d!=load)throw std::logic_error("residual load mismatch");
    }
    Digraph graph()const{
        // Fully forced edges have no tail and cannot connect two vertices.
        size_t movable=0;for(auto const&e:edges)movable+=e.budget<static_cast<int>(e.vertices.size());
        Digraph g(checked_node_count(size(),movable));int factor=size();
        for(auto const&e:edges){
            if(e.budget==static_cast<int>(e.vertices.size()))continue;
            for(size_t j=0;j<e.vertices.size();++j)
                if(e.head[j])g.arc(factor,e.vertices[j]);else g.arc(e.vertices[j],factor);
            ++factor;
        }
        return g;
    }
};
inline Region from_orientation(Orientation o){
    o.validate();Region r;r.original.resize(o.n);std::iota(r.original.begin(),r.original.end(),0);
    r.load=std::move(o.indeg);
    for(auto&e:o.edges)if(e.active){
        const int budget=std::min(o.delta,static_cast<int>(e.vertices.size()));
        r.edges.push_back({std::move(e.vertices),std::move(e.is_head),budget});
    }
    return r;
}

// Direct propagation visits each factor/vertex once in seed-load order.
// forward=false: max descendant load; forward=true: min ancestor load.
inline std::vector<int> direct_bound(Region const&r,Digraph const&g,bool forward){
    int max_load=r.load.empty()?0:*std::max_element(r.load.begin(),r.load.end());
    std::vector<int>offset(static_cast<size_t>(max_load)+1,0),order(r.size());
    for(int d:r.load)++offset[d];
    int total=0;for(int&v:offset){int old=v;v=total;total+=old;}
    for(int v=0;v<r.size();++v)order[offset[r.load[v]]++]=v;
    std::vector<int>label(g.adj.size(),-1),pending;
    auto const&adj=forward?g.adj:g.rev;
    for(int ix=0;ix<r.size();++ix){
        int seed=order[forward?ix:r.size()-1-ix];
        if(label[seed]>=0)continue;
        const int value=r.load[seed];label[seed]=value;pending.clear();pending.push_back(seed);
        for(size_t j=0;j<pending.size();++j)for(int w:adj[pending[j]])
            if(label[w]<0){label[w]=value;pending.push_back(w);}
    }
    label.resize(r.size());return label;
}
inline std::vector<int> bound(Region const&r,Digraph &g,bool lower,bool reverse){
    if(reverse)return direct_bound(r,g,lower);
    Orientation shell(r.size(),1);shell.indeg=r.load;
    const int top=r.load.empty()?0:*std::max_element(r.load.begin(),r.load.end());
    if(lower){for(int &d:shell.indeg)d=top-d;g.adj.swap(g.rev);}
    auto a=shell.reachable_max(g);
    if(lower){g.adj.swap(g.rev);for(int&d:a)d=top-d;}
    return a;
}
struct Reduction {
    Region region;
    std::vector<int>parent_edge;
    std::vector<std::vector<int>>parent_pos;
};
// A and B must be predecessor closed, A subset B. Such sets preserve
// legality when restricting the current head bits to R=B\A.
inline Reduction restrict_region(Region const&r,std::vector<uint8_t>const&A,
                                 std::vector<uint8_t>const&B,bool track=false){
    if(A.size()!=r.original.size()||B.size()!=A.size())throw std::invalid_argument("sandwich mask size");
    Reduction out;auto&z=out.region;
    std::vector<int>index(r.size(),-1);
    for(int v=0;v<r.size();++v){
        if(A[v]&&!B[v])throw std::invalid_argument("sandwich A not subset B");
        if(B[v]&&!A[v]){index[v]=z.size();z.original.push_back(r.original[v]);z.load.push_back(0);}
    }
    for(size_t i=0;i<r.edges.size();++i){
        auto const&e=r.edges[i];Edge t;int outside=0;std::vector<int>positions;
        for(size_t j=0;j<e.vertices.size();++j){
            int v=e.vertices[j];outside+=!B[v];
            if(index[v]>=0){t.vertices.push_back(index[v]);t.head.push_back(e.head[j]);if(track)positions.push_back(j);}
        }
        t.budget=std::min(static_cast<int>(t.vertices.size()),std::max(0,e.budget-outside));
        if(!t.budget)continue;
        for(size_t j=0;j<t.vertices.size();++j)if(t.head[j])++z.load[t.vertices[j]];
        z.edges.push_back(std::move(t));
        if(track){out.parent_edge.push_back(i);out.parent_pos.push_back(std::move(positions));}
    }
    z.validate();return out;
}
inline void lift(Region &parent,Reduction const&sub){
    for(size_t i=0;i<sub.region.edges.size();++i){
        auto&e=parent.edges[sub.parent_edge[i]];
        for(size_t j=0;j<sub.parent_pos[i].size();++j){
            int p=sub.parent_pos[i][j],v=e.vertices[p];
            parent.load[v]+=int(sub.region.edges[i].head[j])-int(e.head[p]);
            e.head[p]=sub.region.edges[i].head[j];
        }
    }
    parent.validate();
}
inline long long pivot_flow(Region&r,int k){
    if(k<1)throw std::invalid_argument("pivot must be positive");
    if(k==1)return 0;
    int movable=0;for(auto const&e:r.edges)movable+=e.budget<static_cast<int>(e.vertices.size());
    int N=checked_node_count(r.size(),movable,2),src=N-2,sink=N-1;
    MaxFlow net(N);
    struct Ref{int edge,pos,origin,arc;};std::vector<Ref>refs;
    int factor=r.size();
    for(size_t i=0;i<r.edges.size();++i){auto const&e=r.edges[i];
        if(e.budget==static_cast<int>(e.vertices.size()))continue;
        for(size_t j=0;j<e.vertices.size();++j){int v=e.vertices[j],u=e.head[j]?factor:v,w=e.head[j]?v:factor;
            refs.push_back({static_cast<int>(i),static_cast<int>(j),u,net.add(u,w,1)});
        }
        ++factor;
    }
    int q=k-1;
    for(int v=0;v<r.size();++v){
        if(r.load[v]<q)net.add(src,v,q-r.load[v]);
        if(r.load[v]>q)net.add(v,sink,r.load[v]-q);
    }
    long long f=net.run(src,sink);
    for(auto const&ref:refs)if(net.g[ref.origin][ref.arc].cap==0){
        auto&e=r.edges[ref.edge];int v=e.vertices[ref.pos];
        r.load[v]+=e.head[ref.pos]?-1:1;e.head[ref.pos]^=1;
    }
    r.validate();return f;
}
struct Stats {
    long long regions=0,flow_calls=0,flow_units=0,full_query_vertices=0,
              query_vertices=0,full_query_incidences=0,query_incidences=0,
              forced_in=0,forced_out=0,balanced_vertices=0;
    int max_depth=0;
};
// Worklist entries form a vertex partition; release each parent after pushing
// its children. Incidences are never duplicated between the children.
inline std::vector<int> decompose(Region root,bool reverse,Stats&stats){
    const int n=root.size();std::vector<int>result(n,-1);
    int K=root.load.empty()?0:*std::max_element(root.load.begin(),root.load.end());
    struct Task{Region r;int lo,hi,depth;};std::vector<Task>todo;
    todo.push_back({std::move(root),0,K,0});
    while(!todo.empty()){
        Task task=std::move(todo.back());todo.pop_back();auto&r=task.r;
        ++stats.regions;stats.max_depth=std::max(stats.max_depth,task.depth);
        if(!r.size())continue;
        if(task.lo==task.hi){for(int v=0;v<r.size();++v)result[r.original[v]]=task.lo;continue;}
        auto g=r.graph();auto U=bound(r,g,false,reverse);
        bool balanced=true;for(int v=0;v<r.size();++v)if(U[v]-r.load[v]>1)balanced=false;
        if(balanced){for(int v=0;v<r.size();++v){
            if(U[v]<task.lo||U[v]>task.hi)throw std::logic_error("balanced interval certificate");
            result[r.original[v]]=U[v];
        }stats.balanced_vertices+=r.size();continue;}
        auto L=bound(r,g,true,reverse);
        // Only an upper bound is needed to shrink the integer search interval.
        int top=0;for(int u:U)top=std::max(top,u);task.hi=std::min(task.hi,top);
        if(task.lo>task.hi)throw std::logic_error("empty sandwich interval");
        if(task.lo==task.hi){for(int v=0;v<r.size();++v)result[r.original[v]]=task.lo;continue;}
        int k=task.lo+(task.hi-task.lo)/2+1;
        std::vector<uint8_t>A(r.size()),B(r.size()),S(r.size());
        for(int v=0;v<r.size();++v){A[v]=L[v]>=k;B[v]=U[v]>=k;S[v]=A[v];stats.forced_in+=A[v];stats.forced_out+=!B[v];}
        stats.full_query_vertices+=r.size();stats.full_query_incidences+=r.incidences();
        auto sub=restrict_region(r,A,B,true);auto&z=sub.region;
        stats.query_vertices+=z.size();stats.query_incidences+=z.incidences();
        if(z.size()){
            ++stats.flow_calls;stats.flow_units+=pivot_flow(z,k);
            auto zg=z.graph();auto M=bound(z,zg,false,reverse);
            std::vector<int>local_of_parent(r.size(),-1);int next=0;
            for(int v=0;v<r.size();++v)if(B[v]&&!A[v])local_of_parent[v]=next++;
            for(int v=0;v<z.size();++v)
                if(z.load[v]<k-1&&M[v]>=k)throw std::logic_error("threshold flow certificate");
            for(int v=0;v<r.size();++v)if(local_of_parent[v]>=0)S[v]=M[local_of_parent[v]]>=k;
            lift(r,sub);
        }
        // Release flow/graph scratch before allocating the two children.
        sub=Reduction{};g=Digraph(0);
        std::vector<uint8_t>empty(r.size(),0),all(r.size(),1);
        auto upper=restrict_region(r,empty,S).region;
        auto lower=restrict_region(r,S,all).region;
        if(lower.size())todo.push_back({std::move(lower),task.lo,k-1,task.depth+1});
        if(upper.size())todo.push_back({std::move(upper),k,task.hi,task.depth+1});
    }
    for(int x:result)if(x<0)throw std::logic_error("unassigned IDN");
    return result;
}
} // namespace hgdsandwich
