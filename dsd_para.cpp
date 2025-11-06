// DSD+：外层 δ 并行 + DIVIDE 任务并行（最小可运行）
// - 外层：delta 并行（omp parallel + for）
// - 内层：仅对 divid() 的左右子区间用 OpenMP tasks（带阈值）
// - 每线程上下文复用；BASE_E 只读

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <numeric>
#include <queue>
#include <string>
#include <vector>
#include <omp.h>
using namespace std;

// -------------------- Base read-only hypergraph --------------------
struct BaseHyperedge {
    vector<int> varr;  // only vertex list, no orientation state here
};
static vector<BaseHyperedge> BASE_E;  // |E|
static int BASE_nodenum = 0;
static int BASE_edgenum = 0;
static int BASE_dmax    = 0;  // max degree over all vertices (undirected)
static int BASE_deltamax= 0;  // max hyperedge size

// -------------------- Per-delta local context --------------------
struct Vertex {
    int id=0, degree=0, indegree=0;
    vector<int> tedge; // outgoing edges (vtarr owner)
    vector<int> hedge; // incoming edges (vharr owner)
};
struct Hyperedge {
    const vector<int>* varr=nullptr; // reference to BASE
    vector<int> vtarr; // tail part (j >= delta)
    vector<int> vharr; // head part (j <  delta)
    int v=0, h=0;
};

// 工具：规范化（有序去重）
static inline void normalize_vec(vector<int>& v){
    sort(v.begin(), v.end());
    v.erase(unique(v.begin(), v.end()), v.end());
}

struct Context {
    int nodenum, edgenum, dmax, deltamax;
    vector<Vertex>    V;          // 1..nodenum
    vector<Hyperedge> E;          // 0..edgenum-1
    vector<vector<int>> dkv;      // dkv[k]: nodes in D_k set
    vector<char> dk;              // mark set

    // 任务粒度阈值（区间长度达到才生成 task）
    int task_threshold;

    Context(int N, int M, int dmax_, int delt_)
        : nodenum(N), edgenum(M), dmax(dmax_), deltamax(delt_),
          V(N+1), E(M), dkv(dmax_+2), dk(N+1,0),
          task_threshold(4096)   // 可按规模改 2k~64k 做实验
    {
        for(int i=1;i<=nodenum;i++){ V[i].id=i; V[i].degree=0; V[i].indegree=0; }
        for(int ei=0; ei<edgenum; ++ei){
            E[ei].varr = &BASE_E[ei].varr;
            E[ei].v = (int)BASE_E[ei].varr.size();
        }
    }

    // 清理“状态”，但尽量保留 capacity（clear 不释放内存）
    void clear_state(){
        for(int i=1;i<=nodenum;i++){
            V[i].indegree=0;
            V[i].tedge.clear();
            V[i].hedge.clear();
        }
        for(auto &vec : dkv) vec.clear();
        std::fill(dk.begin(), dk.end(), 0);
        for(int j=0;j<edgenum;j++){
            E[j].vtarr.clear();
            E[j].vharr.clear();
            E[j].h = 0;
        }
    }

    // 本线程复用：在每个 δ 开始调用
    void reset(){
        clear_state();
        dkv[1].clear();
        dkv[1].reserve(nodenum);  // 容量保留
        for(int i=1;i<=nodenum;i++) dkv[1].push_back(i); // 已有序
    }

    // orientation(delta) 带预分配
    void orientation(int delta){
        for(int i=0;i<edgenum;i++){
            auto &he = E[i];
            const auto &arr = *he.varr;
            int vsz = (int)arr.size();
            he.h = 1;

            he.vharr.clear();
            he.vtarr.clear();
            // 预分配，按本次需要大小预留
            int head = std::min(delta, vsz);
            he.vharr.reserve(head);
            he.vtarr.reserve(vsz - head);

            for(int j=0; j<head; ++j){
                int v = arr[j];
                he.vharr.push_back(v);
                V[v].indegree++;
                V[v].hedge.push_back(i);
            }
            for(int j=head; j<vsz; ++j){
                int v = arr[j];
                he.vtarr.push_back(v);
                V[v].tedge.push_back(i);
            }
        }
    }

    bool reachdk(int vi, int k){
        vector<char> seen(nodenum+1, 0);
        queue<int>q;
        for(int ee: V[vi].tedge){
            for(int vv: E[ee].vharr){
                if(V[vv].indegree>=k) return true;
                if(!seen[vv] && V[vv].indegree==k-1){ seen[vv]=1; q.push(vv); }
            }
        }
        while(!q.empty()){
            int x=q.front(); q.pop();
            for(int ee: V[x].tedge){
                for(int vv: E[ee].vharr){
                    if(V[vv].indegree>=k) return true;
                    if(!seen[vv] && V[vv].indegree==k-1){ seen[vv]=1; q.push(vv); }
                }
            }
        }
        return false;
    }

    void reverse_edge(int vi,int vj,int ee){
        // vi: from head->tail, vj: from tail->head
        auto& vx = V[vi]; auto& vy = V[vj]; auto& he = E[ee];
        // vi indegree--
        vx.indegree--;
        {
            auto it = find(vx.hedge.begin(), vx.hedge.end(), ee);
            if(it!=vx.hedge.end()) vx.hedge.erase(it);
        }
        vx.tedge.push_back(ee);
        // he: move vi from vharr to vtarr
        {
            auto it = find(he.vharr.begin(), he.vharr.end(), vi);
            if(it!=he.vharr.end()) he.vharr.erase(it);
        }
        he.vtarr.push_back(vi);

        // vj indegree++
        {
            auto it = find(he.vtarr.begin(), he.vtarr.end(), vj);
            if(it!=he.vtarr.end()) he.vtarr.erase(it);
        }
        he.vharr.push_back(vj);

        vy.indegree++;
        {
            auto it = find(vy.tedge.begin(), vy.tedge.end(), ee);
            if(it!=vy.tedge.end()) vy.tedge.erase(it);
        }
        vy.hedge.push_back(ee);
    }

    void reachout(int vi,int k){
        queue<int>q;
        for(int i=0;i<(int)V[vi].hedge.size();i++){
            int ee=V[vi].hedge[i];
            for(int vv: E[ee].vtarr){
                if(!dk[vv] && (V[vi].indegree - V[vv].indegree) >= 2){
                    reverse_edge(vi,vv,ee);
                    if(V[vv].indegree>=k) dk[vv]=1;
                    q.push(vv);
                    i--;
                    break;
                }
            }
        }
        while(!q.empty()){ int x=q.front(); q.pop(); reachout(x,k); }
    }

    void reachin(int vi,int k){
        queue<int>q;
        for(int i=0;i<(int)V[vi].tedge.size();i++){
            int ee=V[vi].tedge[i];
            for(int vv: E[ee].vharr){
                if(!dk[vv] && (V[vv].indegree - V[vi].indegree) >= 2){
                    reverse_edge(vv,vi,ee);
                    q.push(vv);
                    i--;
                    break;
                }
            }
        }
        while(!q.empty()){ int x=q.front(); q.pop(); reachin(x,k); }
    }

    void outk(int vi,int k){
        int initd=V[vi].indegree;
        for(int i=0;i<(int)V[vi].hedge.size();i++){
            int ee=V[vi].hedge[i];
            for(int vv: E[ee].vtarr){
                if(dk[vv] && (V[vi].indegree - V[vv].indegree) >= 2){
                    reverse_edge(vi,vv,ee); i--; break;
                }
            }
        }
        for(int i=0;i<(int)V[vi].tedge.size();i++){
            int ee=V[vi].tedge[i];
            for(int vv: E[ee].vharr){
                if(dk[vv] && (V[vv].indegree - V[vi].indegree) >= 2){
                    reverse_edge(vv,vi,ee); i--; break;
                }
            }
        }
        if(initd>V[vi].indegree){
            reachin(vi,k);
            if(V[vi].indegree<k) dk[vi]=0;
        }else if(initd<V[vi].indegree){
            reachout(vi,k);
            if(V[vi].indegree<k) dk[vi]=0;
        }else{
            dk[vi]=0;
        }
    }

    void finddk(int k){
        vector<int> veck;
        veck.reserve(dkv[k].size() + 16);
        for(int i=1;i<=nodenum;i++){
            if(dk[i]) veck.push_back(i);
            if(!dk[i] && V[i].indegree==k-1 && reachdk(i,k)) veck.push_back(i);
        }
        normalize_vec(veck);
        dkv[k].swap(veck);
    }

    void peelk(int k,int l){
        for(int vv: dkv[l]) if(dk[vv]) reachout(vv,k);
        while(true){
            bool flag=false;
            for(int vv: dkv[l]){
                if(dk[vv] && V[vv].indegree<k){
                    flag=true; outk(vv,k);
                }
            }
            if(!flag) break;
        }
    }

    vector<int> getlayer(int kk, int u, int l){
        vector<int> difference;
        difference.reserve(dkv[l].size());
        // dkv[l], dkv[u] already sorted
        set_difference(dkv[l].begin(), dkv[l].end(),
                       dkv[u].begin(), dkv[u].end(),
                       back_inserter(difference));
        std::fill(dk.begin(), dk.end(), 0);
        for(int vv: difference) dk[vv]=1;

        while(true){
            bool flag=false;
            for(int vv: difference){
                if(dk[vv] && V[vv].indegree<kk){
                    flag=true; outk(vv,kk);
                }
            }
            if(!flag) break;
        }
        vector<int> veck;
        veck.reserve(difference.size() + dkv[u].size());
        for(int vv: difference){
            if(dk[vv]) veck.push_back(vv);
            if(!dk[vv] && V[vv].indegree==kk-1 && reachdk(vv,kk)) veck.push_back(vv);
        }
        // merge D_u
        veck.insert(veck.end(), dkv[u].begin(), dkv[u].end());
        normalize_vec(veck);
        return veck;
    }

    bool searchp(int xp){
        if(!dkv[xp].empty()) return true;
        std::fill(dk.begin(), dk.end(), 0);
        for(int i=1;i<=nodenum;i++) if(V[i].indegree>=xp) dk[i]=1;
        for(int i=1;i<=nodenum;i++) if(dk[i]) reachout(i,xp);
        while(true){
            bool flag=false;
            for(int i=1;i<=nodenum;i++){
                if(dk[i] && V[i].indegree<xp){ flag=true; outk(i,xp); }
            }
            if(!flag) break;
        }
        finddk(xp);
        return !dkv[xp].empty();
    }

    int computep(int maxk){
        if(searchp(maxk)) return maxk;
        while(true){
            maxk/=2;
            if(maxk<=1 || searchp(maxk)) return maxk;
        }
    }

    // --------- 任务并行的 divid ----------
    void divid(int u, int l){
        if(u==l || u==l+1) return;
        if(dkv[u].size()==dkv[l].size()){
            if(dkv[u].empty()) return;
            for(int j=l+1;j<u;j++) dkv[j]=dkv[u];
            return;
        }
        int kk=(u+l+1)/2;

        // 先串行填好 mid（getlayer 会修改 V/E/dk）
        if(dkv[kk].empty()){
            dkv[kk]=getlayer(kk,u,l);
        }

        // 左右分支是纯 divid（只写 dkv[...])，可并行为 tasks
        int left_len  = kk - l;
        int right_len = u  - kk;

 
        // 用一个普通指针 self 捕获到任务里
        Context* self = this;

        #pragma omp taskgroup
        {
            // 左支：满足阈值才生成 task，否则当前线程直接递归，避免微任务
            #pragma omp task default(none) firstprivate(kk,l,self) if(left_len >= task_threshold)
            { self->divid(kk, l); }
            if(left_len < task_threshold) self->divid(kk, l);

            // 右支同理
            #pragma omp task default(none) firstprivate(u,kk,self) if(right_len >= task_threshold)
            { self->divid(u, kk); }
            if(right_len < task_threshold) self->divid(u, kk);
        }
    }

    // --------- 保持串行的 dividsearch ----------
    void dividsearch(int u,int l){
        if(u==l||u==l+1) return;
        if(dkv[u].size()==dkv[l].size()){
            if(dkv[u].empty()) return;
            for(int j=l+1;j<u;j++) dkv[j]=dkv[u];
            return;
        }
        vector<int> difference;
        difference.reserve(dkv[l].size());
        set_difference(dkv[l].begin(), dkv[l].end(),
                       dkv[u].begin(), dkv[u].end(),
                       back_inserter(difference));
        int mid=(u+l+1)/2;

        std::fill(dk.begin(), dk.end(), 0);
        for(int vv: difference) if(V[vv].indegree>=mid) dk[vv]=1;

        peelk(mid,l);

        vector<int> veck;
        veck.reserve(difference.size());
        for(int vv: difference){
            if(dk[vv]) veck.push_back(vv);
            if(!dk[vv] && V[vv].indegree==mid-1 && reachdk(vv,mid)) veck.push_back(vv);
        }
        normalize_vec(veck);
        dkv[mid].swap(veck);

        if(dkv[mid].empty()){
            dividsearch(mid,l);      // 空的一侧继续“搜索式”细分（串行）
        }else{
            divid(mid,l);            // 左侧改用任务并行的纯分治
            dividsearch(u,mid);      // 右侧仍然串行搜索
        }
    }
};

// -------------------- I/O: read base edges --------------------
void read_base_edges(const string& path){
    ifstream fin(path);
    if(!fin){ cerr<<"error open: "<<path<<"\n"; exit(1); }
    string s;
    while(fin>>s){
        BaseHyperedge e;
        int ps=0;
        for(int i=0;i<(int)s.size();++i){
            if(s[i]==','){
                int v=stoi(s.substr(ps,i-ps));
                e.varr.push_back(v);
                ps=i+1;
            }
        }
        int v=stoi(s.substr(ps));
        e.varr.push_back(v);
        BASE_E.push_back(move(e));
    }
    BASE_edgenum=(int)BASE_E.size();

    // compute nodenum, degrees, dmax, deltamax
    int N=0, deltamax=0;
    for(const auto& e: BASE_E){
        deltamax=max(deltamax, (int)e.varr.size());
        for(int v: e.varr) N=max(N, v);
    }
    BASE_nodenum=N;
    vector<int> degree(N+1,0);
    for(const auto& e: BASE_E)
        for(int v: e.varr) degree[v]++;

    int dmax=0;
    for(int i=1;i<=N;i++) dmax=max(dmax, degree[i]);

    BASE_dmax=dmax;
    BASE_deltamax=deltamax;

    cerr<<"[read] |V|="<<BASE_nodenum<<", |E|="<<BASE_edgenum
        <<", dmax="<<BASE_dmax<<", deltamax="<<BASE_deltamax<<"\n";
}

// -------------------- Top-level: delta-parallel --------------------
int main(int argc, char** argv){
    // usage: ./dsdp [num_threads] [dataset_path]
    int nthreads = (argc>1? max(1,atoi(argv[1])) : omp_get_max_threads());
    string dataset = (argc>2? string(argv[2]) : "dataset/trivago2.txt");

    omp_set_dynamic(0);
    omp_set_max_active_levels(1); // 限制嵌套并行层数；task 不受此限制
    omp_set_num_threads(nthreads);

    cout<<"OpenMP threads: "<<omp_get_max_threads()<<"\n";
    read_base_edges(dataset);

    auto T0 = chrono::steady_clock::now();

    #pragma omp parallel
    {
        // 每线程只创建一次 Context，循环里 reset() 复用
        Context ctx(BASE_nodenum, BASE_edgenum, BASE_dmax, BASE_deltamax);
        ctx.dkv[1].reserve(ctx.nodenum);

        // 用 dynamic 让空闲线程更容易偷取 task（更配合 task 并行）
        #pragma omp for schedule(dynamic)
        for(int delta=1; delta<=BASE_deltamax; ++delta){
            ctx.reset();

            auto t1 = chrono::steady_clock::now();
            ctx.orientation(delta);
            auto t2 = chrono::steady_clock::now();

            int p = ctx.computep(ctx.dmax);
            int maxk_local = (ctx.dmax >= 2*p ? 2*p : ctx.dmax);

            ctx.dividsearch(maxk_local, p);
            auto t3 = chrono::steady_clock::now();

            ctx.divid(p, 1);  // 这里会在内部按阈值生成 tasks
            auto t4 = chrono::steady_clock::now();

            #pragma omp critical
            {
                cerr<<"[δ="<<delta<<"] orient: "
                    << chrono::duration<double>(t2-t1).count()
                    << " s, dividsearch: "
                    << chrono::duration<double>(t3-t2).count()
                    << " s, divid: "
                    << chrono::duration<double>(t4-t3).count()
                    << " s\n";
            }
        }
    }

    auto T1 = chrono::steady_clock::now();
    cout<< chrono::duration<double>(T1-T0).count() <<" seconds\n";
    return 0;
}