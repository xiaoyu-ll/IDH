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
const int vm=1350;
const int em=350;
const int INF=0x3f3f3f3f;
const int src=0,des=1298;

struct Vertex //1791488
{
    int id;
    int degree;
    int indegree;
    vector<int> tedge;//给该顶点分配出度的边集合，//结点指向边
    vector<int> hedge;//给该顶点分配入度的边集合，//边指向结点
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
int deltamax=0;
map<int,int>frontv;
map<int,int>fronte;
map<int,bool>fh;

int cnt,head[vm+em];
int dep[vm+em];
int edgeflag=0;


void addedge(int cu,int cv,int cw)
{
	edge[cnt].u=cu;	
    edge[cnt].v=cv;	
    edge[cnt].cap=cw;  
    edge[cnt].flow=0; 
    edge[cnt].next=head[cu];
	head[cu]=cnt++;

	edge[cnt].u=cv;	
    edge[cnt].v=cu;	
    edge[cnt].cap=0;
    edge[cnt].flow=0; 	
    edge[cnt].next=head[cv];
	head[cv]=cnt++;
}

void readedge()//读数据
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
                ps=i+1;
                if(tempv>nodenum)
                nodenum=tempv;
                vertex[tempv].degree++;
            }
            i++;
        }
        string temps=strline.substr(ps,i-ps);
        tempv=stoi(temps);
        hyperedge[ei].varr.push_back(tempv);
        hyperedge[ei].v=hyperedge[ei].varr.size();
        if(hyperedge[ei].varr.size()>deltamax)
        deltamax=hyperedge[ei].varr.size();
        if(tempv>nodenum)
        nodenum=tempv;
        vertex[tempv].degree++;
        ei++;
    }
    edgenum=ei;
    cout<<"read edge successful!"<<endl;
    rda.close();
}
void orientation(int delta)
{
    for(int i=0;i<edgenum;i++)
    {
        hyperedge[i].h=1;//每条超边的终点集合的大小
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

int BFS()
{
	queue<int> q;
	while (!q.empty()) q.pop();
	memset(dep,-1,sizeof(dep));
	dep[src] = 0;
	q.push(src);
	while (!q.empty())
	{
		int u=q.front();
		q.pop();
		for (int i=head[u];i!=-1;i=edge[i].next)
		{
			int v=edge[i].v;
			if (edge[i].cap>edge[i].flow&& dep[v]==-1)
			{
				dep[v]=dep[u]+1;
				q.push(v);
			}
		}
	}
	if (dep[des]==-1) return 0;
	return 1;
}

int DFS(int u,int minflow)
{
	if (u==des)	
    return minflow;
	int aug;
	for (int i=head[u];i!=-1;i=edge[i].next)
	{
		int v=edge[i].v;
		if (edge[i].cap>edge[i].flow && dep[v]==dep[u]+1 && (aug=DFS(v, min(minflow,edge[i].cap-edge[i].flow))))
		{
			edge[i].flow+=aug;
			edge[i^1].flow-=aug;
			return aug;
		}
	}
	dep[u]=-1;
	return 0;
}

int Dinic()
{
	int maxflow = 0;
	while(BFS())
	{
		while(true)
		{
			int minflow = DFS(src,INF);
			if (minflow==0)	break;
			maxflow += minflow;
		}
	}
	return maxflow;
}

bool reachbfs(int vi, int vj, int co)
{
    queue<int>q;
    queue<int>q1;
    for(int i=0;i<vertex[vi].tedge.size();i++)//遍历顶点vi的每条出边，结点指向边
    {
        //pe.push_back(vertex[vi].tedge[i]);//把路径上的边依次存储下来
        for(int j=0;j<hyperedge[vertex[vi].tedge[i]].vharr.size();j++)//遍历这条出边的所有具有入度的顶点，边指向结点
        {
            //
            if(!fh[hyperedge[vertex[vi].tedge[i]].vharr[j]])
            {
                fh[hyperedge[vertex[vi].tedge[i]].vharr[j]]=true;
                q.push(hyperedge[vertex[vi].tedge[i]].vharr[j]);
                fronte[hyperedge[vertex[vi].tedge[i]].vharr[j]]=vertex[vi].tedge[i];
                frontv[hyperedge[vertex[vi].tedge[i]].vharr[j]]=vi;
            }
                //if(bfs(hyperedge[vertex[vi].tedge[i]].vharr[j],vj,co))
            //pv.pop_back();
        }
    }
    while(!q.empty())
    {
        if(q.front()==vj)
        {
            return true;
            break;
        }
        q1.push(q.front());
        q.pop();
    }
    while(!q1.empty())
    {
        if(reachbfs(q1.front(),vj,co++))
        {
            return true;
            break;
        }
        q1.pop();
    }
    return false;
}


void reversevte(int vi, int e)
{  
    std::vector<int>::iterator pos;
    //翻转vi到e的出边,处理边   1
    pos = find(hyperedge[e].vtarr.begin(),hyperedge[e].vtarr.end(),vi);
    if (pos != hyperedge[e].vtarr.end()) 
    {
        hyperedge[e].vtarr.erase(pos);
    }
    hyperedge[e].vharr.push_back(vi);
    //翻转vi到e的出边,处理顶点
    vertex[vi].indegree++;//1
    pos = find(vertex[vi].tedge.begin(),vertex[vi].tedge.end(),e);
    if (pos != vertex[vi].tedge.end()) //1
    {
        vertex[vi].tedge.erase(pos);
    }
    vertex[vi].hedge.push_back(e);//0
}

void reverseetv(int e, int vj)
{ 
    std::vector<int>::iterator pos;
    //翻转vj到e的入边,处理顶点
    vertex[vj].indegree--;  //1
    pos = find(vertex[vj].hedge.begin(),vertex[vj].hedge.end(),e);
    if (pos != vertex[vj].hedge.end()) //0
    {
        vertex[vj].hedge.erase(pos);
    }
    vertex[vj].tedge.push_back(e);//1
    //翻转vj到e的入边,处理边   1
    pos = find(hyperedge[e].vharr.begin(),hyperedge[e].vharr.end(),vj);
    if (pos != hyperedge[e].vharr.end()) 
    {
        hyperedge[e].vharr.erase(pos);
    }
    hyperedge[e].vtarr.push_back(vj);
}


void flow(int d)
{
    cnt=0;
    edgeflag=0;
    memset(head,-1,sizeof(head));
    memset(edge, 0, sizeof(edge));
    for(int i=0;i<edgenum;i++)
    {
        for(int j=0;j<hyperedge[i].vtarr.size();j++)
        {
            addedge(hyperedge[i].vtarr[j],vm+i,1);
            edgeflag+=2;
        }
        for(int j=0;j<hyperedge[i].vharr.size();j++)
        {
            addedge(vm+i,hyperedge[i].vharr[j],1);
            edgeflag+=2;
        }
    }
    for(int i=1;i<=nodenum;i++)
    {
        if(vertex[i].indegree<d)
        {
            addedge(src,i,d-vertex[i].indegree);
        }
        else if(vertex[i].indegree>d)
        {
            addedge(i,des,vertex[i].indegree-d);
        }
    }
    int macf=Dinic();
    for (int i=0;i<edgeflag;i++)
    {
        if(i%2==0)
        {
            if(!(edge[i].cap-edge[i].flow))
            {
                if(edge[i].v>=vm)
                {
                    reversevte(edge[i].u,edge[i].v-vm);
                }
                else
                {
                    reverseetv(edge[i].u-vm,edge[i].v);
                }
            }
        }
    }
}


void finddk(int k)
{
    vector<int>veck;
    vector<int>veck1;
    for(int i=1;i<=nodenum;i++)
    {
        if(vertex[i].indegree>=k)
        veck.push_back(i);
    }
    for(int i=1;i<=nodenum;i++)
    {
        if(vertex[i].indegree==k-1)
        {
            bool flag=false;
            for(int j=0;j<vertex[i].tedge.size();j++)
            {
                for(int l=0;l<hyperedge[vertex[i].tedge[j]].vharr.size();l++)
                {
                    int temp=hyperedge[vertex[i].tedge[j]].vharr[l];
                    if(vertex[temp].indegree>=k)
                    {
                        veck1.push_back(i);
                        flag=true;
                        break;
                    }
                }
                if(flag)
                break;
            }
            if(!flag)
            {
                for(int j=0;j<veck.size();j++)
                {
                    int count=0;
                    
                    if(reachbfs(i,veck[j],count))
                    {
                        veck1.push_back(i);
                        break;
                    }   
                    frontv.erase(frontv.begin(),frontv.end());
                    fronte.erase(fronte.begin(),fronte.end());
                    fh.erase(fh.begin(),fh.end());  
                }
            }
        }
    }
    int vecsize1=veck.size();
    veck.insert(veck.end(), veck1.begin(), veck1.end());
    dkv[k].clear();
    dkv[k].insert(dkv[k].end(),veck.begin(),veck.end());
}
int getlayer(int delta)
{
    
    int k;
    
    for(k=1;k<maxn;k++)
    {
        if(!dkv[k-1].size())
        break;
        flow(k-1);
        finddk(k);
    }
    cout<<"**********************"<<endl;
    cout<<"delta="<<delta<<endl;
    for(int i=0;i<k;i++)
    {
        cout<<"dkv["<<i<<"]:"<<dkv[i].size()<<endl;
    }
    cout<<"**********************"<<endl;
    cout<<endl;
    return k;
}
void hypergraphdecomposition()
{
    int total_layers=0;
    int maxk=vm;
    for(int i=1;i<=nodenum;i++)
    {
        dkv[0].push_back(i);
    }
    for(int i=1;i<=deltamax;i++)
    {
        for(int j=1;j<=nodenum;j++)
        {
            vertex[j].indegree=0;
            vertex[j].tedge.clear();
            vertex[j].hedge.clear();
        }
        for(int j=1;j<=maxk;j++)
        dkv[j].clear();
        for(int j=0;j<edgenum;j++)
        {
            hyperedge[j].vtarr.clear();
            hyperedge[j].vharr.clear();
        }
        orientation(i);
        maxk=getlayer(i);
        total_layers+=maxk;
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
    return 0;

}
