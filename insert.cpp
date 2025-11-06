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
#define maxn 25000 //hc
using namespace std;
const int vm=16000000;
const int em=6000000;

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

bool reversiblev(int vi,int k)
{
    queue<int>q;
    queue<int>qe;
    for(int i=0;i<vertex[vi].hedge.size();i++)
    {
        int ee=vertex[vi].hedge[i];
        for(int j=0;j<hyperedge[ee].vtarr.size();j++)
        {
            int vv=hyperedge[ee].vtarr[j];
            if(fh[vv]||vertex[vv].indegree>k)
            continue;
            if(vertex[vv].indegree==k-1)
            {
                reverse(vi,vv,ee);
                return true;
            }
            else
            {
                q.push(vv);
                qe.push(ee);
            }
            fh[vv]=true;
        }
    }
    while(!q.empty())
    {
        if(reversiblev(q.front(),k))
        {
            reverse(vi,q.front(),qe.front());
            return true;
        }
        q.pop();
        qe.pop();
    } 
    return false;
}

void reachv(int vi, int k)
{
    queue<int>q;
    for(int i=0;i<vertex[vi].hedge.size();i++)
    {
        int ee=vertex[vi].hedge[i];
        for(int j=0;j<hyperedge[ee].vtarr.size();j++)
        {
            int vv=hyperedge[ee].vtarr[j];
            if(fh[vv]||vertex[vv].indegree>k)
            continue;
            if(vertex[vv].indegree==k)
            {
                idn[vv]=k+1;
                q.push(vv);
            }
            fh[vv]=true;
        }
    }
    while(!q.empty())
    {
        reachv(q.front(),k);
        q.pop();
    } 
}

void insert_one_file(const string& filepath, int delta)
{
    ifstream rdaa(filepath);
    if(!rdaa){
        cerr << "[ERR] cannot open insert file: " << filepath << "\n";
        exit(1);
    }
    string strline;
    int ei = edgenum; // 从当前边数继续追加
    long long added_edges = 0;

    while(rdaa >> strline)
    {
        int ps=0, pt=0, i=0, tempv;
        hyperedge[ei].varr.clear();
        hyperedge[ei].vharr.clear();
        hyperedge[ei].vtarr.clear();

        // 解析一条新边
        while(i < (int)strline.size())
        {
            if(strline[i]==','){
                pt=i;
                string temps = strline.substr(ps, pt-ps);
                ps=i+1;
                tempv = stoi(temps);
                hyperedge[ei].varr.push_back(tempv);
                if(tempv > nodenum) nodenum = tempv;
            }
            i++;
        }
        // 最后一个顶点
        if(ps < (int)strline.size())
        {
            string temps = strline.substr(ps, i-ps);
            tempv = stoi(temps);
            hyperedge[ei].varr.push_back(tempv);
            if(tempv > nodenum) nodenum = tempv;
        }
        hyperedge[ei].v = (int)hyperedge[ei].varr.size();

        // 按 idn 升序（让高层顶点靠后）
        sort(hyperedge[ei].varr.begin(), hyperedge[ei].varr.end(),
             [&](int a, int b){ return vertex[a].indegree < vertex[b].indegree; });

        // orientation
        int j=0;
        for(j=0; j<delta && j<hyperedge[ei].v; j++){
            int vv = hyperedge[ei].varr[j];
            hyperedge[ei].vharr.push_back(vv);
            vertex[vv].indegree++;
            vertex[vv].hedge.push_back(ei);   //
        }
        for(; j<hyperedge[ei].v; j++){
            int vv = hyperedge[ei].varr[j];
            hyperedge[ei].vtarr.push_back(vv);
            vertex[vv].tedge.push_back(ei);
        }
        


        // 受影响节点的k层位移处理
        for(int t=0; t<(int)hyperedge[ei].vharr.size(); ++t)
        {
            int vv = hyperedge[ei].vharr[t];
            if(vertex[vv].indegree == idn[vv]+1)
            {
                fh.clear();
                if(!reversiblev(vv, idn[vv]))
                {
                    fh.clear();
                    reachv(vv, idn[vv]);
                    idn[vv] = idn[vv] + 1;
                }
            }
        }

        ++ei;
        ++added_edges;
    }

    edgenum = ei; // 更新全局边数
    rdaa.close();
    cout << "[OK] inserted from " << filepath << ", added_edges=" << added_edges
         << ", edgenum_now=" << edgenum << "\n";
}

vector<string> read_insert_list(const string& list_path)
{
    ifstream fin(list_path);
    if(!fin)
    {
        cerr << "[ERR] cannot open batch list: " << list_path << "\n";
        exit(1);
    }
    vector<string> files;
    string line;
    while(getline(fin, line))
    {
        // 去掉空行和注释
        auto L = line;
        // 简单trim
        size_t i=0,j=L.size();
        while(i<j && isspace((unsigned char)L[i])) ++i;
        while(j>i && isspace((unsigned char)L[j-1])) --j;
        L = L.substr(i, j-i);
        if(L.empty()) continue;
        if(L.size()>=1 && (L[0]=='#' || L[0]==';')) 
        continue;
        files.push_back(L);
    }
    cerr << "[INFO] insert batches: " << files.size() << "\n";
    return files;
}

// 导出所有顶点的当前层号 idn：每行 "vertex_id,layer_id"
void dump_idn_all(const string& path, int nodenum, const int idn[])
{
    ofstream fout(path);
    if(!fout)
    {
        cerr << "[ERR] cannot write " << path << "\n";
        return;
    }
    for(int v=1; v<=nodenum; ++v)
    {
        fout << v << "," << idn[v] << "\n";
    }
}

int main()
{
    clock_t start_time, end_time;
    int dmax = readedge();
    cout << "deltamax=" << dmax << endl;

    

    int delta = 9; // 你当前用的 delta
    hypergraphdecomposition(delta);


    start_time = clock();


    // 读取批次列表（每行一个文件路径）
    vector<string> batches = read_insert_list("dataset/insert_list.txt");

    // 逐批次插入
    for(size_t bi=0; bi<batches.size(); ++bi)
    {
        cout << "==== [BATCH " << (bi+1) << "/" << batches.size()
         << "] file=" << batches[bi] << " ====" << endl;
        insert_one_file(batches[bi], delta);


        end_time = clock();
    double Times = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    cout << Times << " seconds" << endl;
    
        // 插完本批，立刻导出本批次后的 idn 结果
        ostringstream fname;
        fname << "result_after_b" << (bi+1) << ".idn.txt";  
        dump_idn_all(fname.str(), nodenum, idn);
        cout << "[OK] wrote " << fname.str() << "\n";
       // for(int i=1; i<=nodenum; i++)
       // {
         //   cout << "idn[" << i << "]=" << idn[i] << "\n";
       // }
       // cout << endl;
    } 
    return 0;
}
