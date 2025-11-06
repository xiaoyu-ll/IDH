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

int reversiblev(int vi,int k)
{
    queue<int>q;
    queue<int>qe;
    for(int i=0;i<vertex[vi].tedge.size();i++)
    {
        int ee=vertex[vi].tedge[i];
        for(int j=0;j<hyperedge[ee].vharr.size();j++)
        {
            int vv=hyperedge[ee].vharr[j];
            if(fh[vv]||vertex[vv].indegree<=k-2)
            continue;
            if(vertex[vv].indegree==k)
            {
                reverse(vv,vi,ee);
                return vv;
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
        int vt=reversiblev(q.front(),k);
        if(vt>0)
        {
            reverse(q.front(),vi,qe.front());
            return vt;
        }
        q.pop();
        qe.pop();
    } 
    return 0;
}

vector<int> reachv(int vi, int k)
{
    queue<int>q;
    vector<int>vecv;
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
                vecv.push_back(vv);
                q.push(vv);
            }
            fh[vv]=true;
        }
    }
    while(!q.empty())
    {
        vector<int>vtemp=reachv(q.front(),k);
        vecv.insert(vecv.end(),vtemp.begin(),vtemp.end());
        q.pop();
    } 
    return vecv;
}

void del_one_file(const string& filepath)
{
    ifstream rdaa(filepath);
    if(!rdaa){
        cout << "[ERR] cannot open delete file: " << filepath << endl;
        exit(1);
    }
    string strline;
    long long delcnt = 0;

    while(rdaa >> strline)
    {
        // 解析首个整数（edge_id），逗号后内容忽略
        int ps=0, pt=0, i=0, dele=-1;
        while(i < (int)strline.size())
        {
            if(strline[i]==',')
            {
                pt=i;
                string temps = strline.substr(ps, pt-ps);
                dele = stoi(temps);
                break;
            }
            i++;
        }
        if(dele==-1)
        {
            // 整行没有逗号：整行就是 edge_id
            dele = stoi(strline);
        }

        // === 以下与原 del() 相同 ===
        std::vector<int>::iterator pos;
        vector<int> vq;

        // 从 vtarr（出端）移除与 dele 的关系
        for(int j=0; j<(int)hyperedge[dele].vtarr.size(); j++)
        {
            int vv = hyperedge[dele].vtarr[j];
            hyperedge[dele].vtarr.erase(hyperedge[dele].vtarr.begin()+j);
            j--;
            pos = find(vertex[vv].tedge.begin(), vertex[vv].tedge.end(), dele);
            if (pos != vertex[vv].tedge.end())
            {
                vertex[vv].tedge.erase(pos);
            }
        }

        // 从 vharr（入端）移除与 dele 的关系，同时记录受影响的入端顶点集合 vq
        for(int j=0; j<(int)hyperedge[dele].vharr.size(); j++)
        {
            int vv = hyperedge[dele].vharr[j];
            vertex[vv].indegree--;
            vq.push_back(vv);
            hyperedge[dele].vharr.erase(hyperedge[dele].vharr.begin()+j);
            j--;
            pos = find(vertex[vv].hedge.begin(), vertex[vv].hedge.end(), dele);
            if (pos != vertex[vv].hedge.end())
            {
                vertex[vv].hedge.erase(pos);
            }
        }

        // 级联更新 idn（与你原来的逻辑保持一致）
        
        for(int j=0; j<(int)vq.size(); j++)
        {
            vector<int>vupd;
            int vv = vq[j];
            if(vertex[vv].indegree == idn[vv]-2)
            {
                fh.clear();
                int tt = reversiblev(vv, idn[vv]);
                fh.clear();
                vector<int> vt = reachv(tt, idn[vv]);
                vt.push_back(tt);
                vupd.insert(vupd.end(),vt.begin(),vt.end());
            }
            fh.clear();
            vector<int> vecq = reachv(vv, idn[vv]);
            vecq.push_back(vv);
            vupd.insert(vupd.end(),vecq.begin(),vecq.end());
            sort(vupd.begin(), vupd.end());  // 先排序
        vupd.erase(unique(vupd.begin(), vupd.end()), vupd.end());  // 再去重
        for(int l=0;l<vupd.size();l++)
        {
            if(!reachdk(vupd[l],idn[vupd[l]]))
            {
                idn[vupd[l]]=idn[vupd[l]]-1;
            }
        }
        }
        

        ++delcnt;
    }

    rdaa.close();
    cout << "[OK] deleted from " << filepath << ", lines=" << delcnt << endl;
}

static inline string trim(const string& s)
{
    size_t i=0,j=s.size();
    while(i<j && isspace((unsigned char)s[i])) 
    ++i;
    while(j>i && isspace((unsigned char)s[j-1])) 
    --j;
    return s.substr(i,j-i);
}

vector<string> read_delete_list(const string& list_path)
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
        string L = trim(line);
        if(L.empty()) continue;
        if(L[0]=='#' || L[0]==';') continue; // 支持注释
        files.push_back(L);
    }
    cerr << "[INFO] delete batches: " << files.size() << "\n";
    return files;
}

// 导出所有顶点当前层号 idn：每行 "vertex_id,layer_id"
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
    int dmax=readedge();
    cout<<"deltamax="<<dmax<<endl;
    
    
    
    hypergraphdecomposition(9);
    

    // 读取“删除批次列表”，例如 dataset/delete_list.txt
    vector<string> del_batches = read_delete_list("dataset/delete_list.txt");

    // 逐批删除，并在每批之后导出一次 idn
    for(size_t bi=0; bi<del_batches.size(); ++bi)
    {
        start_time = clock();

        cout << "==== [DEL BATCH " << (bi+1) << "/" << del_batches.size()<< "] file=" << del_batches[bi] << " ====" << endl;
        del_one_file(del_batches[bi]);

        end_time = clock();     //获取结束时间
        double Times = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    cout<<Times<<"seconds"<<endl;
    
        // 导出本批次后的 idn
        ostringstream fname;
        fname << "result_after_del_b" << (bi+1) << ".idn.txt";
        dump_idn_all(fname.str(), nodenum, idn);
        cout << "[OK] wrote " << fname.str() << "\n";
    }

    //reorientation();
    

    
    
    //std::this_thread::sleep_for(std::chrono::seconds(300)); // 暂停3秒
    return 0;

}
