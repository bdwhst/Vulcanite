#pragma once
#include <unordered_map>
#include <metis.h>

struct Graph {
    std::vector<std::unordered_map<uint32_t, int>> adjList;

    void init(uint32_t size) { adjList.resize(size); };

    void addEdge(uint32_t from, uint32_t to, int cost) { adjList[from][to] = cost; };

    void addEdgeCost(uint32_t from, uint32_t to, int cost) { adjList[from][to] += cost; };
};

struct MetisGraph {
    idx_t nvtxs;
    std::vector<idx_t> xadj;
    std::vector<idx_t> adjncy;
    std::vector<idx_t> adjwgt;
};

void toMetisGraph(const Graph& graph, MetisGraph& metisGraph);