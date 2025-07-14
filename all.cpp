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
#include <stdlib.h> 
#define vm 173000
#define em 234000
using namespace std;
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
Vertex vertex[vm];
Hyperedge hyperedge[em];
int edgenum=0;
int nodenum=0;
map<int,bool>fh;
bool dk[vm];
void readedge()//读数据
{
    //ifstream rda("dataset/hb8.txt");
    //ifstream rda("dataset/walmart.txt");
    ifstream rda("dataset/trivago.txt");
    //ifstream rda("dataset/senate-bills2.txt");
    //ifstream rda("dataset/house-bills2.txt");
    //ifstream rda("dataset/stackoverflow.txt");
    //ifstream rda("dataset/amazon.txt");
    //ifstream rda("dataset/house-committees.txt");
    //ifstream rda("dataset/senate-committees.txt");
    //ifstream rda("dataset/contact-high.txt");
    //ifstream rda("dataset/contact-primary.txt");
    //ifstream rda("dataset/mathoverflow.txt");
    //ifstream rda("dataset/wiki_topcats.txt");
    if(!rda)
    {
        cout<<"error!"<<endl;
        exit(1);
    }
    string strline;
    int ei=0;
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
        if(tempv>nodenum)
        nodenum=tempv;
        vertex[tempv].degree++;
        ei++;
    }
    edgenum=ei;
    cout<<"read edge successful!"<<endl;
    rda.close();
}
void orientation()
{
    for(int i=0;i<edgenum;i++)
    {
        int j=0;
        for(j=0;j<1&&j<hyperedge[i].v;j++)//为每条边的前一半顶点分配出度，起始顶点集
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

void reorientation(int k)
{
    for(int i=1;i<=nodenum;i++)
    {
        if(vertex[i].indegree>=k)
        dk[i]=true;
    }
    for(int i=1;i<=nodenum;i++)
    {
        if(dk[i])
        {
            reachout(i,k);
        }
    }
    cout<<"1111"<<endl;
    while(1)
    {
        cout<<"outk"<<endl;
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
                dk[vi]=true;
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
    for(int i=1;i<=nodenum;i++)
    {
        fh.clear();
        if(!dk[i]&&vertex[i].indegree==k-1)
        {
            if(i==269)
            {
                cout<<i<<endl;
            }
            
            if(reachdk(i,k))
            dk[i]=true;
        }
    }
    cout<<"wanchengfind"<<endl;
    int co=0;
    for(int i=1;i<=nodenum;i++)
    {
        if(dk[i])
        {
            cout<<i<<endl;
            co++;
        }  
    }
    
    cout<<"k="<<k<<" dk:"<<endl;
    cout<<co<<endl;
}


int main()
{
    clock_t start_time, end_time;
    readedge();
    cout<<"...."<<endl;
    start_time = clock();
    int k=5;
    orientation();
    cout<<"wanchengorientation"<<endl;
    reorientation(k);
    cout<<"wanchengreorientation"<<endl;
    finddk(k);
    end_time = clock();     //获取结束时间
    double Times = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    cout<<Times<<"seconds"<<endl;
    return 0;

}
