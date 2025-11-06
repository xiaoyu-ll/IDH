
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

using namespace std;

static inline string trim(const string& s){
    size_t l=0,r=s.size();
    while(l<r && isspace((unsigned char)s[l])) ++l;
    while(r>l && isspace((unsigned char)s[r-1])) --r;
    return s.substr(l,r-l);
}
static inline vector<string> split_comma(const string& line){
    vector<string> out; string cur;
    for(char ch: line){
        if(ch==','){ out.push_back(trim(cur)); cur.clear(); }
        else cur.push_back(ch);
    }
    if(!cur.empty()) out.push_back(trim(cur));
    return out;
}
static inline bool to_int(const string& s, int& v){
    if(s.empty()) return false; char* e=nullptr;
    long x=strtol(s.c_str(), &e, 10);
    if(*e!='\0') return false; v=(int)x; return true;
}

struct PairHash{
    size_t operator()(const pair<int,int>& p) const noexcept {
        return (static_cast<size_t>(p.first)*1000003u) ^ static_cast<size_t>(p.second);
    }
};
static inline pair<int,int> norm_pair(int a,int b){ if(a>b) swap(a,b); return {a,b}; }

// IO
static void read_edges(const string& path, vector<vector<int>>& bills, int& max_id){
    ifstream fin(path);
    if(!fin){ cerr<<"open edges fail: "<<path<<"\n"; exit(1); }
    string line; int x;
    while(getline(fin,line)){
        line=trim(line); if(line.empty()) continue;
        auto t=split_comma(line); vector<int> e;
        for(auto& s: t){ if(to_int(s,x)){ e.push_back(x); max_id=max(max_id,x);} }
        if(!e.empty()) bills.push_back(std::move(e));
    }
}
static void read_layers(const string& path, unordered_map<int,int>& id2layer){
    ifstream fin(path);
    if(!fin){ cerr<<"open layers fail: "<<path<<"\n"; exit(1); }
    string line; bool header_unknown=true;
    while(getline(fin,line)){
        line=trim(line); if(line.empty()) continue;
        if(header_unknown){
            auto probe = split_comma(line);
            bool header_like=false;
            for(auto & tok : probe){
                string low=tok;
                for(char& c: low) c=(char)tolower((unsigned char)c);
                if(low.find("account")!=string::npos || low.find("layer")!=string::npos){
                    header_like=true; break;
                }
            }
            if(header_like){ header_unknown=false; continue; }
            header_unknown=false;
        }
        auto t=split_comma(line);
        if(t.size()<2) continue;
        int idv,kv;
        if(to_int(t[0],idv) && to_int(t[1],kv)) id2layer[idv]=kv;
    }
}
static void compress_layers(unordered_map<int,int>& id2layer){
    vector<int> uniq;
    uniq.reserve(id2layer.size());
    for(auto& kv: id2layer) uniq.push_back(kv.second);
    sort(uniq.begin(), uniq.end());
    uniq.erase(unique(uniq.begin(), uniq.end()), uniq.end());
    unordered_map<int,int> mapv;
    for(size_t i=0;i<uniq.size();++i) mapv[uniq[i]]=(int)i;
    for(auto& kv: id2layer) kv.second = mapv[kv.second];
}

static void nodes_from_bills(const vector<vector<int>>& bills, vector<int>& nodes){
    unordered_set<int> S;
    for(const auto& e: bills) for(int u: e) S.insert(u);
    nodes.assign(S.begin(), S.end());
}
static void pairs_from_bills(const vector<vector<int>>& bills,
                             unordered_set<pair<int,int>,PairHash>& P){
    for(const auto& e: bills){
        for(size_t i=0;i<e.size();++i){
            for(size_t j=i+1;j<e.size();++j){
                P.insert(norm_pair(e[i], e[j]));
            }
        }
    }
}

// quantization
static void quantize_layers_equal_freq(const vector<int>& train_nodes,
                                       unordered_map<int,int>& id2layer,
                                       int bins){
    if(bins<=0) return;
    unordered_map<int,int> layer_count;
    for(int u: train_nodes){
        auto it=id2layer.find(u);
        if(it!=id2layer.end()) layer_count[it->second]++;
    }
    if(layer_count.empty()) return;
    vector<int> layers;
    for(auto& kv: layer_count) layers.push_back(kv.first);
    sort(layers.begin(), layers.end());
    long long total=0;
    for(int L: layers) total += layer_count[L];
    if(total==0) return;
    long long target = max(1LL, total / (long long)bins);

    unordered_map<int,int> layer2bin;
    long long acc=0; int b=0;
    for(size_t i=0;i<layers.size();++i){
        int L=layers[i];
        if(acc>= (long long)(b+1)*target && b+1 < bins) b++;
        layer2bin[L]=b;
        acc += layer_count[L];
    }
    for(auto& kv: id2layer){
        auto it=layer2bin.find(kv.second);
        if(it!=layer2bin.end()) kv.second = it->second;
        else kv.second = min(kv.second, bins-1);
    }
}

// degree for deg-matching
static void degree_from_bills(const vector<vector<int>>& bills, unordered_map<int,int>& deg){
    for(const auto& e: bills){
        for(size_t i=0;i<e.size();++i){
            for(size_t j=i+1;j<e.size();++j){
                deg[e[i]]++; deg[e[j]]++;
            }
        }
    }
}
static inline int deg_bucket(int d){
    if(d<=0) return 0;
    int b=0; d++;
    while(d>1){ d>>=1; b++; }
    return b;
}

// metrics
static double auc_from_scores(const vector<pair<double,int>>& sc){
    int64_t P=0,N=0;
    for(auto& p: sc){ if(p.second) P++; else N++; }
    if(P==0 || N==0) return 0.5;
    vector<pair<double,int>> s=sc;
    sort(s.begin(), s.end(), [](auto& a, auto& b){
        if(a.first!=b.first) return a.first < b.first;
        return a.second < b.second;
    });
    double rank_sum_pos=0.0; int i=0; int n=(int)s.size();
    while(i<n){
        int j=i;
        while(j<n && s[j].first==s[i].first) ++j;
        double r_low=i+1, r_high=j, r_avg=(r_low+r_high)/2.0;
        for(int k=i;k<j;k++) if(s[k].second==1) rank_sum_pos += r_avg;
        i=j;
    }
    double U = rank_sum_pos - (double)P*(P+1)/2.0;
    return U / (double)(P*N);
}
static double ap_from_scores(vector<pair<double,int>> sc){
    sort(sc.begin(), sc.end(), [](auto& a, auto& b){
        if(a.first!=b.first) return a.first > b.first;
        return a.second > b.second;
    });
    int P=0; for(auto& x: sc) if(x.second==1) P++;
    if(P==0) return 0.0;
    int tp=0; double sum_prec=0.0;
    for(size_t i=0;i<sc.size();++i){
        if(sc[i].second==1){
            tp++;
            sum_prec += (double)tp / (double)(i+1);
        }
    }
    return sum_prec / (double)P;
}

// 1D calibration P(pos | |dL|)
static vector<double> calibrate_prob1d(const unordered_set<pair<int,int>,PairHash>& train_pos,
                                       const vector<int>& train_pool,
                                       const unordered_map<int,int>& id2layer,
                                       int train_neg_ratio,
                                       unsigned seed){
    int maxd=0; vector<int> pos_d; pos_d.reserve(train_pos.size());
    for(auto& pr: train_pos){
        auto it1=id2layer.find(pr.first);
        auto it2=id2layer.find(pr.second);
        if(it1==id2layer.end() || it2==id2layer.end()) continue;
        int d=abs(it1->second - it2->second);
        pos_d.push_back(d); maxd=max(maxd,d);
    }
    vector<int> pos_cnt(maxd+1,0), neg_cnt(maxd+1,0);
    for(int d: pos_d) pos_cnt[d]++;

    // negatives from train_pool
    vector<pair<int,int>> cand;
    for(size_t i=0;i<train_pool.size();++i)
        for(size_t j=i+1;j<train_pool.size();++j)
            cand.push_back(norm_pair(train_pool[i], train_pool[j]));
    unordered_set<pair<int,int>,PairHash> pos_set=train_pos;
    vector<pair<int,int>> neg;
    neg.reserve((size_t)train_neg_ratio*pos_set.size());
    std::mt19937 rng(seed);
    std::shuffle(cand.begin(), cand.end(), rng);
    for(auto& pr: cand){
        if(!pos_set.count(pr)){
            neg.push_back(pr);
            if(neg.size()>= (size_t)train_neg_ratio*pos_set.size()) break;
        }
    }
    for(auto& pr: neg){
        auto it1=id2layer.find(pr.first);
        auto it2=id2layer.find(pr.second);
        if(it1==id2layer.end() || it2==id2layer.end()) continue;
        int d=abs(it1->second - it2->second);
        if(d>maxd){ pos_cnt.resize(d+1,0); neg_cnt.resize(d+1,0); maxd=d; }
        neg_cnt[d]++;
    }
    vector<double> prob(maxd+1,0.0);
    const double alpha=1.0;
    for(int d=0; d<=maxd; ++d){
        double p=(pos_cnt[d]+alpha)/(pos_cnt[d]+neg_cnt[d]+2*alpha);
        prob[d]=p;
    }
    // non-increasing enforce
    for(int d=1; d<=maxd; ++d){
        if(prob[d] > prob[d-1]) prob[d]=prob[d-1];
    }
    return prob;
}

// 2D calibration P(pos | min(l), max(l))
static vector<vector<double>> calibrate_prob2d(const unordered_set<pair<int,int>,PairHash>& train_pos,
                                               const vector<int>& train_pool,
                                               const unordered_map<int,int>& id2layer,
                                               int train_neg_ratio,
                                               unsigned seed,
                                               int& L_out){
    int Lmax=0;
    for(auto& kv: id2layer) Lmax=max(Lmax, kv.second);
    int L=Lmax+1;
    L_out=L;
    vector<vector<int>> pos_cnt(L, vector<int>(L,0));
    vector<vector<int>> neg_cnt(L, vector<int>(L,0));

    auto add_cnt = [&](int u,int v, bool is_pos){
        auto it1=id2layer.find(u), it2=id2layer.find(v);
        if(it1==id2layer.end() || it2==id2layer.end()) return;
        int a=min(it1->second, it2->second);
        int b=max(it1->second, it2->second);
        if(is_pos) pos_cnt[a][b]++; else neg_cnt[a][b]++;
    };

    for(auto& pr: train_pos) add_cnt(pr.first, pr.second, true);

    // negatives sample
    vector<pair<int,int>> cand;
    unordered_set<pair<int,int>,PairHash> pos_set=train_pos;
    for(size_t i=0;i<train_pool.size();++i){
        for(size_t j=i+1;j<train_pool.size();++j){
            auto pr=norm_pair(train_pool[i], train_pool[j]);
            if(!pos_set.count(pr)) cand.push_back(pr);
        }
    }
    size_t target=min((size_t)train_neg_ratio*pos_set.size(), cand.size());
    std::mt19937 rng(seed);
    std::shuffle(cand.begin(), cand.end(), rng);
    for(size_t i=0;i<target;i++) add_cnt(cand[i].first, cand[i].second, false);

    // Laplace smoothing
    vector<vector<double>> prob(L, vector<double>(L, 0.0));
    const double alpha=1.0;
    for(int a=0;a<L;a++){
        for(int b=a;b<L;b++){
            double p=(pos_cnt[a][b]+alpha)/(pos_cnt[a][b]+neg_cnt[a][b]+2*alpha);
            prob[a][b]=p;
        }
    }
    // slight smoothing along diagonals to reduce sparsity
    for(int iter=0; iter<1; ++iter){
        for(int a=0;a<L;a++){
            for(int b=a;b<L;b++){
                double s=prob[a][b]; int c=1;
                if(a>0){ s+=prob[a-1][b]; c++; }
                if(b+1<L){ s+=prob[a][b+1]; c++; }
                prob[a][b]=s/c;
            }
        }
    }
    return prob;
}

// node signatures from TRAIN: mean co-sponsor layer
static void compute_signatures(const vector<vector<int>>& bills_train,
                               const unordered_map<int,int>& id2layer,
                               unordered_map<int,double>& sig){
    unordered_map<int, long long> sumL;
    unordered_map<int, long long> cnt;
    for(const auto& e: bills_train){
        for(size_t i=0;i<e.size();++i){
            int u=e[i];
            auto itu=id2layer.find(u);
            if(itu==id2layer.end()) continue;
            for(size_t j=0;j<e.size();++j){
                if(i==j) continue;
                int v=e[j];
                auto itv=id2layer.find(v);
                if(itv==id2layer.end()) continue;
                sumL[u]+=itv->second;
                cnt[u]++;
            }
        }
    }
    for(auto& kv: sumL){
        int u=kv.first;
        if(cnt[u]>0) sig[u]=(double)kv.second / (double)cnt[u];
    }
}

// main
int main(int argc, char** argv){
    ios::sync_with_stdio(false); cin.tie(nullptr);
    if(argc<4){
        cerr<<"Usage: "<<argv[0]<<" <edges_train.txt> <edges_test.txt> <layer_train.txt>\n"
               "  [--only_new] [--all_neg]\n"
               "  [--neg_ratio 5] [--seed 42]\n"
               "  [--quant_bins N]\n"
               "  [--neg_degmatch]\n"
               "  [--train_neg_ratio 5]\n";
        return 1;
    }
    string f_train=argv[1], f_test=argv[2], f_layer=argv[3];
    bool only_new=false, all_neg=false, neg_degmatch=false;
    int neg_ratio=5, quant_bins=0, train_neg_ratio=5;
    unsigned seed=42;
    for(int i=4;i<argc;i++){
        string k=argv[i];
        auto val = [&](int idx)->string{ return (idx+1<argc)? string(argv[idx+1]) : string(); };
        if(k=="--only_new") only_new=true;
        else if(k=="--all_neg") all_neg=true;
        else if(k=="--neg_degmatch") neg_degmatch=true;
        else if(k=="--neg_ratio"){ neg_ratio=stoi(val(i)); i++; }
        else if(k=="--quant_bins"){ quant_bins=stoi(val(i)); i++; }
        else if(k=="--train_neg_ratio"){ train_neg_ratio=stoi(val(i)); i++; }
        else if(k=="--seed"){ seed=(unsigned)stoul(val(i)); i++; }
    }

    int max_id=0;
    vector<vector<int>> bills_train,bills_test;
    unordered_map<int,int> id2layer;
    read_edges(f_train, bills_train, max_id);
    read_edges(f_test,  bills_test,  max_id);
    read_layers(f_layer, id2layer);
    compress_layers(id2layer);

    vector<int> train_nodes, test_nodes;
    nodes_from_bills(bills_train, train_nodes);
    nodes_from_bills(bills_test,  test_nodes);
    if(quant_bins>0){
        quantize_layers_equal_freq(train_nodes, id2layer, quant_bins);
    }

    unordered_set<pair<int,int>,PairHash> train_pos, test_pos;
    pairs_from_bills(bills_train, train_pos);
    pairs_from_bills(bills_test,  test_pos);
    if(only_new){
        unordered_set<pair<int,int>,PairHash> filtered;
        filtered.reserve(test_pos.size()*2);
        for(auto& p: test_pos) if(!train_pos.count(p)) filtered.insert(p);
        test_pos.swap(filtered);
    }
    {
        unordered_set<pair<int,int>,PairHash> filtered;
        for(auto& p: test_pos){
            if(id2layer.count(p.first) && id2layer.count(p.second)) filtered.insert(p);
        }
        test_pos.swap(filtered);
    }
    vector<int> train_pool, test_pool;
    for(int u: train_nodes) if(id2layer.count(u)) train_pool.push_back(u);
    for(int u: test_nodes)  if(id2layer.count(u)) test_pool.push_back(u);

    // TEST negatives
    vector<pair<int,int>> neg_candidates;
    for(size_t i=0;i<test_pool.size();++i)
        for(size_t j=i+1;j<test_pool.size();++j){
            auto pr=norm_pair(test_pool[i], test_pool[j]);
            if(!test_pos.count(pr)) neg_candidates.push_back(pr);
        }
    unordered_set<pair<int,int>,PairHash> test_neg;
    if(all_neg){
        test_neg.insert(neg_candidates.begin(), neg_candidates.end());
    }else{
        if(!neg_degmatch){
            size_t target=min((size_t)neg_ratio*test_pos.size(), neg_candidates.size());
            std::mt19937 rng(seed);
            std::shuffle(neg_candidates.begin(), neg_candidates.end(), rng);
            for(size_t i=0;i<target;i++) test_neg.insert(neg_candidates[i]);
        }else{
            unordered_map<int,int> deg_train;
            degree_from_bills(bills_train, deg_train);
            auto pair_bucket=[&](int u,int v)->pair<int,int>{
                auto itu=deg_train.find(u), itv=deg_train.find(v);
                int bu=0,bv=0;
                if(itu!=deg_train.end()) bu=deg_bucket(itu->second);
                if(itv!=deg_train.end()) bv=deg_bucket(itv->second);
                if(bu>bv) swap(bu,bv);
                return {bu,bv};
            };
            auto key=[&](int a,int b)->long long{ return ((long long)a<<32)|(unsigned int)b; };
            unordered_map<long long,int> pos_bucket_cnt;
            for(auto& pr: test_pos){
                auto bk=pair_bucket(pr.first, pr.second);
                pos_bucket_cnt[key(bk.first,bk.second)]++;
            }
            unordered_map<long long, vector<pair<int,int>>> cand_by_bucket;
            for(auto& pr: neg_candidates){
                auto bk=pair_bucket(pr.first, pr.second);
                cand_by_bucket[key(bk.first,bk.second)].push_back(pr);
            }
            std::mt19937 rng(seed);
            for(auto& kv: pos_bucket_cnt){
                int need=min((long long)neg_ratio*(long long)kv.second,(long long)cand_by_bucket[kv.first].size());
                auto& vec=cand_by_bucket[kv.first];
                if(!vec.empty()){
                    std::shuffle(vec.begin(), vec.end(), rng);
                    for(int i=0;i<need;i++) test_neg.insert(vec[i]);
                }
            }
            size_t target=min((size_t)neg_ratio*test_pos.size(), neg_candidates.size());
            if(test_neg.size()<target){
                std::shuffle(neg_candidates.begin(), neg_candidates.end(), rng);
                for(size_t i=0;i<neg_candidates.size() && test_neg.size()<target; ++i)
                    test_neg.insert(neg_candidates[i]);
            }
        }
    }

    // Calibration 1D & 2D
    vector<double> prob1d = calibrate_prob1d(train_pos, train_pool, id2layer, train_neg_ratio, seed);
    int L=0;
    vector<vector<double>> prob2d = calibrate_prob2d(train_pos, train_pool, id2layer, train_neg_ratio, seed, L);

    // Signatures
    unordered_map<int,double> sig;
    compute_signatures(bills_train, id2layer, sig);

    // scoring
    auto layerprox=[&](int u,int v)->double{
        int du=abs(id2layer.at(u)-id2layer.at(v));
        return 1.0/(1.0+(double)du);
    };
    auto prob_1d=[&](int u,int v)->double{
        int du=abs(id2layer.at(u)-id2layer.at(v));
        if(du >= (int)prob1d.size()) return prob1d.back();
        return prob1d[du];
    };
    auto prob_2d=[&](int u,int v)->double{
        int lu=id2layer.at(u), lv=id2layer.at(v);
        int a=min(lu,lv), b=max(lu,lv);
        if(a>=L || b>=L) return 0.0;
        return prob2d[a][b];
    };
    auto sigscore=[&](int u,int v)->double{
        auto itu=sig.find(u), itv=sig.find(v);
        if(itu==sig.end() || itv==sig.end()) return 0.0;
        double d=fabs(itu->second - itv->second);
        return 1.0/(1.0 + d);
    };
    auto hybrid2=[&](int u,int v)->double{
        return 0.5*layerprox(u,v) + 0.5*prob_2d(u,v);
    };

    vector<pair<double,int>> sc_prox, sc_p1d, sc_p2d, sc_sig, sc_h2;
    sc_prox.reserve(test_pos.size()+test_neg.size());
    sc_p1d.reserve(test_pos.size()+test_neg.size());
    sc_p2d.reserve(test_pos.size()+test_neg.size());
    sc_sig.reserve(test_pos.size()+test_neg.size());
    sc_h2.reserve(test_pos.size()+test_neg.size());

    auto push=[&](const pair<int,int>& p, int y){
        int u=p.first,v=p.second;
        sc_prox.emplace_back(layerprox(u,v), y);
        sc_p1d.emplace_back(prob_1d(u,v), y);
        sc_p2d.emplace_back(prob_2d(u,v), y);
        sc_sig.emplace_back(sigscore(u,v), y);
        sc_h2.emplace_back(hybrid2(u,v), y);
    };
    for(auto& p: test_pos) push(p,1);
    for(auto& p: test_neg) push(p,0);

    cout.setf(ios::fixed); cout<<setprecision(6);
    cout<<"[DIAG] train_pos="<<train_pos.size()<<" train_pool="<<train_pool.size()<<"\n";
    cout<<"[DIAG] test_pos="<<test_pos.size()<<" test_pool="<<test_pool.size()
        <<" test_neg="<<test_neg.size()<<"\n";
    cout<<"[DIAG] quant_bins="<<quant_bins<<" train_neg_ratio="<<train_neg_ratio<<"\n";
    cout<<"===== Future Co-sponsorship Prediction (train-only layers, ADV) =====\n";
    cout<<"Train bills="<<bills_train.size()<<", Test bills="<<bills_test.size()<<"\n";
    cout<<"Positives(Test"<<(only_new?"-only-new":"")<<")="<<test_pos.size()
        <<", Negatives="<<test_neg.size()
        <<", Nodes_with_layers_in_test="<<test_pool.size()<<"\n";

    auto prt=[&](const string& name, const vector<pair<double,int>>& sc){
        cout<<name<<": AUC="<<auc_from_scores(sc)<<"  AP="<<ap_from_scores(sc)<<"\n";
    };
    prt("Scoring=Prob2D(train min/max layer)", sc_p2d);

    return 0;
}
