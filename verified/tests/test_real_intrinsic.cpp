#include "../hgd_intrinsic.hpp"
using namespace hgdopt;
static void require(bool ok){if(!ok)throw std::logic_error("real layer regression");}
int main(int argc,char **argv){
    if(argc!=5)return 2;
    Orientation o(parse_integer(argv[4],0,"n"),parse_integer(argv[2],1,"delta"));
    load_edges(o,argv[1]); // Deliberately NOT equilibrated: evaluation is intrinsic.
    std::ifstream input(argv[3]);require(bool(input));
    std::vector<int> ids(o.n,-1);
    std::string line;
    while(std::getline(input,line)){
        auto comma=line.find(',');require(comma!=std::string::npos);
        int v=parse_integer(line.substr(0,comma),1,"v")-1;
        require(v<o.n && ids[v]<0);
        ids[v]=parse_integer(line.substr(comma+1),0,"idn");
    }
    for(int id:ids)require(id>=0);
    long long total=0,size=0;
    std::cout<<"level,size,marginal,F_Dk,F_Dk1\n";
    for(const auto &layer:intrinsic_layers(o,ids)){
        std::vector<uint8_t> d(o.n),above(o.n);
        for(int v=0;v<o.n;++v){d[v]=ids[v]>=layer.level;above[v]=ids[v]>layer.level;}
        long long f=intrinsic_contribution(o,d),next=intrinsic_contribution(o,above);
        require(layer.numerator==f-next);
        require(layer.numerator>(layer.level-1)*layer.size);
        require(layer.numerator<=layer.level*layer.size);
        total+=layer.numerator;size+=layer.size;
        std::cout<<layer.level<<','<<layer.size<<','<<layer.numerator<<','<<f<<','<<next<<'\n';
    }
    require(size==o.n && total==o.R());
}
