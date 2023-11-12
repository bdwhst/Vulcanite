#include "Graph.h"

void toMetisGraph(const Graph& graph, MetisGraph& metisGraph)
{
	metisGraph.nvtxs = graph.adjList.size();
    for (auto& tosAndCosts : graph.adjList) {
        metisGraph.xadj.push_back(metisGraph.adjncy.size());
        for (const auto & toAndCost : tosAndCosts) {
            metisGraph.adjncy.push_back(toAndCost.first);
            metisGraph.adjwgt.push_back(toAndCost.second);
        }
    }
    metisGraph.xadj.push_back(metisGraph.adjncy.size());
}
