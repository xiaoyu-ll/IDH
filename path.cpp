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
#define maxn1 1500
#define maxn2 61000
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
Vertex vertex[maxn1];
Hyperedge hyperedge[maxn2];
int edgenum=0;
int nodenum=0;
vector<int>pe;
vector<int>pv;
map<int,int>frontv;
map<int,int>fronte;
map<int,bool>fh;
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
        hyperedge[i].h=hyperedge[i].v/2;//每条超边的终点集合的大小
        int j=0;
        for(j=0;j<3&&j<hyperedge[i].v;j++)//为每条边的前一半顶点分配出度，起始顶点集
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
bool bfs(int vi, int vj, int co)
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
        if(bfs(q1.front(),vj,co++))
        {
            return true;
            break;
        }
        q1.pop();
    }
    return false;
}
void reverse(int vi, int vj)
{
    if(vi==vj)
    return;
    //int i=0;    
    std::vector<int>::iterator pos;
    //翻转vj到fronte[vj]的入边,处理顶点
    vertex[vj].indegree--;  //1
    pos = find(vertex[vj].hedge.begin(),vertex[vj].hedge.end(),fronte[vj]);
    if (pos != vertex[vj].hedge.end()) //0
    {
        vertex[vj].hedge.erase(pos);
    }
    vertex[vj].tedge.push_back(fronte[vj]);//1
    //翻转vj到fronte[vj]的入边,处理边   1
    pos = find(hyperedge[fronte[vj]].vharr.begin(),hyperedge[fronte[vj]].vharr.end(),vj);
    if (pos != hyperedge[fronte[vj]].vharr.end()) 
    {
        hyperedge[fronte[vj]].vharr.erase(pos);
    }
    hyperedge[fronte[vj]].vtarr.push_back(vj);
    //翻转frontv[vj]到fronte[vj]的出边,处理边   1
    pos = find(hyperedge[fronte[vj]].vtarr.begin(),hyperedge[fronte[vj]].vtarr.end(),frontv[vj]);
    if (pos != hyperedge[fronte[vj]].vtarr.end()) 
    {
        hyperedge[fronte[vj]].vtarr.erase(pos);
    }
    hyperedge[fronte[vj]].vharr.push_back(frontv[vj]);
    //翻转frontv[vj]到fronte[vj]的出边,处理顶点
    vertex[frontv[vj]].indegree++;//1
    pos = find(vertex[frontv[vj]].tedge.begin(),vertex[frontv[vj]].tedge.end(),fronte[vj]);
    if (pos != vertex[frontv[vj]].tedge.end()) //1
    {
        vertex[frontv[vj]].tedge.erase(pos);
    }
    vertex[frontv[vj]].hedge.push_back(fronte[vj]);//0
    //相应的把路径的第一条和相关顶点边去掉
    //pe.erase(pe.begin());
    //pv.erase(pv.begin());
    reverse(vi, frontv[vj]);
}
void reorientation()
{
    while(1)
    {
        bool flag=false;
        for(int i=0;i<=nodenum;i++)
        {
            for(int j=0;j<=nodenum;j++)
            {
                if(i==j)
                continue;
                frontv.erase(frontv.begin(),frontv.end());
                fronte.erase(fronte.begin(),fronte.end());
                fh.erase(fh.begin(),fh.end());
                if(vertex[j].indegree>(vertex[i].indegree+1))
                {
                    int count=0;
                    if(bfs(i,j,count))
                    {
                        //pv.insert(pv.begin(),i); 
                        reverse(i,j);
                        flag=true;
                    } 
                }
            }
        }
        if(!flag)
        break;
    }
}


bool reachk(int vi,int vk, int co)
{
    for(int i=0;i<vertex[vi].tedge.size();i++)//遍历顶点vi的每条出边，结点指向边
    {
        pe.push_back(vertex[vi].tedge[i]);//把路径上的边依次存储下来
        co++;
        for(int l=0;l<pe.size()-1;l++)
        {
            if(vertex[vi].tedge[i]==pe[l])
            {
                return false;
            }
        }
        if(co==nodenum-1)
        {
            return false;
            break;
        }
        for(int j=0;j<hyperedge[vertex[vi].tedge[i]].vharr.size();j++)//遍历这条出边的所有具有入度的顶点，边指向结点
        {
            if(hyperedge[vertex[vi].tedge[i]].vharr[j]==vk)
            {
                return true;
                break;
            } 
            else
            {
                if(reachk(hyperedge[vertex[vi].tedge[i]].vharr[j],vk,co))
                return true;
            }
        }
    }
    return false;     
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
                pe.clear();
                int count=0;
                fh.erase(fh.begin(),fh.end());
                if(bfs(i,veck[j],count))
                {
                    veck1.push_back(i);
                    break;
                }     
            }
            }
        }
    }
    int vecsize1=veck.size();
    veck.insert(veck.end(), veck1.begin(), veck1.end());
    int vecsize2=veck.size();
    for(int i=0;i<veck.size();i++)
    cout<<veck[i]<<endl;
    cout<<"k="<<k<<" dk:"<<endl;
    cout<<vecsize1<<" "<<vecsize2<<endl;
}
int main()
{
    clock_t start_time, end_time;
    readedge();
    cout<<"...."<<endl;
    start_time = clock();
    orientation();
    reorientation();
    //hyperedgerotation();
    int k=3;
    finddk(k);
    int count=0;
    end_time = clock();     //获取结束时间
    double Times = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    cout<<Times<<"seconds"<<endl;
    system("pause");
    return 0;

}
