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
#include <numeric>
#include <unordered_set>
#include <limits>
#include <random>

using namespace std;
struct TopNRes {
    long long N,tp,fp,fn;
    double precision,recall,f1;
    double roc, ap;  // 新增
};

/********** utils **********/
static inline string trim(const string& s){
    size_t i=0,j=s.size(); while(i<j && isspace((unsigned char)s[i])) ++i;
    while(j>i && isspace((unsigned char)s[j-1])) --j; return s.substr(i,j-i);
}
static inline vector<string> split_simple(const string& s, char d=','){
    vector<string> v; v.reserve(32); string cur; cur.reserve(64);
    for(char c: s){ if(c==d){ v.push_back(cur); cur.clear(); } else cur.push_back(c); }
    v.push_back(cur); return v;
}
static inline bool is_true_str(string v){
    for(char &c: v) c=(char)tolower((unsigned char)c); v=trim(v);
    return (v=="true"||v=="1"||v=="t"||v=="yes");
}

struct PRF { double p,r,f1; };
static PRF prf_from_counts(long long tp,long long fp,long long fn){
    double P=(tp+fp)?(double)tp/(tp+fp):0.0;
    double R=(tp+fn)?(double)tp/(tp+fn):0.0;
    double F=(P+R)?2*P*R/(P+R):0.0;
    return {P,R,F};
}
static double auc_roc(vector<pair<double,int>> a){
    stable_sort(a.begin(), a.end(), [](auto&x,auto&y){return x.first>y.first;});
    long long P=0,N=0; for(auto &p:a) (p.second?P:N)++;
    if(P==0||N==0) return numeric_limits<double>::quiet_NaN();
    long long tp=0,fp=0,tp_prev=0,fp_prev=0; double prev=numeric_limits<double>::infinity(), auc=0;
    for(size_t i=0;i<a.size();++i){
        double s=a[i].first; int y=a[i].second;
        if(s!=prev){
            double tpr_prev=(double)tp_prev/P, fpr_prev=(double)fp_prev/N;
            double tpr=(double)tp/P,        fpr=(double)fp/N;
            auc += (fpr - fpr_prev)*(tpr + tpr_prev)/2.0;
            prev=s; tp_prev=tp; fp_prev=fp;
        }
        if(y) ++tp; else ++fp;
    }
    double tpr_prev=(double)tp_prev/P, fpr_prev=(double)fp_prev/N;
    double tpr=(double)tp/P,          fpr=(double)fp/N;
    auc += (fpr - fpr_prev)*(tpr + tpr_prev)/2.0; return auc;
}
static double auc_pr_ap(vector<pair<double,int>> a){
    stable_sort(a.begin(), a.end(), [](auto&x,auto&y){return x.first>y.first;});
    long long P=0; for(auto&p:a) if(p.second) ++P;
    if(P==0) return numeric_limits<double>::quiet_NaN();
    long long tp=0,fp=0; double prev_rec=0, ap=0;
    for(size_t i=0;i<a.size();){
        double s=a[i].first; long long tp_b=0,fp_b=0; size_t j=i;
        while(j<a.size() && a[j].first==s){ if(a[j].second) ++tp_b; else ++fp_b; ++j; }
        tp+=tp_b; fp+=fp_b;
        double rec=(double)tp/P, prec=(double)tp/max(1LL,tp+fp);
        ap += (rec - prev_rec) * prec; prev_rec=rec; i=j;
    }
    return ap;
}
static pair<double,double> eval_auc_ap(vector<pair<double,int>> sl, long long N){
    if (N <= 0 || sl.empty()) return {numeric_limits<double>::quiet_NaN(),
                                      numeric_limits<double>::quiet_NaN()};
    if (N > (long long)sl.size()) N = sl.size();

    // 找到 topN 内的最小分数
    double thr = sl[N-1].first;
    double low = thr - 1e-9;

    vector<pair<double,int>> proj; proj.reserve(sl.size());
    for (long long i=0; i<(long long)sl.size(); ++i){
        double s = (i < N ? sl[i].first : low);
        proj.push_back({s, sl[i].second});
    }
    double roc = auc_roc(proj);
    double ap  = auc_pr_ap(proj);
    return {roc, ap};
}
static TopNRes eval_topN(vector<pair<double,int>> sl, long long N){
    stable_sort(sl.begin(), sl.end(), [](auto&a,auto&b){return a.first>b.first;});
    if(sl.empty()) return {N,0,0,0,0,0,0};
    if(N<=0) N=1; if(N>(long long)sl.size()) N=(long long)sl.size();
    long long tp=0,fp=0,fn=0;
    for(long long i=0;i<(long long)sl.size();++i){
        int pred=(i<N), y=sl[i].second;
        if(pred&&y) ++tp; else if(pred&&!y) ++fp; else if(!pred&&y) ++fn;
    }
    auto m=prf_from_counts(tp,fp,fn);
    return {N,tp,fp,fn,m.p,m.r,m.f1};
}

// 把原始模型分数映射为“分位分”（0~1，越大越可疑）
static unordered_map<long long,double>
to_percentile_score(const vector<long long>& ids,
                    const unordered_map<long long,double>& raw){
    vector<pair<double,long long>> a; a.reserve(ids.size());
    for(auto id: ids){
        double s = 0.0; auto it = raw.find(id); if(it!=raw.end()) s = it->second;
        a.push_back({s, id});
    }
    stable_sort(a.begin(), a.end(), [](auto&x,auto&y){
        if(x.first!=y.first) return x.first>y.first;
        return x.second<y.second;
    });
    unordered_map<long long,double> pct; pct.reserve(ids.size());
    const double n = (double)ids.size();
    for(size_t i=0;i<a.size();++i){
        pct[a[i].second] = (n>1? (n - i - 0.5)/n : 1.0);
    }
    return pct;
}

/********** main **********/
int main(int argc,char**argv){
    ios::sync_with_stdio(false); cin.tie(nullptr);
    if(argc<5){
        cerr<<"Usage: "<<argv[0]<<" acc_k.csv amlsim_accounts.csv amlsim_transactions.csv out_dir"
            <<" [NseedTop=50] [lambda=0.30] [alpha=0.60] [beta=0.40] [gamma=0.10]\n";
        return 1;
    }
    string P_ACC_K=argv[1], P_ACCOUNTS=argv[2], P_TX=argv[3], OUTDIR=argv[4];
    long long NseedTop = (argc>=6? atoll(argv[5]) : 50);
    double lambda = (argc>=7? atof(argv[6]) : 0.30);
    double alpha  = (argc>=8? atof(argv[7]) : 0.7);
    double beta   = (argc>=9? atof(argv[8]) : 0.40);
    double gamma  = (argc>=10? atof(argv[9]) : 0.10);
    system((string("mkdir -p ")+OUTDIR).c_str());

    // 1) 读账户真值（可选 CUSTOMER_ID）
    unordered_map<long long,int> y; y.reserve(1<<22);
    unordered_map<long long,string> cust; cust.reserve(1<<22);
    vector<long long> all_ids;
    {
        ifstream fin(P_ACCOUNTS); string line;
        if(!fin||!getline(fin,line)){ cerr<<"bad file: "<<P_ACCOUNTS<<"\n"; return 2; }
        auto hdr=split_simple(line,','); int cA=-1,cF=-1,cN=-1;
        for(int i=0;i<(int)hdr.size();++i){
            string h=trim(hdr[i]);
            if(h=="ACCOUNT_ID") cA=i;
            if(h=="IS_FRAUD")  cF=i;
            if(h=="CUSTOMER_ID") cN=i;
        }
        if(cA<0||cF<0){ cerr<<"accounts need ACCOUNT_ID,IS_FRAUD\n"; return 2; }
        string row;
        while(getline(fin,row)){
            if(row.empty()) continue; auto t=split_simple(row,',');
            if((int)t.size()<=max(cA,cF)) continue;
            long long id=atoll(trim(t[cA]).c_str());
            int lab=is_true_str(trim(t[cF]))?1:0;
            y[id]=lab; all_ids.push_back(id);
            if(cN>=0 && (int)t.size()>cN) cust[id]=trim(t[cN]);
        }
    }

    // 2) 读模型分数：AccountID,Layer
    unordered_map<long long,double> s_raw; s_raw.reserve(1<<22);
    {
        ifstream fin(P_ACC_K); string line;
        if(!fin||!getline(fin,line)){ cerr<<"bad file: "<<P_ACC_K<<"\n"; return 2; }
        auto hdr=split_simple(line,','); int cA=-1,cK=-1;
        for(int i=0;i<(int)hdr.size();++i){
            string h=trim(hdr[i]); if(h=="AccountID") cA=i; if(h=="Layer") cK=i;
        }
        if(cA<0||cK<0){ cerr<<"acc_k need AccountID,Layer\n"; return 2; }
        string row;
        while(getline(fin,row)){
            if(row.empty()) continue; auto t=split_simple(row,',');
            if((int)t.size()<=max(cA,cK)) continue;
            long long id=atoll(trim(t[cA]).c_str());
            double sv=atof(trim(t[cK]).c_str());
            s_raw[id]=sv; if(!y.count(id)){ y[id]=0; all_ids.push_back(id); }
        }
    }
    sort(all_ids.begin(), all_ids.end());
    all_ids.erase(unique(all_ids.begin(), all_ids.end()), all_ids.end());

    // 3) 读交易图（无向）
    unordered_map<long long, vector<long long>> G; G.reserve(1<<22);
    auto add_edge=[&](long long u,long long v){
        if(u==v) return; G[u].push_back(v); G[v].push_back(u);
    };
    {
        ifstream fin(P_TX); string line;
        if(!fin||!getline(fin,line)){ cerr<<"bad file: "<<P_TX<<"\n"; }
        else{
            auto hdr=split_simple(line,','); int cS=-1,cR=-1;
            for(int i=0;i<(int)hdr.size();++i){
                string h=trim(hdr[i]);
                if(h=="SENDER_ACCOUNT_ID")  cS=i;
                if(h=="RECEIVER_ACCOUNT_ID") cR=i;
            }
            if(cS>=0 && cR>=0){
                string row;
                while(getline(fin,row)){
                    if(row.empty()) continue; auto t=split_simple(row,',');
                    if((int)t.size()<=max(cS,cR)) continue;
                    long long u=atoll(trim(t[cS]).c_str());
                    long long v=atoll(trim(t[cR]).c_str());
                    add_edge(u,v);
                }
            }
        }
    }

        // 4) 排序（按原始分数）并确定“种子”= Top-NseedTop 中的阳性（并列随机）
    struct Row{ long long id; double score; int lab; };
    vector<Row> ranked; ranked.reserve(all_ids.size());
    for(auto id: all_ids){
        double sv = s_raw.count(id)? s_raw[id] : 0.0;
        ranked.push_back({id, sv, y[id]});
    }
    // 先按分数降序；同分时不再以lab优先，避免系统性偏置，交给后面的随机抽样
    stable_sort(ranked.begin(), ranked.end(),
        [](const Row&a,const Row&b){
            if(a.score!=b.score) return a.score>b.score;
            return a.id<b.id; // 稳定排序，为后续可重复性做准备
        });

    long long Ntake = min<long long>(NseedTop, ranked.size());
    unordered_set<long long> topNset; topNset.reserve(Ntake*2);

    if (Ntake > 0) {
        // 第 Ntake 名的分数阈值
        double s_cut = ranked[Ntake-1].score;

        // 严格大于阈值的全部入选（必须选）
        size_t strict_cnt = 0;
        while (strict_cnt < ranked.size() && ranked[strict_cnt].score > s_cut) ++strict_cnt;
        for (size_t i=0; i<strict_cnt; ++i) topNset.insert(ranked[i].id);

        // 等于阈值的一整块，随机抽取剩余名额
        vector<size_t> tie_idx;
        for (size_t i=strict_cnt; i<ranked.size() && ranked[i].score==s_cut; ++i) tie_idx.push_back(i);

        long long need = Ntake - (long long)topNset.size();
        if (need > 0) {
            // 随机数种子：优先读环境变量 RAND_SEED（便于复现实验），否则用高精时钟
            uint64_t seed = (uint64_t)chrono::high_resolution_clock::now().time_since_epoch().count();
            if (const char* env = getenv("RAND_SEED")) {
                try { seed = stoull(string(env)); } catch(...) {}
            }
            mt19937_64 rng(seed);
            shuffle(tie_idx.begin(), tie_idx.end(), rng);

            for (size_t i=0; i<(size_t)need && i<tie_idx.size(); ++i) {
                topNset.insert(ranked[tie_idx[i]].id);
            }

            // 极端情况下（并列块也不够），继续顺延补满
            for (size_t i=strict_cnt + tie_idx.size();
                 (long long)topNset.size()<Ntake && i<ranked.size();
                 ++i) {
                topNset.insert(ranked[i].id);
            }
        }
    }

    // 最终“种子”= 选中 TopN 集合中的阳性
    unordered_set<long long> seeds; seeds.reserve(Ntake*2);
    for (auto id : topNset) if (y[id]) seeds.insert(id);

    // 5) 模型原始分数 -> 分位分（0~1）
    auto s_model = to_percentile_score(all_ids, s_raw);

    // 6) 方案 A：邻域曝光特征 + 融合
    struct LocalFeat {
    int deg=0;
    int seed1=0;           // 1-hop 种子个数（未加权）
    double frac1=0.0;      // = seed1 / deg
    double frac1_idw=0.0;  // 逆度加权 1-hop 暴露（邻居权重 1/sqrt(deg(nei))）
    double aa1=0.0;        // Adamic–Adar w.r.t. seeds: sum_{v in N(u)∩Seeds} 1/log(1+deg(v))
    double jacc=0.0;       // 与种子集合的邻域杰卡德（可选）
    double seed2=0.0;      // 二阶暴露（仍保留）
    double seed2_norm=0.0; // 二阶规范化
    double deg_pen=0.0;    // 1/sqrt(deg(u))
};

// —— 计算度与 1-hop 特征
unordered_map<long long, LocalFeat> F; F.reserve(G.size());

// 先预备每个点的度，和邻居的逆度、AA 权重累加
for (auto &kv : G) {
    long long u = kv.first;
    auto &f = F[u];
    f.deg = (int)kv.second.size();
    f.deg_pen = 1.0 / sqrt(max(1, f.deg));

    // 分母：逆度权重和（用于把加权命中转成比例）
    double sum_idw = 0.0;
    int s1 = 0;
    double sum_idw_hit = 0.0;
    double aa_sum = 0.0;
    unordered_set<long long> Nu(kv.second.begin(), kv.second.end()); // 为 Jaccard 服务

    for (auto v : kv.second) {
        int degv = (int)G[v].size();
        double w_idw = 1.0 / sqrt(max(1, degv));
        sum_idw += w_idw;

        if (seeds.count(v)) {
            ++s1;
            sum_idw_hit += w_idw;

            // AA：1/log(1+deg(v))，度为1时也有定义
            double w_aa = 1.0 / log(1.0 + (double)max(1, degv));
            aa_sum += w_aa;
        }
    }

    f.seed1 = s1;
    f.frac1 = (f.deg>0 ? (double)s1 / (double)f.deg : 0.0);
    f.frac1_idw = (sum_idw>0 ? sum_idw_hit / sum_idw : 0.0);
    f.aa1 = aa_sum;


}

// 二阶暴露 & 归一化（保留原有）
double mn=1e100, mx=-1e100;
for (auto &kv : G) {
    long long u = kv.first;
    double s2 = 0.0;
    for (auto v : kv.second) {
        auto it = F.find(v);
        if (it!=F.end()) s2 += it->second.frac1_idw; // 注意：这里用加权版 frac1_idw 更稳
    }
    F[u].seed2 = s2;
    mn = min(mn, s2); mx = max(mx, s2);
}
for (auto &kv : F) {
    kv.second.seed2_norm = (mx>mn ? (kv.second.seed2 - mn) / (mx - mn) : 0.0);
}
    // 6.4 融合分
    struct RowF { long long id; double model_pct, frac1, seed2n, degpen, exp_local, final_s; int lab; int is_seed; };
    vector<RowF> fused; fused.reserve(ranked.size());
    for(auto &r: ranked){
        double m = s_model.count(r.id)? s_model[r.id] : 0.0;
        auto it = F.find(r.id);
        double frac1 = (it!=F.end()? it->second.frac1 : 0.0);
        double seed2n= (it!=F.end()? it->second.seed2_norm : 0.0);
        double degpen= (it!=F.end()? it->second.deg_pen : 0.0);
        double exp_local = alpha*frac1 + beta*seed2n + gamma*degpen;
        double f = (1.0 - lambda)*m + lambda*exp_local;
        fused.push_back({r.id, m, frac1, seed2n, degpen, exp_local, f, r.lab, (int)seeds.count(r.id)});
    }
    stable_sort(fused.begin(), fused.end(),
        [](const RowF&a,const RowF&b){
            if(a.final_s!=b.final_s) return a.final_s>b.final_s;
            return a.lab>b.lab;
        });

    // 7) 评估（含种子）
    vector<pair<double,int>> sl; sl.reserve(fused.size());
    for(auto &r: fused) sl.push_back({r.final_s, r.lab});
    double roc = auc_roc(sl);
    double ap  = auc_pr_ap(sl);
    long long P=0; for(auto &r: fused) if(r.lab) ++P;
    long long V=(long long)fused.size();
    double base = (V? (double)P/V : 0.0);
    auto metrK = eval_topN(sl, max(1LL, min(P, V)));
        // 8-) 多窗口 TopN 评估与导出
    auto capN = [&](long long N){ return max(1LL, min(N, (long long)fused.size())); };
    long long Kpos = (long long)P; // 
    struct Win { string name; long long N; };
    vector<Win> wins = {
        {"topK",      capN(Kpos)},
        {"top2K",     capN(2*Kpos)},
        {"top0.5K",   capN(Kpos/2)},
        {"top50",     capN(50)},
        {"top100",    capN(100)},
        {"top500",    capN(500)}
    };

    // 计算每个窗口的指标
    map<string, TopNRes> win_res;
    for (auto &w : wins) {
    auto r = eval_topN(sl, w.N);
    auto [roc, ap] = eval_auc_ap(sl, w.N);
    r.roc = roc; r.ap = ap;
    win_res[w.name] = r;
}

    // 导出 windows_summary.txt
    {
        ofstream fo(OUTDIR + string("/windows_summary.txt"));
        fo << fixed << setprecision(6);
        fo << "# Top-N windows summary\n";
        fo << "num_accounts=" << V << ", positives=" << P << ", base_rate=" << (V? (double)P/V : 0.0) << "\n";
        fo << "windows: topK=" << capN(Kpos) << ", top2K=" << capN(2*Kpos) << ", top0.5K=" << capN(Kpos/2)
           << ", top50=" << capN(50) << ", top100=" << capN(100) << ", top500=" << capN(500) << "\n\n";
        fo << "window,N,tp,fp,fn,precision,recall,F1,hit_rate,ROC_AUC,PR_AUC(AP)\n";
for (auto &w : wins) {
    auto &r = win_res[w.name];
    double hit_rate = (r.N>0? (double)r.tp / (double)r.N : 0.0);
    fo << w.name << "," << r.N << "," << r.tp << "," << r.fp << "," << r.fn << ","
       << r.precision << "," << r.recall << "," << r.f1 << "," << hit_rate << ","
       << r.roc << "," << r.ap << "\n";
}
    }

    // 为快速标注命中，构建 id->rank 的索引（rank 从 1 开始）
    unordered_map<long long, long long> id2rank; id2rank.reserve(fused.size()*1.3);
    for (size_t i=0;i<fused.size();++i) id2rank[fused[i].id] = (long long)i + 1;

    // 各窗口榜单导出（是否命中=label==1；是否在该窗口=rank<=N）
    auto dump_window_csv = [&](const string& name, long long N){
        ofstream fo(OUTDIR + string("/window_") + name + string(".csv"));
        fo << "rank,account_id,customer_id,final_score,label,is_hit,is_seed,model_pct,frac1,seed2_norm,deg_pen,exp_local\n";
        fo << fixed << setprecision(6);
        for (long long i=0; i<N && i<(long long)fused.size(); ++i) {
            const auto &r = fused[i];
            int is_hit = r.lab; // 窗口中命中=阳性
            fo << (i+1) << "," << r.id << "," << cust[r.id] << ","
               << r.final_s << "," << r.lab << "," << is_hit << "," << r.is_seed << ","
               << r.model_pct << "," << r.frac1 << "," << r.seed2n << "," << r.degpen << "," << r.exp_local << "\n";
        }
    };
    for (auto &w : wins) dump_window_csv(w.name, w.N);

    // 生成带各窗口布尔标记的总榜单
    {
        ofstream fo(OUTDIR + string("/ranked_with_windows.csv"));
        fo << "rank,account_id,customer_id,final_score,label,is_seed,"
              "in_topK,in_top2K,in_top0.5K,in_top50,in_top100,in_top500,"
              "model_pct,frac1,seed2_norm,deg_pen,exp_local\n";
        fo << fixed << setprecision(6);

        // 预备窗口阈值（rank<=阈值即在窗口）
        unordered_map<string,long long> thr;
        for (auto &w : wins) thr[w.name] = w.N;

        for (size_t i=0;i<fused.size();++i){
            const auto &r = fused[i];
            long long rk = (long long)i + 1;
            int in_topK     = (rk <= thr["topK"]);
            int in_top2K    = (rk <= thr["top2K"]);
            int in_top05K   = (rk <= thr["top0.5K"]);
            int in_top50    = (rk <= thr["top50"]);
            int in_top100   = (rk <= thr["top100"]);
            int in_top500   = (rk <= thr["top500"]);
            fo << rk << "," << r.id << "," << cust[r.id] << ","
               << r.final_s << "," << r.lab << "," << r.is_seed << ","
               << in_topK << "," << in_top2K << "," << in_top05K << ","
               << in_top50 << "," << in_top100 << "," << in_top500 << ","
               << r.model_pct << "," << r.frac1 << "," << r.seed2n << "," << r.degpen << "," << r.exp_local << "\n";
        }
    }

    // 8) 输出
    {
        ofstream fo(OUTDIR + string("/metrics_overall.txt"));
        fo<<fixed<<setprecision(6);
        fo<<"# Local exposure + model percentile, evaluated on ALL accounts (including seeds)\n";
        fo<<"num_accounts="<<V<<", positives="<<P<<", base_rate="<<base<<"\n";
        fo<<"params: NseedTop="<<NseedTop<<", lambda="<<lambda
          <<", alpha="<<alpha<<", beta="<<beta<<", gamma="<<gamma<<"\n\n";
        fo<<"ROC_AUC="<<roc<<"\n";
        fo<<"PR_AUC(AP)="<<ap<<"\n\n";
        fo<<"F1@K (K=positives): N="<<metrK.N
          <<" tp="<<metrK.tp<<" fp="<<metrK.fp<<" fn="<<metrK.fn
          <<" precision="<<metrK.precision<<" recall="<<metrK.recall<<" F1="<<metrK.f1<<"\n";
    }
    {
        ofstream fo(OUTDIR + string("/ranked_all.csv"));
        fo<<"rank,account_id,customer_id,model_pct,frac1,seed2_norm,deg_pen,exp_local,final_score,label,is_seed_top"<<NseedTop<<"\n";
        fo<<fixed<<setprecision(6);
        for(size_t i=0;i<fused.size();++i){
            long long id=fused[i].id;
            fo<<(i+1)<<","<<id<<","<<cust[id]<<","
              <<fused[i].model_pct<<","<<fused[i].frac1<<","<<fused[i].seed2n<<","<<fused[i].degpen<<","
              <<fused[i].exp_local<<","<<fused[i].final_s<<","<<fused[i].lab<<","<<fused[i].is_seed<<"\n";
        }
    }
        {
        ofstream fo(OUTDIR + string("/seeds_in_topN.csv"));
        fo<<"rank_in_overall,account_id,customer_id,model_raw,label\n";
        long long r=0;
        // 我们需要输出这些阳性种子在 overall 排名中的名次
        // 先构建 id -> rank（从 1 开始）
        unordered_map<long long,long long> id2rk; id2rk.reserve(ranked.size()*1.3);
        for (size_t i=0;i<ranked.size();++i) id2rk[ranked[i].id] = (long long)i+1;

        for (auto id : topNset) {
            // 只输出阳性
            if (y[id]) {
                ++r;
                long long rk = id2rk[id];
                double raw = s_raw.count(id)? s_raw[id] : 0.0;
                fo<<rk<<","<<id<<","<<cust[id]<<","<<fixed<<setprecision(6)<<raw<<","<<1<<"\n";
            }
        }
    }
    cerr<<"[OK] Outputs -> "<<OUTDIR<<"\n";
    return 0;
}