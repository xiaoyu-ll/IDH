#include<iostream>
#include<fstream>
#include<sstream>
#include<string>
#include<algorithm>
#include<cmath>
#include<map>
#include<vector>
#include<stdlib.h>
#include <chrono>
#include <iomanip>
#include<stack>
#include<queue>
#include <cstring>
#include <unordered_set>
#include "ext_metrics.hpp"  
// kdelta_global.cpp
// 一次性收集“所有 δ 的所有层序”，并用 ext_metrics.hpp 计算全局指标（主表 + 扩展表）


using namespace std;


#include "ext_metrics.hpp"


static const int VM = 1730000;  // V 侧最大 id 上界（顶点）
static const int UM = 2340000;  // U 侧最大 id 上界（把 U 视作“超边/二分另一侧”）

// ===== 原始图邻接（仅用于“统计口径”，永远不改动） =====
static int vnum_init = 0, unum_init = 0;              // 原始图最大 id
static vector<vector<int>> V2U_init(VM + 1);          // v -> [u...]
static vector<vector<int>> U2V_init(UM + 1);          // u -> [v...]

// ===== (k,δ)-dense 用到的数据结构 =====
struct Vertex {
    int indegree = 0;              // 当前取向后的入度
    vector<int> tedge;             // 出集合（节点 -> 超边 id）
    vector<int> hedge;             // 入集合（节点 <- 超边 id）
};
struct Hyperedge {
    int v = 0;                     // 该“超边”（u）包含的顶点数 |varr|
    int h = 0;                     // 该超边中被分到“入”的顶点个数（取向后）
    vector<int> varr;              // 原始：此 u 相连的所有 v
    vector<int> vtarr;             // 取向后：被分到“出”的 v
    vector<int> vharr;             // 取向后：被分到“入”的 v
};

// —— 全局（供 (k,δ) 算法使用）——
static int nodenum = 0;            // V 侧最大 id（按数据实际出现）
static int edgenum = 0;            // U 侧最大 id
static int deltamax = 0;           // δ 的最大可取值（= max_u |N(u)|）
static vector<Vertex>    vertex;   // 大小 nodenum+1
static vector<Hyperedge> hyperedge;// 大小 edgenum+1
static vector<vector<int>> dkv;    // dkv[k] = 第 k 层的 V 节点集合
static vector<char> dk;            // 剥皮标记（是否还在 k-核里）
static unordered_map<int,bool> fh; // reachdk 的访问标记

// ===== 读入（每行 "v,u"；去重）并初始化 (k,δ) 需要的结构 =====
static void readedge(const string& path="dataset/mlm2.txt") {
    ifstream fin(path);
    if(!fin){ cerr<<"[Error] cannot open "<<path<<"\n"; exit(1); }

    unordered_set<unsigned long long> S; S.reserve(1<<22);
    string s; long long E = 0;
    int vmax = 0, umax = 0;

    while(fin >> s){
        int p = (int)s.find(',');
        if(p == (int)string::npos) continue;
        int v = stoi(s.substr(0,p));
        int u = stoi(s.substr(p+1));
        if(v < 0 || u < 0) continue;
        if(v >= VM || u >= UM){ cerr<<"[ERR] id exceeds VM/UM, enlarge.\n"; exit(1); }

        unsigned long long key = ((unsigned long long)v<<32) | (unsigned long long)u;
        if(!S.insert(key).second) continue; // 去重

        // —— 原始图邻接（统计口径） ——
        V2U_init[v].push_back(u);
        U2V_init[u].push_back(v);
        vmax = max(vmax, v);
        umax = max(umax, u);
        ++E;
    }
    fin.close();

    vnum_init = vmax;
    unum_init = umax;

    // ===== 初始化 (k,δ) 的容器 =====
    nodenum = vmax;
    edgenum = umax;
    vertex.assign(nodenum + 1, Vertex{});
    hyperedge.assign(edgenum + 1, Hyperedge{});

    // 把原始邻接拷贝到 hyperedge[].varr，并统计 δ 上限
    deltamax = 0;
    for(int u=0; u<=edgenum; ++u){
        hyperedge[u].varr = U2V_init[u];            // u 的邻居 v 列表
        hyperedge[u].v    = (int)hyperedge[u].varr.size();
        deltamax = max(deltamax, hyperedge[u].v);
    }

    cerr<<"[Load] V="<<(vnum_init+1)<<" U="<<(unum_init+1)<<" E="<<E
        <<"  deltamax="<<deltamax<<"\n";
    if(deltamax == 0){ cerr<<"[Error] empty graph.\n"; exit(1); }
}

// ===== 取向：把每条超边 u 的前 δ 个 v 放进 vharr(入)，其余进 vtarr(出) =====
static void orientation(int delta){
    // 清空 (k,δ) 状态
    for(int v=0; v<=nodenum; ++v){
        vertex[v].indegree = 0;
        vertex[v].hedge.clear();
        vertex[v].tedge.clear();
    }
    for(int u=0; u<=edgenum; ++u){
        hyperedge[u].vharr.clear();
        hyperedge[u].vtarr.clear();
        hyperedge[u].h = 0;
    }

    // 逐超边分配
    for(int u=0; u<=edgenum; ++u){
        const auto &ns = hyperedge[u].varr;
        int sz = (int)ns.size();
        int take = min(delta, sz);
        hyperedge[u].h = take;

        // 入集合
        for(int i=0; i<take; ++i){
            int v = ns[i];
            hyperedge[u].vharr.push_back(v);
            vertex[v].indegree++;
            vertex[v].hedge.push_back(u);
        }
        // 出集合
        for(int i=take; i<sz; ++i){
            int v = ns[i];
            hyperedge[u].vtarr.push_back(v);
            vertex[v].tedge.push_back(u);
        }
    }
}


static bool reachdk(int vi, int k){
    queue<int>q;
    // 1 跳：vi 的所有出边 u 的入集合里，若存在 indegree>=k 的顶点即可成功
    for(size_t i=0;i<vertex[vi].tedge.size();++i){
        int u = vertex[vi].tedge[i];
        for(int vv: hyperedge[u].vharr){
            if(vertex[vv].indegree >= k) return true;
            if(!fh[vv] && vertex[vv].indegree==k-1){
                fh[vv] = true; q.push(vv);
            }
        }
    }
    // BFS 扩展
    while(!q.empty()){
        int x = q.front(); q.pop();
        if(reachdk(x,k)) return true;
    }
    return false;
}

static void reverse_edge(int vi, int vj, int ee){
    // vi: from in -> out ; vj: from out -> in
    // vi
    auto it = find(vertex[vi].hedge.begin(), vertex[vi].hedge.end(), ee);
    if(it != vertex[vi].hedge.end()) vertex[vi].hedge.erase(it);
    vertex[vi].tedge.push_back(ee);
    vertex[vi].indegree--;

    auto it2 = find(hyperedge[ee].vharr.begin(), hyperedge[ee].vharr.end(), vi);
    if(it2 != hyperedge[ee].vharr.end()) hyperedge[ee].vharr.erase(it2);
    hyperedge[ee].vtarr.push_back(vi);

    // vj
    auto it3 = find(hyperedge[ee].vtarr.begin(), hyperedge[ee].vtarr.end(), vj);
    if(it3 != hyperedge[ee].vtarr.end()) hyperedge[ee].vtarr.erase(it3);
    hyperedge[ee].vharr.push_back(vj);

    auto it4 = find(vertex[vj].tedge.begin(), vertex[vj].tedge.end(), ee);
    if(it4 != vertex[vj].tedge.end()) vertex[vj].tedge.erase(it4);
    vertex[vj].hedge.push_back(ee);
    vertex[vj].indegree++;
}

static void reachout(int vi,int k, vector<char>& dk){
    queue<int>q;
    for(size_t i=0;i<vertex[vi].hedge.size();++i){
        int ee = vertex[vi].hedge[i];
        for(int vv: hyperedge[ee].vtarr){
            if(!dk[vv] && (vertex[vi].indegree - vertex[vv].indegree) >= 2){
                reverse_edge(vi, vv, ee);
                if(vertex[vv].indegree >= k) dk[vv] = true;
                q.push(vv);
                --i; break;
            }
        }
    }
    while(!q.empty()){
        int x=q.front(); q.pop();
        reachout(x,k,dk);
    }
}

static void reachin(int vi,int k, vector<char>& dk){
    queue<int>q;
    for(size_t i=0;i<vertex[vi].tedge.size();++i){
        int ee = vertex[vi].tedge[i];
        for(int vv: hyperedge[ee].vharr){
            if(!dk[vv] && (vertex[vv].indegree - vertex[vi].indegree) >= 2){
                reverse_edge(vv, vi, ee);
                q.push(vv);
                --i; break;
            }
        }
    }
    while(!q.empty()){
        int x=q.front(); q.pop();
        reachin(x,k,dk);
    }
}

static void outk(int vi,int k, vector<char>& dk){
    int initd = vertex[vi].indegree;

    for(size_t i=0;i<vertex[vi].hedge.size();++i){
        int ee = vertex[vi].hedge[i];
        for(int vv: hyperedge[ee].vtarr){
            if(dk[vv] && (vertex[vi].indegree - vertex[vv].indegree) >= 2){
                reverse_edge(vi, vv, ee);
                --i; break;
            }
        }
    }
    for(size_t i=0;i<vertex[vi].tedge.size();++i){
        int ee = vertex[vi].tedge[i];
        for(int vv: hyperedge[ee].vharr){
            if(dk[vv] && (vertex[vv].indegree - vertex[vi].indegree) >= 2){
                reverse_edge(vv, vi, ee);
                --i; break;
            }
        }
    }

    if(initd > vertex[vi].indegree){
        reachin(vi,k,dk);
        if(vertex[vi].indegree < k) dk[vi] = false;
    }else if(initd < vertex[vi].indegree){
        reachout(vi,k,dk);
        if(vertex[vi].indegree < k) dk[vi] = false;
    }else{
        dk[vi] = false;
    }
}

static void peelk(int k, vector<char>& dk){
    while(true){
        bool flag = false;
        for(int i=1;i<=nodenum;i++){
            if(dk[i] && vertex[i].indegree < k){
                flag = true;
                outk(i,k,dk);
            }
        }
        if(!flag) break;
    }
}

static bool reachdk_wrapper(int i,int k){
    fh.clear();
    return reachdk(i,k);
}

static void finddk(int k, vector<char>& dk, vector<int>& layerOut){
    layerOut.clear();
    for(int i=1;i<=nodenum;i++){
        if(dk[i]){ layerOut.push_back(i); continue; }
        if(vertex[i].indegree == k-1 && reachdk_wrapper(i,k)) layerOut.push_back(i);
    }
}

// return kmax (下一层为空时的 k)
static int getlayer(){
    // dk 初始化为 true
    dk.assign(nodenum + 1, 1);
    int k;
    for(k=2; ; ++k){
        if(dkv[k-1].empty()) break;
        peelk(k, dk);
        finddk(k, dk, dkv[k]);
        if(dkv[k].empty()) break;
    }
    return k;
}

// ====== 收集“某个 δ”的所有非空层，附加到 global_layers ======
static void collect_layers_for_delta(int delta, vector<vector<int>>& global_layers){
    orientation(delta);

    // dkv[k] 需要事先分配到一个合理上限；这里按 nodenum+2
    dkv.assign(nodenum + 3, {});
    dkv[1].reserve(nodenum);
    for(int v=1; v<=nodenum; ++v) dkv[1].push_back(v);

    int kmax = getlayer();
    for(int k=1; k<kmax; ++k){
        if(!dkv[k].empty()) global_layers.push_back(dkv[k]); // 复制
    }
}

// ====== 计算并打印“全局指标”（一次性） ======
struct Summary {
    int L=0, maxV=0; long long maxE=0, sumV=0, sumE=0;
    double rhoB_bar=0.0, output_ratio=0.0, rho_inner=0.0, rho_max=0.0, Bdens_inner=0.0;
    ExtMetrics ext;
};

static Summary compute_and_print_global_metrics(const vector<vector<int>>& layers,
                                                int SIGMA_V, int SIGMA_U, int TOPK,
                                                double TAU, int LSTAR, double EPCT){
    auto stats = precompute_stats(layers, vnum_init, unum_init, V2U_init, U2V_init);

    Summary S;
    long long W=0; double WR=0.0;
    int last=-1;
    for(int i=(int)layers.size()-1;i>=0;--i) if(!layers[i].empty()){ last=i; break; }

    for(size_t i=0;i<layers.size();++i){
        if(layers[i].empty()) continue;
        S.L++;
        const auto &st = stats[i];
        S.sumV += st.Vk; S.sumE += st.Ek;
        W      += st.Ek; WR     += st.Ek * st.rhoB;
        S.rho_max = max(S.rho_max, st.rhoB);
    }
    double avgV = (S.L? (double)S.sumV/S.L : 0.0);
    double avgE = (S.L? (double)S.sumE/S.L : 0.0);
    S.rhoB_bar  = (W? WR/W : 0.0);

    if(last>=0 && !layers[last].empty()){
        S.maxV = stats[last].Vk; S.maxE = stats[last].Ek;
        S.rho_inner = stats[last].rhoB;
        S.Bdens_inner = butterfly_density_for_layer(layers[last], vnum_init, unum_init, V2U_init, U2V_init);
    }

    // output_ratio（原始口径）
    vector<char> inV(vnum_init+1, 0);
    for(const auto& Vk : layers) for(int v: Vk) if(0<=v && v<=vnum_init) inV[v]=1;
    long long Etot=0, Ecov=0;
    for(int v=0; v<=vnum_init; ++v){
        long long d = (long long)V2U_init[v].size();
        Etot += d; if(inV[v]) Ecov += d;
    }
    S.output_ratio = Etot? (double)Ecov/Etot : 0.0;

    // 扩展指标
    S.ext = compute_extended_metrics(layers, stats, vnum_init, unum_init, V2U_init, U2V_init,
                                     SIGMA_V, SIGMA_U, TOPK, TAU, LSTAR, EPCT);

    // —— 主表 —— //
    cout<<fixed<<setprecision(3);
    cout<<"L="<<S.L
        <<"  maxV="<<S.maxV
        <<"  maxE="<<S.maxE
        <<"  avgV="<<avgV
        <<"  avgE="<<avgE
        <<"  rhoB_bar="<<S.rhoB_bar
        <<"  output_ratio="<<S.output_ratio
        <<"  rho_inner="<<S.rho_inner
        <<"  rho_max="<<S.rho_max
        <<"  Bdens_inner="<<S.Bdens_inner
        <<"\n";

    // —— 扩展表 —— //
    cout<<"rhoB_max_sigma="<<S.ext.rhoB_max_sigma
        <<"  rhoB_inner_sigma="<<S.ext.rhoB_inner_sigma
        <<"  L_eff="<<S.ext.L_eff
        <<"  L_at_tau="<<S.ext.L_at_tau
        <<"  rhoB_at_topK="<<S.ext.rhoB_at_topK
        <<"  rhoB_at_topK_w="<<S.ext.rhoB_at_topK_w
        <<"  Bdens_inner_sigma="<<S.ext.Bdens_inner_sigma
        <<"  Bdens_at_topK="<<S.ext.Bdens_at_topK
        <<"  E_at_Lstar="<<S.ext.E_at_Lstar
        <<"  L_at_Epct="<<S.ext.L_at_Epct
        <<"\n";

    return S;
}

int main(int argc, char** argv){
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    string path   = (argc>=2? argv[1] : string("dataset/mll2.txt"));
    int SIGMA_V   = (argc>=3? atoi(argv[2]) : 50);
    int SIGMA_U   = (argc>=4? atoi(argv[3]) : 50);
    int TOPK      = (argc>=5? atoi(argv[4]) : 5);
    double TAU    = (argc>=6? atof(argv[5]) : 0.80);
    int LSTAR     = (argc>=7? atoi(argv[6]) : 5);
    double EPCT   = (argc>=8? atof(argv[7]) : 0.90);

    readedge(path);

    // 收集“所有 δ”的全局层序
    vector<vector<int>> global_layers;
    global_layers.reserve(2048);
    for(int delta=1; delta<=deltamax; ++delta){
        collect_layers_for_delta(delta, global_layers);
    }

    // 一次性计算全局主表 + 扩展表
    compute_and_print_global_metrics(global_layers,
                                     SIGMA_V, SIGMA_U, TOPK, TAU, LSTAR, EPCT);
    return 0;
}
