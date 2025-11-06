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
#include<unordered_set>



using namespace std;


#include "ext_metrics.hpp"



static const int VM = 250000; // V 侧最大 id
static const int UM = 350000; // U 侧最大 id

// ---------- 原始图邻接（用于统计口径，一律用原始图） ----------
static int vnum=0, unum=0;
static vector<vector<int>> V2U_init(VM+1), U2V_init(UM+1);

// ---------- 读入（去重边） ----------
static void readedge(const string& path){
    ifstream fin(path);
    if(!fin){ cerr<<"[Error] cannot open "<<path<<"\n"; exit(1); }
    unordered_set<unsigned long long> S; S.reserve(1<<23);
    string s; long long E=0;
    while(fin>>s){
        int p=(int)s.find(',');
        if(p==string::npos) continue;
        int v=stoi(s.substr(0,p));
        int u=stoi(s.substr(p+1));
        if(v>VM || u>UM){ cerr<<"[Error] id exceeds VM/UM, enlarge constants.\n"; exit(1); }
        unsigned long long key=((unsigned long long)v<<32)|(unsigned long long)u;
        if(S.insert(key).second){
            V2U_init[v].push_back(u);
            U2V_init[u].push_back(v);
            vnum=max(vnum,v); unum=max(unum,u); E++;
        }
    }
    fin.close();
    cerr<<"[Load] |V|="<<vnum<<" |U|="<<unum<<" |E|="<<E<<"\n";
}


static vector<vector<int>> layers_for_a(int a){
    // 工作副本（不动原始图）
    vector<vector<int>> V2U = V2U_init;
    vector<vector<int>> U2V = U2V_init;
    vector<int> degV(vnum+1,0), degU(unum+1,0);
    vector<char> aliveV(vnum+1,0), aliveU(unum+1,0);

    // 允许 0 号顶点：索引从 0..vnum / 0..unum
    for(int v=0; v<=vnum; ++v){ degV[v] = (int)V2U[v].size(); if(degV[v]>0) aliveV[v]=1; }
    for(int u=0; u<=unum; ++u){ degU[u] = (int)U2V[u].size(); if(degU[u]>0) aliveU[u]=1; }

    auto removeV = [&](int v, int b, deque<int>& qU){
        if(!aliveV[v]) return;
        aliveV[v]=0;
        for(int u: V2U[v]){
            if(!aliveU[u]) continue;
            // 从 u 的邻接删掉 v（线性擦除）
            auto &lst = U2V[u];
            auto it = find(lst.begin(), lst.end(), v);
            if(it!=lst.end()) lst.erase(it);
            int old = degU[u];
            degU[u]--;
            // **只在度跨过阈值时入队**（避免全扫描）
            if(old >= b && degU[u] < b) qU.push_back(u);
        }
        V2U[v].clear();
        degV[v]=0;
    };
    auto removeU = [&](int u, int a, deque<int>& qV){
        if(!aliveU[u]) return;
        aliveU[u]=0;
        for(int v: U2V[u]){
            if(!aliveV[v]) continue;
            auto &lst = V2U[v];
            auto it = find(lst.begin(), lst.end(), u);
            if(it!=lst.end()) lst.erase(it);
            int old = degV[v];
            degV[v]--;
            if(old >= a && degV[v] < a) qV.push_back(v);
        }
        U2V[u].clear();
        degU[u]=0;
    };

    int beta_max=0; for(int u=0; u<=unum; ++u) beta_max = max(beta_max, degU[u]);
    vector<vector<int>> layers(beta_max+2); // 1..beta_max

    for(int b=1; b<=beta_max; ++b){
        deque<int> qV, qU;

        // 初始化：把当前低于阈值的点放入队列（只做一次）
        for(int v=0; v<=vnum; ++v) if(aliveV[v] && degV[v] < a) qV.push_back(v);
        for(int u=0; u<=unum; ++u) if(aliveU[u] && degU[u] < b) qU.push_back(u);

        // 级联删除：仅用**邻居触发**，不做全体扫描
        while(!qV.empty() || !qU.empty()){
            while(!qV.empty()){
                int v=qV.front(); qV.pop_front();
                if(!aliveV[v] || degV[v] >= a) continue;
                removeV(v, b, qU);
            }
            while(!qU.empty()){
                int u=qU.front(); qU.pop_front();
                if(!aliveU[u] || degU[u] >= b) continue;
                removeU(u, a, qV);
            }
        }

        // 收集这一层的 V
        vector<int> Vk; Vk.reserve(1024);
        for(int v=0; v<=vnum; ++v) if(aliveV[v]) Vk.push_back(v);
        if(Vk.empty()) break;
        layers[b] = move(Vk);
    }
    return layers; // layers[0] 空，从 1 开始有效
}

// ---------- 汇总并打印（主表 + 扩展表），一律用原始图口径 ----------
struct Summary {
    int L=0, maxV=0; long long maxE=0, sumV=0, sumE=0;
    double rhoB_bar=0.0, output_ratio=0.0, rho_inner=0.0, rho_max=0.0, Bdens_inner=0.0;
    ExtMetrics ext;
};

static Summary summarize_and_print(const vector<vector<int>>& layers,
                                   int SIGMA_V, int SIGMA_U, int TOPK,
                                   double TAU, int LSTAR, double EPCT)
{
    // 去掉 layers[0]，准备统计
    vector<vector<int>> Ls; Ls.reserve(layers.size());
    for(size_t i=1;i<layers.size();++i) Ls.push_back(layers[i]);
    auto stats = precompute_stats(Ls, vnum, unum, V2U_init, U2V_init);

    Summary S;
    long long W=0; double WR=0.0;
    int last=-1;
    for(int i=(int)Ls.size()-1;i>=0;--i) if(!Ls[i].empty()){ last=i; break; }

    for(size_t i=0;i<Ls.size();++i){
        if(Ls[i].empty()) continue;
        S.L++;
        const auto &st = stats[i];
        S.sumV += st.Vk; S.sumE += st.Ek;
        W      += st.Ek; WR     += st.Ek * st.rhoB;
        S.rho_max = max(S.rho_max, st.rhoB);
        if((int)i==last){
            S.maxV = st.Vk; S.maxE = st.Ek; S.rho_inner = st.rhoB;
            S.Bdens_inner = butterfly_density_for_layer(Ls[i], vnum, unum, V2U_init, U2V_init);
        }
    }
    double avgV = (S.L? (double)S.sumV/S.L : 0.0);
    double avgE = (S.L? (double)S.sumE/S.L : 0.0);
    S.rhoB_bar = (W? WR/W : 0.0);

    // output_ratio：所有层 V 并集覆盖边的占比（原始口径）
    vector<char> inV(vnum+1,0);
    for(const auto& Vk : Ls) for(int v: Vk) if(v>=1 && v<=vnum) inV[v]=1;
    long long Etot=0, Ecov=0;
    for(int v=1; v<=vnum; ++v){
        long long d=(long long)V2U_init[v].size();
        Etot+=d; if(inV[v]) Ecov+=d;
    }
    S.output_ratio = Etot? (double)Ecov/Etot : 0.0;


    S.ext = compute_extended_metrics(Ls, stats, vnum, unum, V2U_init, U2V_init,
                                     SIGMA_V, SIGMA_U, TOPK, TAU, LSTAR, EPCT);

    // —— 打印 主表 —— //
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

    // —— 打印 扩展表 —— //
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

    auto T0 = chrono::high_resolution_clock::now();
    readedge(path);

    // 设一个可行的 a 上限（避免无谓爆炸）
    int maxDegV=0; for(int v=1; v<=vnum; ++v) maxDegV=max(maxDegV, (int)V2U_init[v].size());
    int A_MAX = min(maxDegV, 200);

    // 穷举 a，挑选“层数最多”的那条 a-path 作为 Bi-core 的输出口径
    int bestL = -1;
    vector<vector<int>> best_layers;
    for(int a=1; a<=A_MAX; ++a){
        auto layers = layers_for_a(a);
        int L=0; for(size_t i=1;i<layers.size();++i) if(!layers[i].empty()) L++;
        if(L>bestL){ bestL=L; best_layers.swap(layers); }
        // 若希望“以 L_eff 最大”为准，可改成：
        // auto tmp = precompute_stats(vector<vector<int>>(layers.begin()+1,layers.end()),
        //                             vnum, unum, V2U_init, U2V_init);
        // int L_eff=0; for(size_t i=1;i<layers.size();++i)
        //     if(!layers[i].empty() && tmp[i-1].Vk>=SIGMA_V && tmp[i-1].Uk>=SIGMA_U) L_eff++;
        // if(L_eff > bestL_eff){ ... }
    }

    auto sum = summarize_and_print(best_layers, SIGMA_V, SIGMA_U, TOPK, TAU, LSTAR, EPCT);

    auto T1 = chrono::high_resolution_clock::now();
    double sec = chrono::duration<double>(T1-T0).count();
    double mem = get_peak_rss_mb();
    cout<<fixed<<setprecision(3)<<"Mem(MB)="<<mem<<"  T(s)="<<sec<<"\n";
    return 0;
}