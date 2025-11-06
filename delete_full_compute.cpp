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

//#define maxn 1800000 //tc
#define maxn 2500 //hc
using namespace std;
const int vm=173000;
const int em=234000;

struct Vertex //1791488
{
    int id;
    int degree;
    int indegree;
    vector<int> tedge;//给该顶点分配出度的边集合，//结点指向边
    vector<int> hedge;//给该顶点分配入度的边集合，//边指向结点
    bool flag;
};
struct Hyperedge//1735400 
{
    int id;
    vector<int> varr;//这条超边所有的顶点
    vector<int> vtarr;//分配出度的顶点，//结点指向边,多的那个在t集
    vector<int> vharr;//分配入度的顶点，//边指向结点
    int v;//该超边的度（包含的顶点个数）
    int h;//改超边的出度，超边所有的具有入度的顶点个数
};

Vertex vertex[vm];
Hyperedge hyperedge[em];
vector<int> dkv[maxn];
vector<int> layer[maxn];
int edgenum=0;
int nodenum=0;
bool dk[vm];
int idn[vm];

map<int,bool>fh;






int readedge()//读数据
{
    ifstream rda("dataset/senate-bills.txt");
    if(!rda)
    {
        cout<<"error!"<<endl;
        exit(1);
    }
    string strline;
    int ei=0;
    int dmax=0;
    while(rda>>strline)//读取每一条超边并把顶点分隔开来
    {
        int ps=0,pt=0,i=0,tempv;
        vector<int> temparr;
        int cou=0;
        while(i<strline.size())
        {
            if(strline[i]==',')
            {
                cou++;
                pt=i;
                string temps=strline.substr(ps,pt-ps);
                ps=i+1;
                //if(cou==1)
                //continue;
                tempv=stoi(temps);
                hyperedge[ei].varr.push_back(tempv);
                vertex[tempv].flag=true;
                if(tempv>nodenum)
                nodenum=tempv;
                vertex[tempv].degree++;
            }
            i++;
        }
        string temps=strline.substr(ps,i-ps);
        tempv=stoi(temps);
        hyperedge[ei].varr.push_back(tempv);
        vertex[tempv].flag=true;
        hyperedge[ei].v=hyperedge[ei].varr.size();
        if(hyperedge[ei].varr.size()>dmax)
        dmax=hyperedge[ei].varr.size();
        if(tempv>nodenum)
        nodenum=tempv;
        vertex[tempv].degree++;
        ei++;
    }
    edgenum=ei;
    cout<<"read edge successful!"<<endl;
    rda.close();
    return dmax;
}

void del_one_file(const string& filepath)
{
    ifstream fin(filepath);
    if(!fin)
    { 
        cerr<<"[ERR] cannot open delete file: "<<filepath<<"\n"; 
        exit(1); 
    }
    string line; long long cnt=0;
    while(fin >> line)
    {
        int dele=-1;
        // 取逗号前的第一个整数；若整行无逗号，则整行即 id
        size_t pos = line.find(',');
        try
        {
            if(pos==string::npos) 
            dele = stoi(line);
            else                  
            dele = stoi(line.substr(0,pos));
        }
        catch(...)
        { continue; }
        if(dele<0 || dele>=em) 
        continue;

        // 清空这条边（重构版只需使其“消失”即可）
        for(int j=0;j<hyperedge[dele].varr.size();j++)
        {
            int vv=hyperedge[dele].varr[j];
            vertex[vv].degree--;
            if(vertex[vv].degree==0)
            vertex[vv].flag=false;
        }
        hyperedge[dele].varr.clear();
        hyperedge[dele].vtarr.clear();
        hyperedge[dele].vharr.clear();
        hyperedge[dele].v = 0;
        ++cnt;
    }
    cout << "[OK] delete file applied: " << filepath << ", lines=" << cnt << "\n";
}

void orientation(int delta)
{
    for(int i=0;i<edgenum;i++)
    {
        int j=0;
        for(j=0;j<delta&&j<hyperedge[i].v;j++)//为每条边的前一半顶点分配出度，起始顶点集
        {
            hyperedge[i].vharr.push_back(hyperedge[i].varr[j]);
            vertex[hyperedge[i].varr[j]].indegree++;
            vertex[hyperedge[i].varr[j]].hedge.push_back(i);//边指向结点
        }
        for(j;j<hyperedge[i].v;j++)//为每条边的前一半顶点分配出度，起始顶点集
        {
            hyperedge[i].vtarr.push_back(hyperedge[i].varr[j]);
            vertex[hyperedge[i].varr[j]].tedge.push_back(i);//结点指向边
        }
    }
}

bool reachdk(int vi, int k)
{
    queue<int>q;
    for(int i=0;i<vertex[vi].tedge.size();i++)//遍历顶点vi的每条出边，结点指向边
    {
        for(int j=0;j<hyperedge[vertex[vi].tedge[i]].vharr.size();j++)//遍历这条出边的所有具有入度的顶点，边指向结点
        {
            int vv=hyperedge[vertex[vi].tedge[i]].vharr[j];
            if(vertex[vv].indegree>=k)
            {
                return true;
            }
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
        {
            return true;
            break;
        }
        q.pop();
    }
    return false;
}

void finddk(int k)
{
    //memset(dk,false,sizeof(dk));
    vector<int>veck;
    for(int i=1;i<=nodenum;i++)
    {
        if(dk[i])
        {
            veck.push_back(i);
        }
        fh.clear();
        if(!dk[i]&&vertex[i].indegree==k-1&&reachdk(i,k))
        {
            veck.push_back(i);
        }
    }
    dkv[k].clear();
    dkv[k].insert(dkv[k].end(),veck.begin(),veck.end());
}

void reverse(int vi, int vj,int ee)
{  
    std::vector<int>::iterator pos;
    //翻转vi到ee的入边,处理顶点
    vertex[vi].indegree--;  //1
    pos = find(vertex[vi].hedge.begin(),vertex[vi].hedge.end(),ee);
    if (pos != vertex[vi].hedge.end()) //0
    {
        vertex[vi].hedge.erase(pos);
    }
    vertex[vi].tedge.push_back(ee);//1
    //翻转vi到ee的入边,处理边   1
    pos = find(hyperedge[ee].vharr.begin(),hyperedge[ee].vharr.end(),vi);
    if (pos != hyperedge[ee].vharr.end()) 
    {
        hyperedge[ee].vharr.erase(pos);
    }
    hyperedge[ee].vtarr.push_back(vi);
    //翻转vj到ee的出边,处理边   1
    pos = find(hyperedge[ee].vtarr.begin(),hyperedge[ee].vtarr.end(),vj);
    if (pos != hyperedge[ee].vtarr.end()) 
    {
        hyperedge[ee].vtarr.erase(pos);
    }
    hyperedge[ee].vharr.push_back(vj);
    //翻转vj到ee的出边,处理顶点
    vertex[vj].indegree++;//1
    pos = find(vertex[vj].tedge.begin(),vertex[vj].tedge.end(),ee);
    if (pos != vertex[vj].tedge.end()) //1
    {
        vertex[vj].tedge.erase(pos);
    }
    vertex[vj].hedge.push_back(ee);//0
}

void reachout(int vi,int k)
{
    queue<int>q;
    for(int i=0;i<vertex[vi].hedge.size();i++)
    {
        int ee=vertex[vi].hedge[i];
        for(int j=0;j<hyperedge[ee].vtarr.size();j++)
        {
            int vv=hyperedge[ee].vtarr[j];
            if(fh[vv])
            continue;
            if(!dk[vv]&&(vertex[vi].indegree-vertex[vv].indegree)>=1)
            {
                reverse(vi,vv,ee);
                if(vertex[vv].indegree>=k)
                {
                    dk[vv]=true;
                }
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
    for(int i=0;i<vertex[vi].tedge.size();i++)
    {
        int ee=vertex[vi].tedge[i];
        for(int j=0;j<hyperedge[ee].vharr.size();j++)
        {
            int vv=hyperedge[ee].vharr[j];
            if(fh[vv])
            continue;
            if(!dk[vv]&&(vertex[vv].indegree-vertex[vi].indegree)>=1)
            {
                reverse(vv,vi,ee);
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
    for(int i=0;i<vertex[vi].hedge.size();i++)
    {
        int ee=vertex[vi].hedge[i];
        for(int j=0;j<hyperedge[ee].vtarr.size();j++)
        {
            int vv=hyperedge[ee].vtarr[j];
            if(dk[vv]&&(vertex[vi].indegree-vertex[vv].indegree)>=2)
            {
                reverse(vi,vv,ee);
                i--;
                break;
            }
        }
    }
    for(int i=0;i<vertex[vi].tedge.size();i++)
    {
        int ee=vertex[vi].tedge[i];
        for(int j=0;j<hyperedge[ee].vharr.size();j++)
        {
            int vv=hyperedge[ee].vharr[j];
            if(dk[vv]&&(vertex[vv].indegree-vertex[vi].indegree)>=2)
            {
                reverse(vv,vi,ee);
                i--;
                break;
            }
        }
    }
    if(initd>vertex[vi].indegree)//变小了
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
    while(1)
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
    for(k=2;k<maxn;k++)
    {
        if(!dkv[k-1].size())
        break;
        peelk(k);
        finddk(k);
    }
    //cout<<"**********************"<<endl;
    cout<<"delta="<<delta<<endl;
    ostringstream oss;
    //oss << "dataset/sb_d" << delta << ".txt";
    //ofstream out1("dataset/sb1_d20.txt");
    for(int i=1;i<k;i++)
    {
        //cout<<"dkv["<<i<<"]:"<<dkv[i].size()<<endl;
        sort(dkv[i].begin(),dkv[i].end());
        sort(dkv[i+1].begin(),dkv[i+1].end());
        set_difference(dkv[i].begin(), dkv[i].end(),dkv[i+1].begin(), dkv[i+1].end(),back_inserter(layer[i]));
        //cout<<"layer["<<i<<"]:"<<layer[i].size()<<endl;
        //out1<<i<<",";
        //for(int j=0;j<layer[i].size();j++)
        //out1<<layer[i][j]<<",";
        //out1<<endl;
        for(int j=0;j<layer[i].size();j++)
        idn[layer[i][j]]=i;
    }
    cout<<"layer["<<k-2<<"]:"<<layer[k-2].size()<<endl;
    cout<<"layer["<<k-1<<"]:"<<layer[k-1].size()<<endl;
    //cout<<"layer["<<k<<"]:"<<layer[k].size()<<endl;
    //cout<<"**********************"<<endl;
    //cout<<endl;
    return k;
}
void hypergraphdecomposition(int delta)
{
    int maxk=vm;
    orientation(delta);
    for(int i=1;i<=nodenum;i++)
    {
        if(vertex[i].flag)
        dkv[1].push_back(i);
    }
    maxk=getlayer(delta);
}

// 简单 trim
static inline string trim(const string& s)
{
    size_t i=0,j=s.size();
    while(i<j && isspace((unsigned char)s[i])) 
    ++i;
    while(j>i && isspace((unsigned char)s[j-1])) 
    --j;
    return s.substr(i,j-i);
}

// 导出所有顶点的当前层号 idn：每行 "vertex_id,layer_id"
void dump_idn_all(const string& path, int nodenum, const int idn[])
{
    ofstream fout(path);
    if(!fout)
    { 
        cerr<<"[ERR] cannot write "<<path<<"\n"; 
        return; 
    }
    for(int v=1; v<=nodenum; ++v) 
    fout << v << "," << idn[v] << "\n";
}

// 读取“删除批次列表”，每行一个文件路径（支持 # ; 注释行）
vector<string> read_delete_list(const string& list_path)
{
    ifstream fin(list_path);
    if(!fin)
    { 
        cerr << "[ERR] cannot open batch list: " << list_path << "\n"; 
        exit(1); 
    }
    vector<string> files; string line;
    while(getline(fin, line))
    {
        string L = trim(line);
        if(L.empty()) 
        continue;
        if(L[0]=='#' || L[0]==';') 
        continue;
        files.push_back(L);
    }
    cerr << "[INFO] delete batches: " << files.size() << "\n";
    return files;
}

// 每次重构前，清空全局状态
void reset_state()
{
    for(int i=0;i<vm;++i)
    {
        vertex[i].id=0; 
        vertex[i].degree=0; 
        vertex[i].indegree=0;
        vertex[i].tedge.clear(); 
        vertex[i].hedge.clear(); 
        vertex[i].flag=false;
        dk[i]=false; idn[i]=0;
    }
    for(int i=0;i<em;++i)
    {
        hyperedge[i].id=0; 
        hyperedge[i].varr.clear();
        hyperedge[i].vtarr.clear(); 
        hyperedge[i].vharr.clear();
        hyperedge[i].v=0; 
        hyperedge[i].h=0;
    }
    for(int i=0;i<maxn;++i)
    { 
        dkv[i].clear(); 
        layer[i].clear(); 
    }
    edgenum=0; 
    nodenum=0; 
    fh.clear();
}

int main()
{
    // 配置路径
    //const string base_file = "dataset/test.txt";  // 初始图
    const string list_file = "dataset/delete_list.txt";   // 批次列表
    const int delta = 9;                                  

    // 读取批次列表
    vector<string> del_batches = read_delete_list(list_file);
    if(del_batches.empty())
    {
        cerr << "[WARN] no delete batches; will just run once on base graph.\n";
    }

    // 逐批：用 (base + 前缀删除[0..i]) 重读→删边→重分解→导出
    for(size_t bi=0; bi<max<size_t>(1, del_batches.size()); ++bi)
    {
        cout << "==== [REBUILD DEL BATCH " << (bi+1)
             << "/" << (del_batches.empty()?1:del_batches.size())
             << "] ====\n";

        // 1) 清空状态
        reset_state();

        // 2) 重读初始图
        int dmax = readedge();  // 你原有的读图函数
        (void)dmax;

        // 3) 应用前缀删除 (如果有批次)
        if(!del_batches.empty())
        {
            for(size_t j=0; j<=bi; ++j)
            del_one_file(del_batches[j]);
        }

        // 4) 全量重分解
        auto t0 = chrono::high_resolution_clock::now();
        hypergraphdecomposition(delta);
        auto t1 = chrono::high_resolution_clock::now();
        double secs = chrono::duration<double>(t1-t0).count();
        cout << "[INFO] decomposed in "<< fixed << setprecision(6) << secs << " s\n";

        // 5) 导出当前批次的 idn
        ostringstream fname;
        fname << "result_fullrecompute_del_b" << (bi+1) << ".idn.txt";
        dump_idn_all(fname.str(), nodenum, idn);
        cout << "[OK] wrote " << fname.str() << "\n";
    }

    cout << "[DONE]\n";
    return 0;
}