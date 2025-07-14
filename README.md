# Density Decomposition on Hypergraphs
# ABSTRACT
Mining dense subhypergraphs is a fundamental task in hypergraph analysis, with broad applications in community detection, pattern discovery, and task scheduling. However, existing models—such as $k$-core, neighbor-$k$-core, and $(k, h)$-core—either incur high computational costs or fail to capture the intrinsic structural density of hypergraphs.
In this paper, we propose a novel model, the $(k, \delta)$-dense subhypergraph, which jointly considers vertex-level connectivity and local edge constraints to better characterize hypergraph cohesion. We show that $(k, \delta)$-dense subhypergraphs naturally induce a hierarchical and nested decomposition, effectively revealing multi-scale dense regions.
To enable efficient computation, we develop an fair-stable-based algorithm with time complexity $O(n \cdot m \cdot \delta)$, and further propose a divide-and-conquer decomposition framework that computes the full density decomposition in $O(d^E_{max} \cdot n \cdot m \cdot \delta \cdot \log k_{\max})$.
Extensive experiments on nine real-world hypergraph datasets show that our method consistently outperforms prior core-based approaches in terms of density, conductance, structural granularity, and robustness—while maintaining strong computational efficiency. Case studies further highlight the ability of our method to uncover tighter, more cohesive, and hierarchically structured communities that are often overlooked by existing techniques.

# Datasets

Because some datasets used in the paper can download  them at https://pan.quark.cn/s/1308697e75fa](https://www.cs.cornell.edu/~arb/data/

# Compile
All experiments are compiled in the same way, first in the ` IDH ` directory.


` g++ -O3 all.cpp -o all `




# Run
To run the program, you can choose an algorithm to run.


` ./all `



