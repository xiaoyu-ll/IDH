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
int path_parent[vm];
int path_parent_edge[vm];
int path_label[vm];
unsigned int path_seen[vm];
unsigned int path_search_id = 0;

unsigned int next_path_search_id()
{
    ++path_search_id;
    if (path_search_id == 0) {
        fill(path_seen, path_seen + vm, 0);
        path_search_id = 1;
    }
    return path_search_id;
}
void readedge(const string &path)//读数据
{
    //ifstream rda("dataset/hb8.txt");
    //ifstream rda("dataset/walmart.txt");
    ifstream rda(path);
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
void orientation(int delta)
{
    for(int i=0;i<edgenum;i++)
    {
        int j=0;
        for(j=0;j<delta&&j<hyperedge[i].v;j++)//assign min(delta, |e|) targets
        {
            hyperedge[i].vharr.push_back(hyperedge[i].varr[j]);
            vertex[hyperedge[i].varr[j]].indegree++;
            vertex[hyperedge[i].varr[j]].hedge.push_back(i);//边指向结点
        }
        for(;j<hyperedge[i].v;j++)//assign the remaining vertices as sources
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

// Find and reverse complete reversible hyperpaths ending at vi.  The previous
// implementation only tested one-edge indegree gaps while recursively walking
// predecessors; it could therefore miss a path whose endpoints differ by two
// although no individual arc on the path does.
bool reverse_path_to(int vi, int k)
{
    const unsigned int search_id = next_path_search_id();
    queue<int> q;
    path_seen[vi] = search_id;
    path_parent[vi] = vi;
    q.push(vi);
    int source = -1;
    while (!q.empty() && source == -1)
    {
        int cur = q.front(); q.pop();
        for (int ee : vertex[cur].hedge)
        {
            for (int pred : hyperedge[ee].vtarr)
            {
                if (path_seen[pred] == search_id) continue;
                path_seen[pred] = search_id;
                path_parent[pred] = cur;
                path_parent_edge[pred] = ee;
                if (!dk[pred] && vertex[vi].indegree - vertex[pred].indegree >= 2) {
                    source = pred;
                    break;
                }
                q.push(pred);
            }
            if (source != -1) break;
        }
    }
    if (source == -1) return false;
    int cur = source;
    while (cur != vi)
    {
        int next = path_parent[cur];
        reverse(next, cur, path_parent_edge[cur]);
        cur = next;
    }
    if (vertex[source].indegree >= k) dk[source] = true;
    return true;
}

void reachout(int vi,int k)
{
    // Fast path used by Algorithm 4: consume adjacent cross-boundary
    // reversals and recursively continue only from vertices whose state
    // changed.  The set-wise cleanup below handles the non-local paths that
    // cannot be exposed by these local updates alone.
    queue<int> q;
    for (int i = 0; i < (int)vertex[vi].hedge.size(); ++i) {
        int ee = vertex[vi].hedge[i];
        for (int j = 0; j < (int)hyperedge[ee].vtarr.size(); ++j) {
            int vv = hyperedge[ee].vtarr[j];
            if (!dk[vv] && vertex[vi].indegree - vertex[vv].indegree >= 2) {
                reverse(vi, vv, ee);
                if (vertex[vv].indegree >= k) dk[vv] = true;
                q.push(vv);
                --i;
                break;
            }
        }
    }
    while (!q.empty()) {
        int v = q.front(); q.pop();
        reachout(v, k);
    }
}

bool reverse_path_from(int vi)
{
    const unsigned int search_id = next_path_search_id();
    queue<int> q;
    path_seen[vi] = search_id;
    path_parent[vi] = vi;
    q.push(vi);
    int target = -1;
    while (!q.empty() && target == -1)
    {
        int cur = q.front(); q.pop();
        for (int ee : vertex[cur].tedge)
        {
            for (int next : hyperedge[ee].vharr)
            {
                if (path_seen[next] == search_id) continue;
                path_seen[next] = search_id;
                path_parent[next] = cur;
                path_parent_edge[next] = ee;
                if (!dk[next] && vertex[next].indegree - vertex[vi].indegree >= 2) {
                    target = next;
                    break;
                }
                q.push(next);
            }
            if (target != -1) break;
        }
    }
    if (target == -1) return false;
    vector<pair<int,int> > steps;
    for (int cur = target; cur != vi; cur = path_parent[cur])
        steps.push_back(make_pair(cur, path_parent_edge[cur]));
    reverse(steps.begin(), steps.end());
    int cur = vi;
    for (const auto &step : steps)
    {
        int next = step.first;
        reverse(next, cur, step.second);
        cur = next;
    }
    return true;
}

void reachin(int vi,int k)
{
    (void)k;
    while (reverse_path_from(vi)) {}
}

// Exhaust cross-boundary paths set-wise.  Running a full backward BFS once for
// every member of S repeats the same failed traversal |S| times on dense data.
// This multi-source search propagates the minimum outside endpoint indegree
// through the oriented incidence graph and returns one reversible path into S.
// Repeating only after a successful reversal is equivalent to exhausting all
// per-vertex REACHOUT calls, but a no-path certificate costs one graph scan.
bool reverse_one_crossing_path(int k)
{
    const int INF_LABEL = 0x3f3f3f3f;
    priority_queue<pair<int,int>, vector<pair<int,int> >,
                   greater<pair<int,int> > > pq;
    fill(path_label, path_label + nodenum + 1, INF_LABEL);
    for (int v = 1; v <= nodenum; ++v) {
        if (!dk[v]) {
            path_label[v] = vertex[v].indegree;
            path_parent[v] = v;
            path_parent_edge[v] = -1;
            pq.push(make_pair(path_label[v], v));
        }
    }

    int target = -1;
    while (!pq.empty()) {
        int label = pq.top().first;
        int cur = pq.top().second;
        pq.pop();
        if (label != path_label[cur]) continue;
        if (dk[cur] && vertex[cur].indegree - label >= 2) {
            target = cur;
            break;
        }
        for (int ee : vertex[cur].tedge) {
            for (int next : hyperedge[ee].vharr) {
                if (label < path_label[next]) {
                    path_label[next] = label;
                    path_parent[next] = cur;
                    path_parent_edge[next] = ee;
                    pq.push(make_pair(label, next));
                }
            }
        }
    }
    if (target == -1) return false;

    vector<pair<int,int> > steps;
    int source = target;
    while (path_parent[source] != source) {
        steps.push_back(make_pair(source, path_parent_edge[source]));
        source = path_parent[source];
    }
    reverse(steps.begin(), steps.end());
    int cur = source;
    for (const auto &step : steps) {
        reverse(step.first, cur, step.second);
        cur = step.first;
    }
    if (vertex[source].indegree >= k) {
        dk[source] = true;
        // A promoted endpoint may expose a whole local cascade.  Consume it
        // now instead of rebuilding the global reachability forest once per
        // adjacent transfer.
        reachout(source, k);
    }
    return true;
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
        if(dk[i]) reachout(i,k);
    }
    while (reverse_one_crossing_path(k)) {}
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
        bool repaired = false;
        while (reverse_one_crossing_path(k)) repaired = true;
        if(!flag && !repaired)
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
            co++;
        }  
    }
    
    cout<<"k="<<k<<" dk:"<<endl;
    cout<<co<<endl;
}


int main(int argc, char **argv)
{
    clock_t start_time, end_time;
    const string input = argc > 1 ? argv[1] : "dataset/trivago.txt";
    const int k = argc > 2 ? stoi(argv[2]) : 5;
    const int delta = argc > 3 ? stoi(argv[3]) : 1;
    if (k < 0 || delta < 1) {
        cerr << "usage: " << argv[0] << " [dataset] [k>=0] [delta>=1]" << endl;
        return 2;
    }
    readedge(input);
    cout<<"...."<<endl;
    start_time = clock();
    orientation(delta);
    cout<<"wanchengorientation"<<endl;
    reorientation(k);
    cout<<"wanchengreorientation"<<endl;
    finddk(k);
    end_time = clock();     //获取结束时间
    double Times = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    cout<<Times<<"seconds"<<endl;
    return 0;

}
