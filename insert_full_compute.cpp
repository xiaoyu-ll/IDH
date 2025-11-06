#include<iostream>
#include<fstream>
#include<sstream>
#include<string>
#include<algorithm>
#include<cmath>
#include<map>
#include<vector>
#include<stdlib.h>
#include <iomanip>
#include<stack>
#include<queue>
#include <cstring>
#include <thread>
#include <chrono>

using namespace std;

// ===== 配置 =====
#define maxn 25000 //hc
using namespace std;
const int vm=16000000;
const int em=6000000;

// ===== 数据结构 =====
struct Vertex {
    int id=0;
    int degree=0;
    int indegree=0;
    vector<int> tedge; // 结点指向边
    vector<int> hedge; // 边指向结点
    bool flag=false;
};
struct Hyperedge {
    int id=0;
    vector<int> varr;   // 顶点
    vector<int> vtarr;  // 结点指向边(出)
    vector<int> vharr;  // 边指向结点(入)
    int v=0;            // 超边大小
    int h=0;
};

Vertex vertex[vm];
Hyperedge hyperedge[em];
vector<int> dkv[maxn];
vector<int> layer[maxn];
int edgenum=0;
int nodenum=0;
bool dk[vm];
int idn[vm];
map<int,bool> fh;

// ===== 工具函数 =====
static inline string trim(const string& s){
    size_t i=0,j=s.size();
    while(i<j && isspace((unsigned char)s[i])) ++i;
    while(j>i && isspace((unsigned char)s[j-1])) --j;
    return s.substr(i,j-i);
}
static inline vector<int> split_ints_comma(const string& line)
{
    vector<int> v; v.reserve(64);
    int cur=0; 
    bool in_num=false, neg=false;
    for(char c: line)
    {
        if((c>='0' && c<='9') || c=='-')
        {
            if(c=='-')
            { 
                neg=true; 
                in_num=true; 
            }
            else
            {
                if(!in_num)
                { 
                    cur=0; 
                    neg=false; 
                    in_num=true; 
                }
                cur = cur*10 + (c-'0');
            }
        }
        else if(c==',' || isspace((unsigned char)c))
        {
            if(in_num)
            {
                v.push_back(neg? -cur : cur);
                in_num=false; cur=0; neg=false;
            }
        }
    }
    if(in_num) v.push_back(neg? -cur : cur);
    return v;
}

// ===== 输出当前 idn =====
void dump_idn_all(const string& path, int nodenum, const int idn[])
{
    ofstream fout(path);
    if(!fout)
    { 
        cerr<<"[ERR] cannot write "<<path<<"\n"; 
        return; 
    }
    for(int v=1; v<=nodenum; ++v)
    {
        fout << v << "," << idn[v] << "\n";
    }
}

// ===== 清空/重置全局状态（重构前调用）=====
void reset_state(){
    // 顶点
    for(int i=0;i<vm;++i)
    {
        vertex[i].id=0;
        vertex[i].degree=0;
        vertex[i].indegree=0;
        vertex[i].tedge.clear();
        vertex[i].hedge.clear();
        vertex[i].flag=false;
        dk[i]=false;
        idn[i]=0;
    }
    // 超边
    for(int i=0;i<em;++i)
    {
        hyperedge[i].id=0;
        hyperedge[i].varr.clear();
        hyperedge[i].vtarr.clear();
        hyperedge[i].vharr.clear();
        hyperedge[i].v=0;
        hyperedge[i].h=0;
    }
    // 层结构
    for(int i=0;i<maxn;++i)
    {
        dkv[i].clear();
        layer[i].clear();
    }
    edgenum=0;
    nodenum=0;
    fh.clear();
}

// ===== 从单个文件读边，追加到 [start_ei, ...) =====
int readedge_from(const string& filepath, int start_ei, int& nodemax_out)
{
    ifstream rda(filepath);
    if(!rda)
    { 
        cerr<<"[ERR] cannot open "<<filepath<<"\n"; 
        exit(1); 
    }
    string strline;
    int ei=start_ei;
    int dmax=0;
    while(rda>>strline)
    {
        auto arr = split_ints_comma(strline);
        if(arr.size()<1) continue;
        // 清空目标槽
        hyperedge[ei].varr.clear();
        hyperedge[ei].vtarr.clear();
        hyperedge[ei].vharr.clear();

        // 去重/排序
        sort(arr.begin(), arr.end());
        arr.erase(unique(arr.begin(), arr.end()), arr.end());
        if(arr.size()<1) continue;

        // 写入
        for(int v: arr){
            hyperedge[ei].varr.push_back(v);
            vertex[v].flag=true;
            vertex[v].degree++; // 用作统计
            if(v>nodemax_out) 
            nodemax_out=v;
        }
        hyperedge[ei].v = (int)hyperedge[ei].varr.size();
        dmax = max(dmax, hyperedge[ei].v);
        ++ei;
    }
    rda.close();
    return ei - start_ei; // 本文件加入的边数
}

// ===== 按文件序列完整读入（初始 + 若干新增批次）=====
int read_all_files(const vector<string>& files){
    int ei=0;
    int dmax_all=0;
    int local_maxnode=0;
    for(const auto& f: files)
    {
        int before = ei;
        int added  = readedge_from(f, ei, local_maxnode);
        ei += added;
        // 更新全局 nodenum
        nodenum = max(nodenum, local_maxnode);
        // 简单统计最大超边大小（可选）
        // dmax_all = max(dmax_all, dmax_file);
        (void)before; (void)dmax_all;
        cerr<<"[INFO] read "<<f<<" add_edges="<<added<<" ei_now="<<ei<<"\n";
    }
    edgenum = ei;
    return edgenum;
}


void orientation(int delta)
{
    for(int i=0;i<edgenum;i++)
    {
        int j=0;
        for(j=0;j<delta&&j<hyperedge[i].v;j++)
        {
            hyperedge[i].vharr.push_back(hyperedge[i].varr[j]);
            vertex[hyperedge[i].varr[j]].indegree++;
            vertex[hyperedge[i].varr[j]].hedge.push_back(i);
        }
        for(; j<hyperedge[i].v;j++)
        {
            hyperedge[i].vtarr.push_back(hyperedge[i].varr[j]);
            vertex[hyperedge[i].varr[j]].tedge.push_back(i);
        }
    }
}

bool reachdk(int vi, int k)
{
    queue<int>q;
    for(int i=0;i<(int)vertex[vi].tedge.size();i++)
    {
        for(int j=0;j<(int)hyperedge[vertex[vi].tedge[i]].vharr.size();j++)
        {
            int vv=hyperedge[vertex[vi].tedge[i]].vharr[j];
            if(vertex[vv].indegree>=k) 
            return true;
            if(!fh[vv]&&vertex[vv].indegree==k-1)
            {
                fh[vv]=true; 
                q.push(vv);
            }
        }
    }
    while(!q.empty())
    {
        if(reachdk(q.front(),k)) 
        return true;
        q.pop();
    }
    return false;
}

void finddk(int k)
{
    vector<int> veck;
    for(int i=1;i<=nodenum;i++)
    {
        if(dk[i]) 
        veck.push_back(i);
        fh.clear();
        if(!dk[i] && vertex[i].indegree==k-1 && reachdk(i,k))
        {
            veck.push_back(i);
            //dk[i]=true;
        }
    }
    dkv[k].clear();
    dkv[k].insert(dkv[k].end(), veck.begin(), veck.end());
}

void reverse_edge(int vi, int vj,int ee)
{
    auto pos = find(vertex[vi].hedge.begin(),vertex[vi].hedge.end(),ee);
    vertex[vi].indegree--;
    if (pos != vertex[vi].hedge.end()) 
    vertex[vi].hedge.erase(pos);
    vertex[vi].tedge.push_back(ee);

    pos = find(hyperedge[ee].vharr.begin(),hyperedge[ee].vharr.end(),vi);
    if (pos != hyperedge[ee].vharr.end()) 
    hyperedge[ee].vharr.erase(pos);
    hyperedge[ee].vtarr.push_back(vi);

    pos = find(hyperedge[ee].vtarr.begin(),hyperedge[ee].vtarr.end(),vj);
    if (pos != hyperedge[ee].vtarr.end()) 
    hyperedge[ee].vtarr.erase(pos);
    hyperedge[ee].vharr.push_back(vj);

    vertex[vj].indegree++;
    pos = find(vertex[vj].tedge.begin(),vertex[vj].tedge.end(),ee);
    if (pos != vertex[vj].tedge.end()) 
    vertex[vj].tedge.erase(pos);
    vertex[vj].hedge.push_back(ee);
}

void reachout(int vi,int k)
{
    queue<int>q;
    for(int i=0;i<(int)vertex[vi].hedge.size();i++)
    {
        int ee=vertex[vi].hedge[i];
        for(int j=0;j<(int)hyperedge[ee].vtarr.size();j++)
        {
            int vv=hyperedge[ee].vtarr[j];
            if(fh[vv])
            continue;
            if(!dk[vv]&&(vertex[vi].indegree-vertex[vv].indegree)>=1)
            {
                reverse_edge(vi,vv,ee);
                if(vertex[vv].indegree>=k) 
                dk[vv]=true;
                q.push(vv);
                fh[vv]=true;
                i--; 
                break;
            }
        }
    }
    while(!q.empty())
    { 
        reachout(q.front(),k); 
        q.pop(); 
    }
}

void reachin(int vi,int k)
{
    queue<int>q;
    for(int i=0;i<(int)vertex[vi].tedge.size();i++)
    {
        int ee=vertex[vi].tedge[i];
        for(int j=0;j<(int)hyperedge[ee].vharr.size();j++)
        {
            int vv=hyperedge[ee].vharr[j];
            if(fh[vv])
            continue;
            if(!dk[vv]&&(vertex[vv].indegree-vertex[vi].indegree)>=1)
            {
                reverse_edge(vv,vi,ee);
                q.push(vv);
                fh[vv]=true;
                i--; 
                break;
            }
        }
    }
    while(!q.empty())
    { 
        reachin(q.front(),k); 
        q.pop(); 
    }
}

void outk(int vi,int k)
{
    int initd=vertex[vi].indegree;
    for(int i=0;i<(int)vertex[vi].hedge.size();i++)
    {
        int ee=vertex[vi].hedge[i];
        for(int j=0;j<(int)hyperedge[ee].vtarr.size();j++)
        {
            int vv=hyperedge[ee].vtarr[j];
            if(dk[vv]&&(vertex[vi].indegree-vertex[vv].indegree)>=2)
            {
                reverse_edge(vi,vv,ee); 
                i--; 
                break;
            }
        }
    }
    for(int i=0;i<(int)vertex[vi].tedge.size();i++)
    {
        int ee=vertex[vi].tedge[i];
        for(int j=0;j<(int)hyperedge[ee].vharr.size();j++)
        {
            int vv=hyperedge[ee].vharr[j];
            if(dk[vv]&&(vertex[vv].indegree-vertex[vi].indegree)>=2)
            {
                reverse_edge(vv,vi,ee); 
                i--; 
                break;
            }
        }
    }
    if(initd>vertex[vi].indegree)
    {
        fh.clear();
        reachin(vi,k); 
        if(vertex[vi].indegree<k) 
        dk[vi]=false;
    }
    else if(initd<vertex[vi].indegree)
    {
        fh.clear();
        reachout(vi,k); 
        if(vertex[vi].indegree<k) 
        dk[vi]=false;
    }
    else
    {
        dk[vi]=false;
    }
}

void peelk(int k)
{
    while(1)
    {
        bool flag=false;
        for(int i=1;i<=nodenum;i++)
        {
            if(dk[i])
            {
                fh.clear();
                reachout(i,k);
            }
        }
        if(!flag)
        break;
    }
    while(true)
    {
        bool flag=false;
        for(int i=1;i<=nodenum;i++)
        {
            if(dk[i]&&vertex[i].indegree<k)
            { 
                flag=true; 
                outk(i,k); 
            }
        }
        if(!flag) 
        break;
    }
}

int getlayer(int delta)
{
    memset(dk,true,sizeof(dk));
    int k;
    for(k=1;k<maxn;k++)
    {
        if(!dkv[k-1].size())
        break;
        peelk(k);
        finddk(k);
    }
    cout<<"delta="<<delta<<"\n";
    for(int i=1;i<k;i++)
    {
        sort(dkv[i].begin(),dkv[i].end());
        sort(dkv[i+1].begin(),dkv[i+1].end());
        set_difference(dkv[i].begin(), dkv[i].end(), dkv[i+1].begin(), dkv[i+1].end(),back_inserter(layer[i]));
        for(int x: layer[i]) 
        idn[x]=i;
    }
    cout<<"layer["<<k-2<<"]="<<layer[k-2].size()<<"\n";
    cout<<"layer["<<k-1<<"]="<<layer[k-1].size()<<"\n";
    return k;
}

void hypergraphdecomposition(int delta)
{
    orientation(delta);
    for(int i=1;i<=nodenum;i++)
    {
        if(vertex[i].flag) 
        dkv[0].push_back(i);
    }
    (void)getlayer(delta);
}

// ===== 读批次列表 =====
vector<string> read_insert_list(const string& list_path)
{
    ifstream fin(list_path);
    if(!fin)
    { 
        cerr<<"[ERR] cannot open "<<list_path<<"\n"; 
        exit(1); 
    }
    vector<string> files; 
    string line;
    while(getline(fin, line))
    {
        string L = trim(line);
        if(L.empty()) 
        continue;
        if(L[0]=='#' || L[0]==';') 
        continue;
        files.push_back(L);
    }
    cerr<<"[INFO] batches="<<files.size()<<"\n";
    return files;
}

// ===== 主流程：初始 + 前缀批次 依次重构 + 分解 + 导出 =====
int main()
{
    // 配置
    //const string base_file = "dataset/house-bills.txt";
    const string base_file = "dataset/senate-bills.txt";
    //const string base_file = "dataset/test.txt";
    
    //const string base_file = "dataset/trivago.txt";
    
    //const string base_file = "dataset/amazon.txt";
    //const string base_file = "dataset/stackoverflow.txt";
    const string list_file = "dataset/insert_list.txt";
    //ifstream rda("dataset/hc2.txt");
    //ifstream rda("dataset/walmart2.txt");
    //ifstream rda("dataset/trivago2.txt");
    //ifstream rda("dataset/senate-bills2.txt");
    //ifstream rda("dataset/house-bills.txt");
    //ifstream rda("dataset/stackoverflow.txt");
    //ifstream rda("dataset/amazon.txt");
    //ifstream rda("dataset/house-committees2.txt");
    //ifstream rda("dataset/senate-committees2.txt");
    //ifstream rda("dataset/contact-high2.txt");
    //ifstream rda("dataset/contact-primary2.txt");
    //ifstream rda("dataset/mathoverflow2.txt");
    //ifstream rda("dataset/wiki_topcats.txt");
    //ifstream rda("dataset/gptgene3.txt");
    //ifstream rda("dataset/aml.txt");
    //ifstream rda("dataset/tmathoverflow3.txt");
    //ifstream rda("dataset/test.txt");
    const int delta = 9;

    // 读取批次文件名
    vector<string> batches = read_insert_list(list_file);

    // 逐批：用 (base + 前缀[0..i]) 重新构图与分解
    for(size_t bi=0; bi<batches.size(); ++bi)
    {
        cout << "==== [REBUILD BATCH " << (bi+1) << "/" << batches.size()
             << "] files: base + insert[0.." << bi << "] ====\n";

        // 1) 清空
        reset_state();

        // 2) 读入：初始 + 当前前缀批次
        vector<string> files; files.reserve(1+bi+1);
        files.push_back(base_file);
        for(size_t j=0;j<=bi;++j) files.push_back(batches[j]);
        read_all_files(files);
        cout << "[INFO] graph built: V<= "<<nodenum<<", E="<<edgenum<<"\n";

        // 3) 全量重分解
        auto t0 = chrono::high_resolution_clock::now();
        hypergraphdecomposition(delta);
        auto t1 = chrono::high_resolution_clock::now();
        double secs = chrono::duration<double>(t1-t0).count();
        cout << "[INFO] decomposed in "<<fixed<<setprecision(6)<<secs<<" s\n";

        // 4) 导出本批次后的 idn
        ostringstream outname;
        outname << "result_fullrecompute_b" << (bi+1) << ".idn.txt";
        dump_idn_all(outname.str(), nodenum, idn);
        cout << "[OK] wrote "<< outname.str() << "\n";
    }

    cout << "[DONE]\n";
    return 0;
}