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

//#define maxn 2450000 //hb
#define maxn 40000 //hc
//#define maxn 40000 //ch
//#define maxn 450000 //sb
//#define maxn 30000 //sc
//#define maxn 90000 //cp
using namespace std;
const int vm=1300;
const int em=350;
const int INF=0x3f3f3f3f;
const int src=0,des=1298;

struct Vertex //1791488
{
    int id;
    int degree;
    int indegree;
    bool flag;
    vector<int> tedge;//给该顶点分配出度的边集合，//结点指向边
    vector<int> hedge;//给该顶点分配入度的边集合，//边指向结点
    vector<int> edge;//给该顶点分配入度的边集合，//边指向结点
};
struct Hyperedge//1735400 
{
    int id;
    bool flag;
    vector<int> varr;//这条超边所有的顶点
    vector<int> initvarr;//这条超边所有的顶点
    vector<int> vtarr;//分配出度的顶点，//结点指向边,多的那个在t集
    vector<int> vharr;//分配入度的顶点，//边指向结点
    int v;//该超边的度（包含的顶点个数）
    int h;//改超边的出度，超边所有的具有入度的顶点个数
};
struct Edge
{
	int u,v,next;
	int cap;
    int flow;
    vector<int>e;
}edge[maxn];

Vertex vertex[vm];
Hyperedge hyperedge[em];
vector<int> dkv[maxn];
int edgenum=0;
int nodenum=0;

map<int,int>frontv;
map<int,int>fronte;
map<int,bool>fh;

int cnt,head[vm+em];
int dep[vm+em];
int edgeflag=0;

int dmax=0;



int readedge()//读数据
{
    //ifstream rda("dataset/senate-bills.txt");
    //ifstream rda("dataset/house-bills.txt");
    //ifstream rda("dataset/stackoverflow.txt");
    //ifstream rda("dataset/amazon.txt");
    ifstream rda("dataset/house-committees.txt");
    //ifstream rda("dataset/senate-committees.txt");
    //ifstream rda("dataset/contact-high.txt");
    //ifstream rda("dataset/contact-primary.txt");
    //ifstream rda("dataset/mathoverflow.txt");
    //ifstream rda("dataset/wiki_topcats.txt");
    //ifstream rda("dataset/test.txt");
    //ifstream rda("dataset/tmathoverflow.txt");
    //ifstream rda("dataset/email_eu.txt");
    if(!rda)
    {
        cout<<"error!"<<endl;
        exit(1);
    }
    string strline;
    int ei=0;
    int deltamax=0;
    int cou=0;
    while(rda>>strline)//读取每一条超边并把顶点分隔开来
    {
        int ps=0,pt=0,i=0,tempv;
        vector<int> temparr;
        while(i<strline.size())
        {
            if(strline[i]==',')
            {
                pt=i;
                string temps=strline.substr(ps,pt-ps);
                tempv=stoi(temps);
                hyperedge[ei].varr.push_back(tempv);
                hyperedge[ei].initvarr.push_back(tempv);
                ps=i+1;
                if(tempv>nodenum)
                nodenum=tempv;
                vertex[tempv].degree++;
                if(vertex[tempv].degree>dmax)
                dmax=vertex[tempv].degree;
                vertex[tempv].flag=false;
                vertex[tempv].edge.push_back(ei);
            }
            i++;
        }
        string temps=strline.substr(ps,i-ps);
        tempv=stoi(temps);
        hyperedge[ei].varr.push_back(tempv);
        hyperedge[ei].initvarr.push_back(tempv);
        hyperedge[ei].v=hyperedge[ei].varr.size();
        hyperedge[ei].flag=false;
        if(hyperedge[ei].varr.size()>deltamax)
        deltamax=hyperedge[ei].varr.size();
        if(tempv>nodenum)
        nodenum=tempv;
        vertex[tempv].degree++;
        if(vertex[tempv].degree>dmax)
        dmax=vertex[tempv].degree;
        vertex[tempv].flag=false;
        vertex[tempv].edge.push_back(ei);
        ei++;
    }
    edgenum=ei;
    cout<<"read edge successful!"<<endl;
    rda.close();
    return deltamax;
}

void orientation(int delta)
{
    for(int i=0;i<edgenum;i++)
    {
        int j=0;
        for(j=0;j<delta&&j<hyperedge[i].varr.size();j++)//为每条边的前一半顶点分配出度，起始顶点集
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
            if(!dk[vv]&&(vertex[vi].indegree-vertex[vv].indegree)>=2)
            {
                reverse(vi,vv,ee);
                if(vertex[vv].indegree>=k)
                {
                    dk[vv]=true;
                }
                q.push(vv);
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
            if(!dk[vv]&&(vertex[vv].indegree-vertex[vi].indegree)>=2)
            {
                reverse(vv,vi,ee);
                q.push(vv);
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
        reachin(vi,k);
        if(vertex[vi].indegree<k)
        dk[vi]=false;
    }
    else if(initd<vertex[vi].indegree)
    {
        reachout(vi,k);
        if(vertex[vi].indegree<k)
        dk[vi]=false;
    }
    else
    {
        dk[vi]=false;
    } 
}

void peelk(int k,int l)
{
    for(int i=0;i<dkv[l].size();i++)
    {
        int vv=dkv[l][i];
        if(dk[vv])
        {
            reachout(vv,k);
        }
    }
    while(1)
    {
        bool flag=false;
        for(int i=0;i<dkv[l].size();i++)
        {
            int vv=dkv[l][i];
            if(dk[vv]&&vertex[vv].indegree<k)
            {
                flag=true;
                outk(vv,k);
            }
        }
        if(!flag)
        break;
    }
}
vector<int> getlayer(int kk, int u, int l)
{
    vector<int>difference;
    sort(dkv[l].begin(), dkv[l].end());
    sort(dkv[u].begin(), dkv[u].end());
    set_difference(dkv[l].begin(),dkv[l].end(),dkv[u].begin(),dkv[u].end(),inserter(difference,difference.begin()));
    memset(dk,false,sizeof(dk));
    for(int i=0;i<difference.size();i++)
    {
        int vv=difference[i];
        dk[vv]=true;
    }
    while (1)
    {
        bool flag=false;
        for(int i=0;i<difference.size();i++)
        {
            int vv=difference[i];
            if(dk[vv]&&vertex[vv].indegree<kk)
            {
                flag=true;
                outk(vv,kk);
            }
        }
        if(!flag)
        break;
    }
    vector<int>veck;
    for(int i=0;i<difference.size();i++)
    {
        int vv=difference[i];
        if(dk[vv])
        {
            veck.push_back(vv);
        }
        fh.clear();
        if(!dk[vv]&&vertex[vv].indegree==kk-1&&reachdk(vv,kk))
        {
            veck.push_back(vv);
        }
    }
    veck.insert(veck.end(),dkv[u].begin(),dkv[u].end());
    return veck;
}

bool searchp(int xp)
{
    if(dkv[xp].size()!=0)
    return true;
    memset(dk,false,sizeof(dk));
    for(int i=1;i<=nodenum;i++)
    {
        if(vertex[i].indegree>=xp)
        dk[i]=true;
    }
    for(int i=1;i<=nodenum;i++)
    {
        if(dk[i])
        {
            reachout(i,xp);
        }
    }
    while(1)
    {
        bool flag=false;
        for(int i=1;i<=nodenum;i++)
        {
            if(dk[i]&&vertex[i].indegree<xp)
            {
                flag=true;
                outk(i,xp);
            }
        }
        if(!flag)
        break;
    }
    finddk(xp);
    if(dkv[xp].empty())
    return false;
    return true;
}


int computep(int maxk)
{
    if(searchp(maxk))
    return maxk;
    int qul=1,qur=maxk;
    while(1)
    {
        maxk/=2;
        if(searchp(maxk))
        {
            return maxk;
        }
    }
    return qur;
}

void divid(int u, int l)
{
    vector<int>::iterator it;
    int kk;
    if(u==l||u==l+1)
    {
        return;
    }
    if(dkv[u].size()==dkv[l].size())
    {
        if(dkv[u].size()==0)
        return;
        for(int j=l+1;j<u;j++)
        dkv[j]=dkv[u];
        return;
    }
    kk=(u+l+1)/2;
    if(dkv[kk].size()==0)
    dkv[kk]=getlayer(kk,u,l);
    divid(u,kk);
    divid(kk,l);
    
}

void dividsearch(int u,int l)
{
    if(u==l||u==l+1)
    {
        return;
    }
    if(dkv[u].size()==dkv[l].size())
    {
        if(dkv[u].size()==0)
        return;
        for(int j=l+1;j<u;j++)
        dkv[j]=dkv[u];
        return;
    }
    vector<int>difference;
    sort(dkv[l].begin(), dkv[l].end());
    sort(dkv[u].begin(), dkv[u].end());
    set_difference(dkv[l].begin(),dkv[l].end(),dkv[u].begin(),dkv[u].end(),inserter(difference,difference.begin()));
    int mid=(u+l+1)/2;
    memset(dk,false,sizeof(dk));
    for(int i=0;i<dkv[l].size();i++)
    {
        int vv=dkv[l][i];
        if(vertex[vv].indegree>=mid)
        dk[vv]=true;
    }
    peelk(mid,l);
    vector<int>veck;
    for(int i=0;i<dkv[l].size();i++)
    {
        int vv=dkv[l][i];
        if(dk[vv])
        {
            veck.push_back(vv);
        }
        fh.clear();
        if(!dk[vv]&&vertex[vv].indegree==mid-1&&reachdk(vv,mid))
        {
            veck.push_back(vv);
        }
    }
    dkv[mid].clear();
    dkv[mid].insert(dkv[mid].end(),veck.begin(),veck.end());
    if(dkv[mid].size()==0)
    {
        dividsearch(mid,l);
    }
    else
    {
        divid(mid,l);
        dividsearch(u,mid);
    }
}



void hypergraphdecomposition()
{
    int total_layers=0;
    int maxk=dmax;
    for(int i=1;i<=nodenum;i++)
    dkv[1].push_back(i);
    for(int i=1;i<=deltamax;i++)
    {
        for(int j=1;j<=nodenum;j++)
        {
            vertex[j].indegree=0;
            vertex[j].tedge.clear();
            vertex[j].hedge.clear();
        }
        for(int j=2;j<=maxk;j++)
        dkv[j].clear();
        for(int j=0;j<edgenum;j++)
        {
            hyperedge[j].vtarr.clear();
            hyperedge[j].vharr.clear();
        }
        orientation(i);
        int maxk=dmax;
        int p=computep(dmax);
        if(dmax>=2*p)
        maxk=2*p;
        dividsearch(maxk,p);
        divid(p,1);
        cout<<"**********************"<<endl;
        cout<<"delta="<<i<<endl;
        for(int i=1;i<=dmax;i++)
        {
            if(dkv[i].size()==0)
            break;
            cout<<"dkv["<<i<<"]:"<<dkv[i].size()<<endl;
        }
        cout<<"**********************"<<endl;
        cout<<endl;
    }
    cout<<"total_layers="<<total_layers<<endl;
}
int main()
{
    clock_t start_time, end_time;
    readedge();
    
    
    start_time = clock();
    hypergraphdecomposition();
    
    //reorientation();
    

    end_time = clock();     //获取结束时间
    double Times = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    cout<<Times<<"seconds"<<endl;
    std::this_thread::sleep_for(std::chrono::seconds(300)); // 暂停3秒
    return 0;

}
