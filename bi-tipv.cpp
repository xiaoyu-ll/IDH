#include<iostream>
#include<fstream>
#include<sstream>
#include<string>
#include<algorithm>
#include<cmath>
#include<map>
#include<vector>
#include<cstdlib>
#include<chrono>
#include<iomanip>
#include<stack>
#include<queue>
#include<cstring>
#include<unordered_map>
#include "ext_metrics.hpp"


using namespace std;



// ====== 上限 ======
const int VM = 1730000; // V 侧
const int UM = 1730000; // U 侧

struct Vertex {
    int id = 0;
    long long tip = -1;         // theta(v)
    vector<int> vec;            // 邻接：V->U 或 U->V
    bool flag = false;          // 是否出现
};

Vertex vertexv[VM]; // V 侧（本次剥皮对象）
Vertex vertexu[UM]; // U 侧
int vnum = 0;       // V 最大 id
int unum = 0;       // U 最大 id

// 读入：每行 "v,u"
void readedge() {
    ifstream rda("dataset/mll2.txt");
    if (!rda) { cerr << "error: cannot open input\n"; exit(1); }
    string line;
    while (rda >> line) {
        int pos = (int)line.find(',');
        if (pos == (int)string::npos) continue;
        int v = stoi(line.substr(0, pos));
        int u = stoi(line.substr(pos + 1));
        vnum = max(vnum, v);
        unum = max(unum, u);
        if (v>=VM || u>=UM){ cerr<<"[ERR] ID exceed VM/UM, please enlarge.\n"; exit(1); }
        vertexv[v].flag = true;
        vertexu[u].flag = true;
        vertexv[v].vec.push_back(u); // V -> U
        vertexu[u].vec.push_back(v); // U -> V（对称）
    }
    cerr << "read edge successful! |U|~" << (unum + 1)
         << " |V|~" << (vnum + 1) << "\n";
    rda.close();
}

// 组合 C(x,2)
static inline long long comb2(long long x) { return (x>=2)? (x*(x-1))/2 : 0; }

// ====== k-tip(V)：计算 theta(v) ======
vector<long long> ktip_on_V() {
    int nV = vnum + 1;
    vector<char> active(nV, 0);
    for (int v = 0; v <= vnum; ++v) if (vertexv[v].flag) active[v] = 1;

    vector<int> mark(nV, 0), touched;
    vector<int> cnt(nV, 0);
    int vis = 1;

    auto D2 = [&](int v)->const vector<int>& {
        touched.clear();
        ++vis;
        for (int u : vertexv[v].vec) {
            const auto &Vlist = vertexu[u].vec; // u 的所有 V 邻居
            for (int d : Vlist) {
                if (d == v || !active[d]) continue;
                if (mark[d] != vis) { mark[d] = vis; cnt[d] = 1; touched.push_back(d); }
                else { cnt[d] += 1; }
            }
        }
        return touched;
    };

    vector<long long> beta(nV, 0), theta(nV, -1);
    for (int v = 0; v <= vnum; ++v) {
        if (!active[v]) continue;
        const auto &L = D2(v);
        long long s = 0; for (int d : L) s += comb2(cnt[d]);
        beta[v] = s;
    }

    using P = pair<long long,int>;
    priority_queue<P, vector<P>, greater<P>> pq;
    int actCnt = 0;
    for (int v = 0; v <= vnum; ++v) if (active[v]) { pq.emplace(beta[v], v); actCnt++; }

    while (actCnt > 0) {
        int v; long long b;
        while (true) {
            if (pq.empty()) {
                for (int i = 0; i <= vnum; ++i)
                    if (active[i]) { theta[i] = beta[i]; active[i] = 0; }
                actCnt = 0; break;
            }
            b = pq.top().first; v = pq.top().second; pq.pop();
            if (!active[v]) continue;
            if (b != beta[v]) { pq.emplace(beta[v], v); continue; }
            break;
        }
        if (actCnt == 0) break;

        theta[v] = beta[v];
        active[v] = 0;
        actCnt--;

        const auto &L = D2(v);
        for (int d : L) {
            if (!active[d]) continue;
            long long dec = comb2(cnt[d]);
            if (dec == 0) continue;
            beta[d] -= dec;
            if (beta[d] < 0) beta[d] = 0;
            pq.emplace(beta[d], d);
        }
    }

    for (int v = 0; v <= vnum; ++v) vertexv[v].tip = theta[v];
    return theta;
}

// ====== 二分图密度（按 U' = N(V_k) 归并），以及 |E_k| ======

// ====== 最内层蝴蝶密度（每边归一化，原始图上算） ======


// ====== 1) 仅构建所有层（不打印），返回 Vk_layers ======
static vector<vector<int>> build_layers_by_ktip_V(){
    auto theta = ktip_on_V();

    long long tip_max = 0;
    for (int v = 0; v <= vnum; ++v) if (vertexv[v].flag) tip_max = max(tip_max, theta[v]);

    vector<vector<int>> Vk_layers;
    Vk_layers.assign((size_t)tip_max+1, {});
    for(int v=0; v<=vnum; ++v){
        if(!vertexv[v].flag) continue;
        long long tv = theta[v];
        for(long long k=1; k<=tv; ++k) Vk_layers[(size_t)k].push_back(v);
    }
    return Vk_layers;
}


static void summarize_and_print(const vector<vector<int>>& Vk_layers, double Tsec_total){
    // ----- 原主表计算（保持不变） -----
    int L = 0; for(size_t k=1;k<Vk_layers.size();++k) if(!Vk_layers[k].empty()) L++;

    long long sumV=0, sumE=0, W=0; double WR=0.0, rho_max=0.0;
    int k_star=0; for(int k=(int)Vk_layers.size()-1;k>=1;--k) if(!Vk_layers[k].empty()){ k_star=k; break; }

    int maxV=0; long long maxE=0; double rho_inner=0.0, Bdens_inner=0.0;

    for(size_t k=1;k<Vk_layers.size();++k){
        if(Vk_layers[k].empty()) continue;

    }
    extern Vertex vertexv[]; extern Vertex vertexu[];
    extern int vnum, unum;

    vector<vector<int>> V2U(vnum+1), U2V(unum+1);
    for (int v=0; v<=vnum; ++v) V2U[v]=vertexv[v].vec;
    for (int u=0; u<=unum; ++u) U2V[u]=vertexu[u].vec;

    auto simple_bip = [&](const vector<int>& Vk)->LayerStats{
        vector<char> inV(vnum+1,0), markU(unum+1,0);
        for(int v: Vk) if(0<=v && v<=vnum) inV[v]=1;
        long long Ek=0; int Uk=0;
        for(int v: Vk){
            Ek += (long long)V2U[v].size();
            for(int u: V2U[v]) if(!markU[u]){ markU[u]=1; Uk++; }
        }
        int Vk_sz=(int)Vk.size();
        double rho = (Uk>0 && Vk_sz>0)? (double)Ek/((double)Uk*(double)Vk_sz) : 0.0;
        return {rho, Vk_sz, Uk, Ek};
    };

    for(size_t k=1; k<Vk_layers.size(); ++k){
        if(Vk_layers[k].empty()) continue;
        auto st = simple_bip(Vk_layers[k]);
        sumV += st.Vk; sumE += st.Ek;
        W    += st.Ek; WR   += st.Ek * st.rhoB;
        rho_max = max(rho_max, st.rhoB);
        if((int)k==k_star){
            maxV = st.Vk; maxE = st.Ek; rho_inner = st.rhoB;
            Bdens_inner = butterfly_density_for_layer(Vk_layers[k], vnum, unum, V2U, U2V);
        }
    }
    double avgV = (L? (double)sumV/L : 0.0);
    double avgE = (L? (double)sumE/L : 0.0);
    double rhoB_bar = (W? WR/(double)W : 0.0);

    vector<char> inV(vnum+1,0);
    for(size_t k=1;k<Vk_layers.size();++k)
        for(int v:Vk_layers[k]) if(0<=v && v<=vnum) inV[v]=1;
    long long E_total=0, E_cov=0;
    for(int v=0; v<=vnum; ++v){
        long long deg=(long long)V2U[v].size();
        E_total += deg; if(inV[v]) E_cov += deg;
    }
    double output_ratio = (E_total? (double)E_cov/(double)E_total : 0.0);



    cout<<fixed<<setprecision(3);
    cout<<"L="<<L
        <<"  maxV="<<maxV
        <<"  maxE="<<maxE
        <<"  avgV="<<avgV
        <<"  avgE="<<avgE
        <<"  rhoB_bar="<<rhoB_bar
        <<"  output_ratio="<<output_ratio
        <<"  rho_inner="<<rho_inner
        <<"  rho_max="<<rho_max
        <<"  Bdens_inner="<<Bdens_inner
        <<"  T(s)="<<Tsec_total
        <<"\n";



    vector<vector<int>> layers; layers.reserve(Vk_layers.size());
    for(size_t k=1;k<Vk_layers.size();++k) layers.push_back(Vk_layers[k]);

    auto stats = precompute_stats(layers, vnum, unum, V2U, U2V);

    // 这些参数可按数据集调：阈值、TopK、τ、L*、覆盖百分比
    int    sigmaV = 50, sigmaU = 50;
    int    topK   = 10;
    double tau    = 0.80;
    int    Lstar  = 10;
    double Epct   = 0.90;

    auto ext = compute_extended_metrics(layers, stats, vnum, unum, V2U, U2V,
                                        sigmaV, sigmaU, topK, tau, Lstar, Epct);

    cout<<fixed<<setprecision(3)
        <<"rhoB_max_sigma="<<ext.rhoB_max_sigma
        <<"  rhoB_inner_sigma="<<ext.rhoB_inner_sigma
        <<"  L_eff="<<ext.L_eff
        <<"  L_at_tau="<<ext.L_at_tau
        <<"  rhoB@TopK="<<ext.rhoB_at_topK
        <<"  rhoB@TopK_w="<<ext.rhoB_at_topK_w
        <<"  Bdens_inner_sigma="<<ext.Bdens_inner_sigma
        <<"  Bdens@TopK="<<ext.Bdens_at_topK
        <<"  E@Lstar="<<ext.E_at_Lstar
        <<"  L@Epct="<<ext.L_at_Epct
        <<"\n";
}
int main(){
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    auto t0 = chrono::high_resolution_clock::now();

    readedge();
    auto Vk_layers = build_layers_by_ktip_V();

    auto t1 = chrono::high_resolution_clock::now();
    double sec = chrono::duration<double>(t1 - t0).count();

    summarize_and_print(Vk_layers, sec);
    return 0;
}