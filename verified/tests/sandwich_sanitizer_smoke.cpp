#include "../hgd_sandwich.hpp"
using namespace hgdsandwich;
int main(){
    for(int rep=0;rep<1;++rep){
        Orientation o(9,3);
        for(std::vector<int>e:std::vector<std::vector<int>>{{0,1,2,3},{1,3,4,5},{0,2,5,6,7},{2,4,7,8},{0,1,2,3}})
            hgdopt::greedy_add(o,std::move(e));
        auto r=from_orientation(std::move(o));
        for(bool reverse:{false,true}){
            Stats st;auto ids=decompose(r,reverse,st);
            if(ids.size()!=9)throw std::logic_error("sanitizer smoke output size");
            for(int x:ids)if(x<0)throw std::logic_error("sanitizer smoke unassigned vertex");
        }
    }
    std::cout<<"PASS: sandwich AddressSanitizer/UBSan smoke, both backends\n";
}
