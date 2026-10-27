#include "../hgd_optimized.hpp"
#include <climits>
using namespace hgdopt;
static void require(bool value){if(!value)throw std::logic_error("dynamic boundary failure");}
static bool same(const Orientation&a,const Orientation&b){
    if(a.n!=b.n||a.delta!=b.delta||a.indeg!=b.indeg||a.edges.size()!=b.edges.size())return false;
    for(size_t i=0;i<a.edges.size();++i)
        if(a.edges[i].active!=b.edges[i].active||a.edges[i].vertices!=b.edges[i].vertices||
           a.edges[i].is_head!=b.edges[i].is_head)return false;
    return true;
}
static void exact(const Orientation&o,const std::vector<int>&ids){
    o.validate();require(o.n<=8);
    for(int k=0;k<=max_degree(o)+1;++k){
        int best=INT_MIN;unsigned common=(1u<<o.n)-1,actual=0;
        for(unsigned x=0;x<(1u<<o.n);++x){
            int value=-(k-1)*__builtin_popcount(x);
            for(const auto&e:o.edges)if(e.active){
                int outside=0;for(int v:e.vertices)outside+=!(x&(1u<<v));
                value+=std::max(0,std::min(o.delta,(int)e.vertices.size())-outside);
            }
            if(value>best){best=value;common=x;}else if(value==best)common&=x;
        }
        for(int v=0;v<o.n;++v)if(ids[v]>=k)actual|=1u<<v;
        require(common==actual);
    }
    auto reach=reachable_max_reverse(o);
    require(reach==ids);
    for(int v=0;v<o.n;++v)require(reach[v]<=o.indeg[v]+1);
}
int main(int argc,char**argv){
    if(argc!=2)return 2;
    std::string path=argv[1];
    auto write=[&](const std::string&s){std::ofstream file(path);file<<s;};
    for(int delta:{1,2,9}){
        Orientation o(6,delta);
        o.add_edge({0,1});o.add_edge({1,2});o.add_edge({3,4});o.add_edge({0,1});
        // Invalid batches must preserve even a non-equilibrated starting state.
        for(auto scenario:std::vector<std::pair<bool,std::string>>{
            {true,"1,7\n0,2\n"},{true,"1,7\n2,junk\n"},
            {false,"0\n0\n"},{false,"0\n99\n"},{false,"0\n-1\n"}}){
            auto before=o;long long moves=-77;write(scenario.second);
            bool threw=false;
            try{update_batch(o,path,scenario.first,&moves);}catch(const std::exception&){threw=true;}
            require(threw&&same(o,before)&&moves==-77);
        }
        write("# no changes\n\n");exact(o,update_batch(o,path,true));
        // Deletion splits the first component; another component stays untouched.
        auto untouched=o.edges[2].is_head;
        write("1\n");exact(o,update_batch(o,path,false));
        require(o.edges[2].is_head==untouched);
        write("1\n");auto before=o;bool rejected=false;
        try{update_batch(o,path,false);}catch(const std::invalid_argument&){rejected=true;}
        require(rejected&&same(o,before));
        // New vertices, duplicate endpoints and repeated edge occurrences.
        write("3,7,7\n3,7\n8\n");exact(o,update_batch(o,path,true));
        require(o.n==8&&o.edges.size()==7&&o.edges[4].vertices==o.edges[5].vertices);
        write("0\n2\n3\n4\n5\n6\n");exact(o,update_batch(o,path,false));
        for(int degree:o.indeg)require(degree==0);
        // Tombstones remain: new occurrence gets ID 7, not a reused ID.
        write("1,8\n");exact(o,update_batch(o,path,true));
        require(o.edges.size()==8&&o.edges[7].active&&!o.edges[0].active);
        write("7\n");exact(o,update_batch(o,path,false));
    }
    Orientation empty(0,1);write("# empty\n");exact(empty,update_batch(empty,path,false));
    write("2,2\n");exact(empty,update_batch(empty,path,true));require(empty.n==2);
    for(bool greedy:{false,true}){
        auto before=empty;bool rejected=false;
        try{if(greedy)greedy_add(empty,{INT_MAX});else empty.add_edge({INT_MAX});}
        catch(const std::invalid_argument&){rejected=true;}
        require(rejected&&same(empty,before));
    }
    bool rejected=false;try{Orientation invalid(-1,1);}catch(const std::invalid_argument&){rejected=true;}
    require(rejected);
    std::cout<<"PASS: dynamic validation atomicity, split components, new vertices, duplicates, tombstones, empty graphs, API integer boundaries\n";
}
