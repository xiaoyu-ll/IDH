#pragma once
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
using namespace std;

// ===== 数据结构：每层统计 & 扩展指标 =====
struct LayerStats { double rhoB=0; int Vk=0; int Uk=0; long long Ek=0; };
struct ExtMetrics {
    // 1) 规模阈值密度
    double rhoB_max_sigma=0.0, rhoB_inner_sigma=0.0;
    // 2) 有效层
    int L_eff=0, L_at_tau=0;
    // 3) 密度–规模帕累托
    double rhoB_at_topK=0.0, rhoB_at_topK_w=0.0;
    // 4) 蝴蝶密度阈值
    double Bdens_inner_sigma=0.0, Bdens_at_topK=0.0;
    // 5) 覆盖效率
    double E_at_Lstar=0.0;  // 最前 L* 层覆盖边比例
    int    L_at_Epct=0;     // 覆盖 Epct 需要的层数
};

// ===== 单层二分图密度（原始图口径） =====
// V2U[v] = 相邻 U 列表；U2V[u] = 相邻 V 列表
inline LayerStats bip_density_for_layer(const vector<int>& Vk_nodes,
                                        int vnum, int unum,
                                        const vector<vector<int>>& V2U,
                                        const vector<vector<int>>& U2V) {
    vector<char> inV(vnum+1, 0), markU(unum+1, 0);
    for (int v: Vk_nodes) if (0<=v && v<=vnum) inV[v]=1;
    long long Ek=0; int Uk=0;
    for (int v: Vk_nodes) {
        Ek += (long long)V2U[v].size();
        for (int u: V2U[v]) if (!markU[u]) { markU[u]=1; Uk++; }
    }
    int Vk=(int)Vk_nodes.size();
    double rho = (Uk>0 && Vk>0) ? (double)Ek / ((double)Uk * (double)Vk) : 0.0;
    return {rho, Vk, Uk, Ek};
}

// ===== 蝴蝶密度（每边归一化；原始图上算） =====
inline long long C2(long long x){ return x>=2? x*(x-1)/2 : 0; }

inline double butterfly_density_for_layer(const vector<int>& Vk_nodes,
                                          int vnum, int unum,
                                          const vector<vector<int>>& V2U,
                                          const vector<vector<int>>& U2V) {
    if (Vk_nodes.empty()) return 0.0;
    vector<char> inV(vnum+1, 0);
    for (int v: Vk_nodes) if (0<=v && v<=vnum) inV[v]=1;

    unordered_map<unsigned long long,int> pairCnt;
    pairCnt.reserve(1<<20);
    auto key=[](int a,int b){ if(a>b) swap(a,b); return ((unsigned long long)a<<32)|b; };

    long long Ek=0;
    for (int v: Vk_nodes) Ek += (long long)V2U[v].size();

    for (int u=0; u<=unum; ++u) {
        const auto &Vlist = U2V[u];
        if (Vlist.empty()) continue;
        vector<int> L; L.reserve(Vlist.size());
        for (int v: Vlist) if (inV[v]) L.push_back(v);
        int s=(int)L.size(); if (s<2) continue;
        for (int i=0;i<s;i++) for (int j=i+1;j<s;j++) pairCnt[key(L[i],L[j])]++;
    }
    long long B=0; for (auto &kv: pairCnt){ long long c=kv.second; if (c>=2) B+=C2(c); }
    return Ek? (double)B/(double)Ek : 0.0;
}

// ===== 预计算每层密度统计，避免重复算 =====
inline vector<LayerStats> precompute_stats(const vector<vector<int>>& layers,
                                           int vnum, int unum,
                                           const vector<vector<int>>& V2U,
                                           const vector<vector<int>>& U2V) {
    vector<LayerStats> stats(layers.size());
    for (size_t k=0;k<layers.size();++k)
        if (!layers[k].empty())
            stats[k] = bip_density_for_layer(layers[k], vnum, unum, V2U, U2V);
    return stats;
}

// ===== 计算“最前 t 层”的边覆盖率 =====
inline double edge_coverage_prefix(const vector<vector<int>>& layers,
                                   size_t t, int vnum,
                                   const vector<vector<int>>& V2U) {
    vector<char> inV(vnum+1,0);
    for (size_t i=0;i<t && i<layers.size();++i)
        for (int v: layers[i]) if (0<=v && v<=vnum) inV[v]=1;
    long long Etot=0, Ecov=0;
    for (int v=0; v<=vnum; ++v) {
        long long deg=(long long)V2U[v].size();
        Etot += deg;
        if (inV[v]) Ecov += deg;
    }
    return Etot? (double)Ecov/(double)Etot : 0.0;
}

// ===== 核心：计算 5 组“扩展指标” =====
inline ExtMetrics compute_extended_metrics(const vector<vector<int>>& layers,
                                           const vector<LayerStats>& stats,
                                           int vnum, int unum,
                                           const vector<vector<int>>& V2U,
                                           const vector<vector<int>>& U2V,
                                           // 参数
                                           int sigmaV, int sigmaU,   // 规模阈值
                                           int topK,                 // 帕累托 K
                                           double tau,               // L@τ
                                           int Lstar,                // E@L*
                                           double Epct_target)       // L@E%
{
    ExtMetrics M;

    // 找到原始“最内层”索引
    int last_idx=-1;
    for (int i=(int)layers.size()-1; i>=0; --i) if (!layers[i].empty()) { last_idx=i; break; }

    // 1) rhoB^{max,σ} & rhoB^{inner,σ}
    double rho_max_sigma=0.0;
    for (size_t k=0;k<layers.size();++k) {
        if (layers[k].empty()) continue;
        const auto &st = stats[k];
        if (st.Vk>=sigmaV && st.Uk>=sigmaU) rho_max_sigma = max(rho_max_sigma, st.rhoB);
    }
    double rho_inner_sigma=0.0;
    if (last_idx>=0) {
        for (int k=last_idx; k>=0; --k) {
            if (layers[k].empty()) continue;
            const auto &st=stats[k];
            if (st.Vk>=sigmaV && st.Uk>=sigmaU) { rho_inner_sigma = st.rhoB; break; }
        }
    }
    M.rhoB_max_sigma   = rho_max_sigma;
    M.rhoB_inner_sigma = rho_inner_sigma;

    // 2) L_eff & L@τ
    int L_eff=0;
    for (size_t k=0;k<layers.size();++k)
        if (!layers[k].empty() && stats[k].Vk>=sigmaV && stats[k].Uk>=sigmaU) L_eff++;
    M.L_eff = L_eff;

    int L_at_tau=(int)layers.size();
    for (size_t t=1; t<=layers.size(); ++t) {
        if (edge_coverage_prefix(layers, t, vnum, V2U) >= tau) { L_at_tau=(int)t; break; }
    }
    M.L_at_tau = L_at_tau;

    // 3) ρ_B@TopK & 加权
    vector<pair<long long,int>> rank; rank.reserve(layers.size());
    for (size_t k=0;k<layers.size();++k) {
        if (layers[k].empty()) continue;
        const auto &st=stats[k];
        rank.push_back({ (long long)st.Vk*(long long)st.Uk, (int)k });
    }
    sort(rank.begin(), rank.end(), [](auto&a, auto&b){ return a.first>b.first; });
    int take = min(topK, (int)rank.size());
    double sumR=0.0, wr=0.0; long long sumW=0;
    for (int i=0;i<take;i++) {
        int k = rank[i].second;
        sumR += stats[k].rhoB;
        wr   += stats[k].rhoB * stats[k].Ek;
        sumW += stats[k].Ek;
    }
    M.rhoB_at_topK   = (take? sumR/take : 0.0);
    M.rhoB_at_topK_w = (sumW? wr/sumW   : 0.0);

    // 4) Bdens^{inner,σ} & Bdens@TopK
    double B_inner_sigma=0.0;
    if (last_idx>=0) {
        for (int k=last_idx; k>=0; --k) {
            if (layers[k].empty()) continue;
            const auto &st=stats[k];
            if (st.Vk>=sigmaV && st.Uk>=sigmaU) {
                B_inner_sigma = butterfly_density_for_layer(layers[k], vnum, unum, V2U, U2V);
                break;
            }
        }
    }
    M.Bdens_inner_sigma = B_inner_sigma;

    double B_topK=0.0;
    if (take>0) {
        for (int i=0;i<take;i++) {
            int k=rank[i].second;
            B_topK += butterfly_density_for_layer(layers[k], vnum, unum, V2U, U2V);
        }
        B_topK /= take;
    }
    M.Bdens_at_topK = B_topK;

    // 5) 覆盖效率：E@L* 与 L@E%
    M.E_at_Lstar = edge_coverage_prefix(layers, (size_t)Lstar, vnum, V2U);

    int L_at_Epct=(int)layers.size();
    for (size_t t=1; t<=layers.size(); ++t) {
        if (edge_coverage_prefix(layers, t, vnum, V2U) >= Epct_target) { L_at_Epct=(int)t; break; }
    }
    M.L_at_Epct = L_at_Epct;

    return M;
}