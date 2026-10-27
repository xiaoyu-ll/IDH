#pragma once
// Intrinsic statistics on the FULL active graph. This optional reporting API
// is separate from the timed search/decomposition implementations.
#include "hgd_optimized.hpp"
#include <map>

namespace hgdopt {
inline long long intrinsic_contribution(const Orientation &o,
                                        const std::vector<uint8_t> &inside) {
    if(inside.size()!=static_cast<size_t>(o.n))
        throw std::invalid_argument("intrinsic set bitmap size mismatch");
    long long total=0;
    for(const auto &e:o.edges)if(e.active) {
        int outside=0;
        for(int v:e.vertices)outside+=!inside[v];
        total+=std::max(0,std::min(o.delta,(int)e.vertices.size())-outside);
    }
    return total;
}

struct IntrinsicLayer {
    int level=0;
    long long size=0, numerator=0;
    double density()const {
        if(size<=0)throw std::domain_error("empty layer has no density");
        return static_cast<double>(numerator)/size;
    }
};

// Requires the canonical IDNs of o. Empty layers are omitted, not assigned a
// fictitious zero average. Each numerator is F(D_k)-F(D_{k+1}), computed from
// the r_e LOWEST endpoint IDNs of every active occurrence, not induced edges.
// No orientation tightness is assumed in this evaluation.
inline std::vector<IntrinsicLayer> intrinsic_layers(const Orientation &o,
                                                   const std::vector<int> &idn) {
    if(idn.size()!=static_cast<size_t>(o.n))
        throw std::invalid_argument("intrinsic IDN array size mismatch");
    std::map<int,IntrinsicLayer> layers;
    for(int k:idn) {
        if(k<0)throw std::invalid_argument("negative IDN");
        auto &layer=layers[k];layer.level=k;++layer.size;
    }
    for(const auto &e:o.edges)if(e.active) {
        std::vector<int> scores;
        scores.reserve(e.vertices.size());
        for(int v:e.vertices)scores.push_back(idn[v]);
        const int r=std::min(o.delta,(int)scores.size());
        std::partial_sort(scores.begin(),scores.begin()+r,scores.end());
        for(int i=0;i<r;++i)++layers.at(scores[i]).numerator;
    }
    std::vector<IntrinsicLayer> result;
    for(const auto &entry:layers)result.push_back(entry.second);
    return result;
}
} // namespace hgdopt
