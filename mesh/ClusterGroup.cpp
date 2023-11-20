#include "ClusterGroup.h"

void ClusterGroup::buildTriangleIndicesLocalGlobalMapping()
{
    int localTriangleIndex = 0;
    for (const auto & clusterGroupFace: clusterGroupFaces)
    {
        triangleIndicesLocalGlobalMap.push_back(clusterGroupFace.idx());
        triangleIndicesGlobalLocalMap[clusterGroupFace.idx()] = localTriangleIndex++;
    }

}

void ClusterGroup::buildLocalTriangleGraph()
{
    //int embeddingSize = targetClusterSize * (1 + (clusterGroupFaces.size() + 1) / targetClusterSize) - clusterGroupFaces.size();
    //localTriangleGraph.init(clusterGroupFaces.size() + embeddingSize);
    int embeddedSize = (clusterGroupFaces.size() + targetClusterSize - 1) / targetClusterSize * targetClusterSize;
    localTriangleGraph.init(embeddedSize);
    for (const auto& heh : clusterGroupHalfedges)
    {
        MyMesh::FaceHandle fh = mesh->face_handle(heh);
        MyMesh::FaceHandle fh2 = mesh->opposite_face_handle(heh);
        if (fh.idx() < 0 || fh2.idx() < 0) continue;
        auto clusterGroupIdx1 = mesh->property(clusterGroupIndexPropHandle, heh) - 1;
        auto clusterGroupIdx2 = mesh->property(clusterGroupIndexPropHandle, mesh->opposite_halfedge_handle(heh)) - 1;
        if (clusterGroupIdx1 == clusterGroupIdx2) // Interior edges
        {
            auto localTriangleIdx1 = triangleIndicesGlobalLocalMap[fh.idx()];
            auto localTriangleIdx2 = triangleIndicesGlobalLocalMap[fh2.idx()];
            localTriangleGraph.addEdge(localTriangleIdx1, localTriangleIdx2, 1);
            localTriangleGraph.addEdge(localTriangleIdx2, localTriangleIdx1, 1);
        }
	}
}

void ClusterGroup::generateLocalClusters()
{
    MetisGraph triangleMetisGraph;
    toMetisGraph(localTriangleGraph, triangleMetisGraph);

    localTriangleClusterIndices.resize(triangleMetisGraph.nvtxs);
    idx_t ncon = 1;

    int clusterSize = std::min(targetClusterSize, triangleMetisGraph.nvtxs); // how many triangles does each cluster contain
    localClusterNum = triangleMetisGraph.nvtxs / clusterSize; // target cluster num after partition
    if (localClusterNum <= 1)
    {
        for (int i = 0; i < triangleMetisGraph.nvtxs; ++i)
        {
			localTriangleClusterIndices[i] = 0;
		}
		return;
    }
    // Set fixed target cluster size
    real_t* tpwgts = (real_t*)malloc(ncon * localClusterNum * sizeof(real_t)); // We need to set a weight for each partition
    float sum = 0;
    for (idx_t i = 0; i < localClusterNum; ++i) {
        tpwgts[i] = static_cast<float>(clusterSize) / triangleMetisGraph.nvtxs; // 
        sum += tpwgts[i];
    }

    idx_t objVal;
    idx_t options[METIS_NOPTIONS];
    METIS_SetDefaultOptions(options);
    options[METIS_OPTION_SEED] = 42;  // Set your desired seed value
    auto res = METIS_PartGraphKway(&triangleMetisGraph.nvtxs, &ncon, triangleMetisGraph.xadj.data(), triangleMetisGraph.adjncy.data(), NULL, NULL, triangleMetisGraph.adjwgt.data(), &localClusterNum, tpwgts, NULL, options, &objVal, localTriangleClusterIndices.data());
    free(tpwgts);
    ASSERT(res, "METIS_PartGraphKway failed");
}
