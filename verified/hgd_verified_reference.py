"""Certified reference implementation for (k, delta)-dense hypergraph decomposition.

This code is designed for correctness checks, NOT as a performance replacement for
an optimized C++ implementation. Hyperedges are arbitrary finite vertex subsets;
parallel hyperedges are allowed. Vertices are indexed 0,...,n-1.

Steps for a fixed delta:
  1. Minimum-cost integral flow finds an orientation minimizing sum_v indegree[v]^2.
  2. This orientation is egalitarian (any reversible path would lower the cost).
  3. SCC condensation computes r_v = max_{v reaches w} indegree[w].
  4. D_{k,delta} = {v: r_v >= k}, including k=0 and isolated vertices.

Runtime for an explicit incidence representation of size L=sum_e |e|:
O(R (n+m+L) log(n+m+2) + n+m+L), R=sum_e min(delta,|e|).
Space O(n+m+L). Bounds assume binary-heap shortest paths with potentials.
"""
from dataclasses import dataclass
from heapq import heappush, heappop
from collections import deque

@dataclass
class Arc:
    to: int
    rev: int
    cap: int
    cost: int

class MinCostFlow:
    def __init__(self, n):
        self.g = [[] for _ in range(n)]

    def add(self, u, v, cap, cost):
        gu = self.g[u]
        gv = self.g[v]
        i = len(gu)
        j = len(gv) + int(u == v)  # no self-loops in this network
        gu.append(Arc(v, j, cap, cost))
        gv.append(Arc(u, i, 0, -cost))
        return i

    def send(self, src, sink, need):
        n = len(self.g)
        pot = [0] * n
        ans = 0
        for sent in range(need):
            INF = 10**25
            dist = [INF] * n
            prev = [None] * n
            dist[src] = 0
            heap = [(0, src)]
            while heap:
                du,u = heappop(heap)
                if du != dist[u]:
                    continue
                for i,a in enumerate(self.g[u]):
                    if a.cap <= 0:
                        continue
                    w = a.cost + pot[u] - pot[a.to]
                    if w < 0:
                        raise AssertionError('negative reduced cost: flow logic error')
                    cand=du+w
                    if cand < dist[a.to]:
                        dist[a.to] = cand
                        prev[a.to] = (u,i)
                        heappush(heap,(cand,a.to))
            if dist[sink] >= INF:
                raise RuntimeError('infeasible hypergraph degree allocation')
            for v in range(n):
                if dist[v] < INF:
                    pot[v] += dist[v]
            v=sink
            cost=0
            while v != src:
                u,i=prev[v]
                a=self.g[u][i]
                cost+=a.cost
                a.cap-=1
                self.g[v][a.rev].cap+=1
                v=u
            ans+=cost
        return ans

def _normalize(n,edges):
    if not isinstance(n,int) or n<0:
        raise ValueError('n must be a nonnegative integer')
    out=[]
    for e in edges:
        x=tuple(sorted(set(e)))
        if len(x)==0:
            raise ValueError('empty hyperedges are not allowed')
        if any(not isinstance(v,int) or v<0 or v>=n for v in x):
            raise ValueError('hyperedge contains an invalid vertex')
        out.append(x)
    return out

def egalitarian_orientation(n, edges, delta):
    """Return (heads, indegrees, squared-load objective value)."""
    edges=_normalize(n,edges)
    if not isinstance(delta,int) or delta<1:
        raise ValueError('delta must be >=1')
    m=len(edges)
    N=n+m+2
    SRC=n+m
    SINK=n+m+1
    ff=MinCostFlow(N)
    degree=[0]*n
    for e in edges:
        for v in e:degree[v]+=1
    tracked=[]
    R=0
    for i,e in enumerate(edges):
        edge_node=n+i
        r=min(delta,len(e))
        R+=r
        ff.add(SRC,edge_node,r,0)
        curr=[]
        for v in e:
            pos=ff.add(edge_node,v,1,0)
            curr.append((v,pos))
        tracked.append(curr)
    for v in range(n):
        for j in range(1,degree[v]+1):
            ff.add(v,SINK,1,2*j-1)
    best=ff.send(SRC,SINK,R)
    heads=[]
    indeg=[0]*n
    for i,pairs in enumerate(tracked):
        edge_node=n+i
        hv=[]
        for v,pos in pairs:
            if ff.g[edge_node][pos].cap==0:
                hv.append(v)
                indeg[v]+=1
        assert len(hv)==min(delta,len(edges[i]))
        heads.append(tuple(hv))
    assert best==sum(x*x for x in indeg)
    return heads,indeg,best

def reachability_max_indegree(n, edges, heads, indegrees):
    """Compute per-vertex max reachable indegree via SCC on incidence DAG."""
    edges=_normalize(n,edges)
    m=len(edges)
    N=n+m
    adj=[[] for _ in range(N)]
    rev=[[] for _ in range(N)]
    for i,(e,hd) in enumerate(zip(edges,heads)):
        s=set(hd)
        enode=n+i
        for v in e:
            if v in s:
                adj[enode].append(v)
                rev[v].append(enode)
            else:
                adj[v].append(enode)
                rev[enode].append(v)
    seen=[False]*N
    order=[]
    for start in range(N):
        if seen[start]:continue
        seen[start]=True
        st=[(start,0)]
        while st:
            u,j=st[-1]
            if j<len(adj[u]):
                v=adj[u][j];st[-1]=(u,j+1)
                if not seen[v]:
                    seen[v]=True
                    st.append((v,0))
            else:
                order.append(u)
                st.pop()
    comps=[-1]*N
    sizes=[]
    for start in reversed(order):
        if comps[start]>=0:continue
        cid=len(sizes)
        st=[start];comps[start]=cid;num=0
        while st:
            u=st.pop();num+=1
            for v in rev[u]:
                if comps[v]<0:
                    comps[v]=cid;st.append(v)
        sizes.append(num)
    cnum=len(sizes)
    max_in_comp=[-1]*cnum
    for v in range(n):
        ci=comps[v]
        max_in_comp[ci]=max(max_in_comp[ci], indegrees[v])
    dag=[set() for _ in range(cnum)]
    indeg_c=[0]*cnum
    for u in range(N):
        cu=comps[u]
        for v in adj[u]:
            cv=comps[v]
            if cu!=cv and cv not in dag[cu]:
                dag[cu].add(cv)
                indeg_c[cv]+=1
    q=deque(i for i,d in enumerate(indeg_c) if d==0)
    topo=[]
    while q:
        u=q.popleft();topo.append(u)
        for v in dag[u]:
            indeg_c[v]-=1
            if indeg_c[v]==0:q.append(v)
    assert len(topo)==cnum
    reachmax=max_in_comp[:]
    for u in reversed(topo):
        for v in dag[u]:
            reachmax[u]=max(reachmax[u],reachmax[v])
    ret=[reachmax[comps[v]] for v in range(n)]
    assert all(ret[v]>=indegrees[v] for v in range(n))
    return ret

def compute(n, edges, delta):
    """Return a dictionary with canonical IDNs and the fair orientation."""
    edges=_normalize(n,edges)
    heads, indegrees, potential=egalitarian_orientation(n, edges, delta)
    labels=reachability_max_indegree(n,edges,heads,indegrees)
    return {'idn':labels, 'indegrees':indegrees, 'heads':heads, 'potential':potential}

def dense_set(result,k):
    if k<0:raise ValueError('k must be nonnegative')
    return frozenset(v for v,r in enumerate(result['idn']) if r>=k)

def insert_and_recompute(n,edges,new_edge,delta):
    """Certified dynamic fallback: recompute the affected fixed-delta state."""
    return compute(n,list(edges)+[new_edge],delta)

def delete_and_recompute(n,edges,index,delta):
    """Certified dynamic fallback: remove one specified hyperedge occurrence."""
    new_edges=list(edges)
    new_edges.pop(index)
    return compute(n,new_edges,delta)

if __name__=='__main__':
    # one hyperedge + deletion: both vertices' labels must become zero
    one=compute(2,[(0,1)],1)
    gone=delete_and_recompute(2,[(0,1)],0,1)
    assert one['idn']==[1,1] and gone['idn']==[0,0]
    # nontrivial 6-node instance where printed Algorithm 4 may give D_3 = V,
    # although the canonical D_3 is empty.
    six=[(0,1),(0,2),(0,4),(0,5),(1,2),(2,4),
         (3,1),(3,2),(3,4),(3,5),(4,1),(4,5)]
    result=compute(6,six,1)
    assert dense_set(result,3)==frozenset()
    assert result['idn']==[2,2,2,2,2,2]
    print('PASS: deletion, Algorithm-4 counterexample, SCC labels')
    print('6-node certified IDNs:',result['idn'],'indegrees:',result['indegrees'])


def pivot_stable_mining(n, edges, delta, k):
    """Corrected, flow-free DSM-ALL replacement for a *single* D_{k,delta}.

    A single multi-source BFS looks for s->t with d_s<=k-2 and d_t>=k,
    and swaps path incidences. The threshold excess
       Phi_k = sum_v max(0, indegree[v]-(k-1))
    drops by EXACTLY one on every swap, so there are at most R swaps.
    Complexity O((R+1)*(n+m+L)), memory O(n+m+L).

    This returns the canonical D_{k,delta} without computing a globally
    egalitarian orientation, which is stronger than necessary.
    """
    edges=_normalize(n,edges)
    if delta<1 or k<0:
        raise ValueError('delta >=1, k>=0 required')
    m=len(edges)
    # Arbitrary valid orientation (fixed vertex order for reproducibility).
    hsets=[set(e[:min(delta,len(e))]) for e in edges]
    di=[0]*n
    for h in hsets:
        for v in h:di[v]+=1
    R=sum(map(len,hsets))
    phi=lambda: sum(max(0,d-(k-1)) for d in di)
    initial_phi=phi()
    count=0
    def factor_graph():
        adj=[[] for _ in range(n+m)]
        rev=[[] for _ in range(n+m)]
        for i,e in enumerate(edges):
            h=hsets[i];ev=n+i
            for v in e:
                if v in h:
                    adj[ev].append(v)
                    rev[v].append(ev)
                else:
                    adj[v].append(ev)
                    rev[ev].append(v)
        return adj,rev
    while True:
        adj,rev=factor_graph()
        pred=[-1]*(n+m)
        q=deque()
        for u in range(n):
            if di[u]<=k-2:
                pred[u]=u
                q.append(u)
        t=-1
        while q and t<0:
            u=q.popleft()
            if u<n and di[u]>=k:
                t=u
                break
            for v in adj[u]:
                if pred[v]<0:
                    pred[v]=u
                    q.append(v)
        if t<0:
            break
        path=[t]
        while pred[path[-1]]!=path[-1]:
            path.append(pred[path[-1]])
        path.reverse()
        assert path[0]<n and path[-1]<n and len(path)%2==1
        source=path[0]
        before=phi()
        for i in range(0,len(path)-2,2):
            u,ei,v=path[i:i+3]
            assert ei>=n and u<n and v<n
            hi=hsets[ei-n]
            assert u not in hi and v in hi
            hi.remove(v);hi.add(u)
        di[source]+=1
        di[t]-=1
        count+=1
        assert phi()==before-1
        assert count<=initial_phi<=R
    # Predecessor closure in the final orientation.
    S={u for u in range(n) if di[u]>=k}
    q=deque(S)
    seen=set(S)
    while q:
        v=q.popleft()
        for p in rev[v]:
            if p not in seen:
                seen.add(p);q.append(p)
    D=frozenset(u for u in range(n) if u in seen)
    return {'dense':D, 'indegrees':di, 'heads':[tuple(sorted(h)) for h in hsets],
            'reversals':count,'initial_potential':initial_phi,
            'k_stable':True}
