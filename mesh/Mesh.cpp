#include "Mesh.h"

void Mesh::buildTriangleGraph() 
{
    // Add embedding
    int embeddingSize = targetClusterSize * (1 + (mesh.n_faces() + 1) / targetClusterSize) - mesh.n_faces();
    triangleGraph.init(mesh.n_faces() + embeddingSize);

    for (const MyMesh::EdgeHandle& edge : mesh.edges()) {
        // Do something with the edge...
        MyMesh::HalfedgeHandle heh = mesh.halfedge_handle(edge, 0);
        MyMesh::FaceHandle fh = mesh.face_handle(heh);
        MyMesh::FaceHandle fh2 = mesh.opposite_face_handle(heh);
        if (fh.idx() < 0 || fh2.idx() < 0) continue;
        triangleGraph.addEdge(fh.idx(), fh2.idx(), 1);
        triangleGraph.addEdge(fh2.idx(), fh.idx(), 1);
    }
}

void Mesh::generateCluster()
{
    MetisGraph triangleMetisGraph;
    toMetisGraph(triangleGraph, triangleMetisGraph);

    triangleClusterIndex.resize(triangleMetisGraph.nvtxs);
    idx_t ncon = 1;
    
    clusterNum = triangleMetisGraph.nvtxs / targetClusterSize;
    int clusterSize = std::min(targetClusterSize, triangleMetisGraph.nvtxs);
    // Set fixed target cluster size
    real_t* tpwgts = (real_t*)malloc(ncon * clusterNum * sizeof(real_t));
    float sum = 0;
    for (idx_t i = 0; i < clusterNum; ++i) {
        tpwgts[i] = static_cast<float>(clusterSize) / triangleMetisGraph.nvtxs;
        sum += tpwgts[i];
    }

    idx_t objVal;
    idx_t options[METIS_NOPTIONS];
    METIS_SetDefaultOptions(options);
    options[METIS_OPTION_SEED] = 42;  // Set your desired seed value
    auto res = METIS_PartGraphKway(&triangleMetisGraph.nvtxs, &ncon, triangleMetisGraph.xadj.data(), triangleMetisGraph.adjncy.data(), NULL, NULL, triangleMetisGraph.adjwgt.data(), &clusterNum, tpwgts, NULL, options, &objVal, triangleClusterIndex.data());
    free(tpwgts);
    ASSERT(res, "METIS_PartGraphKway failed");

    triangleIndicesSortedByClusterIdx.resize(mesh.n_faces());
    for (size_t i = 0; i < mesh.n_faces(); i++)
        triangleIndicesSortedByClusterIdx[i] = i;

    std::sort(triangleIndicesSortedByClusterIdx.begin(), triangleIndicesSortedByClusterIdx.end(), [&](uint32_t a, uint32_t b) {
            return triangleClusterIndex[a] < triangleClusterIndex[b];
        });

    triangleVertexIndicesSortedByClusterIdx.resize(mesh.n_faces() * 3);
    for (const auto triangleIndex: triangleIndicesSortedByClusterIdx)
    {
		auto face = mesh.face_handle(triangleIndex);
		auto fv_it = mesh.fv_iter(face);
		triangleVertexIndicesSortedByClusterIdx[triangleIndex * 3] = fv_it->idx();
        ++fv_it;
        triangleVertexIndicesSortedByClusterIdx[triangleIndex * 3 + 1] = fv_it->idx();
        ++fv_it;
        triangleVertexIndicesSortedByClusterIdx[triangleIndex * 3 + 2] = fv_it->idx();
	}

    // Init Clusters
    clusters.resize(clusterNum);
    for (MyMesh::FaceIter face_it = mesh.faces_begin(); face_it != mesh.faces_end(); ++face_it) {
        MyMesh::FaceHandle fh = *face_it;
        auto clusterIdx = triangleClusterIndex[fh.idx()];
        auto & cluster = clusters[clusterIdx];
        cluster.triangleIndices.push_back(fh.idx());
        glm::vec3 pMinWorld, pMaxWorld;
        glm::vec3 p0, p1, p2;
        MyMesh::FaceVertexIter fv_it = mesh.fv_iter(fh);

        // Get the positions of the three vertices
        auto point0 = mesh.point(*fv_it);
        ++fv_it;
        auto point1 = mesh.point(*fv_it);
        ++fv_it;
        auto point2 = mesh.point(*fv_it);

        p0[0] = point0[0];
        p0[1] = point0[1];
        p0[2] = point0[2];

        p1[0] = point1[0];
        p1[1] = point1[1];
        p1[2] = point1[2];

        p2[0] = point2[0];
        p2[1] = point2[1];
        p2[2] = point2[2];

        p0 = glm::vec3(modelMatrix * glm::vec4(p0, 1.0f));
        p1 = glm::vec3(modelMatrix * glm::vec4(p1, 1.0f));
        p2 = glm::vec3(modelMatrix * glm::vec4(p2, 1.0f));
        
        getTriangleAABB(p0, p1, p2, pMinWorld, pMaxWorld);

        cluster.mergeAABB(pMinWorld, pMaxWorld);  
    }


    uint32_t currClusterIdx = -1;
    for (size_t i = 0; i < triangleIndicesSortedByClusterIdx.size(); i++)
    {
        auto currTriangleIndex = triangleIndicesSortedByClusterIdx[i];
        if (triangleClusterIndex[currTriangleIndex] != currClusterIdx)
        {
            if (currClusterIdx != -1)
                clusters[currClusterIdx].triangleIndicesEnd = i;
            currClusterIdx = triangleClusterIndex[currTriangleIndex];
            clusters[currClusterIdx].triangleIndicesStart = i;
        }
    }
    clusters[currClusterIdx].triangleIndicesEnd = triangleIndicesSortedByClusterIdx.size();

    //for (size_t i = 0; i < clusterNum; i++)
    //{
    //    std::cout << "Cluster id: " << i << " size: " << std::count(triangleClusterIndex.begin(), triangleClusterIndex.end(), i) << std::endl;
    //}
    //auto res = METIS_PartGraphRecursive(&triangleMetisGraph.nvtxs, &ncon, triangleMetisGraph.xadj.data(), triangleMetisGraph.adjncy.data(), NULL, NULL, triangleMetisGraph.adjwgt.data(), &numClusters, NULL, NULL, NULL, &objVal, triangleClusterIndex.data());
    //ASSERT(res, "METIS_PartGraphRecursive failed");
    //for (size_t i = 0; i < triangleClusterIndex.size(); i++)
    //{
    //    std::cout << "index: " << i << " triangleClusterIndex[i]: " << triangleClusterIndex[i] << std::endl;
    //}
    /*if (res != METIS_OK) {
        std::cout << "METIS_PartGraphKway failed" << std::endl;
    }*/

}


void Mesh::buildClusterGraph() 
{
    // Add embedding
    int embeddingSize = targetClusterGroupSize * (1 + (clusterNum + 1) / targetClusterGroupSize) - clusterNum;
    clusterGraph.init(clusterNum + embeddingSize);

    //clusterGraph.init(clusterNum);
    for (const MyMesh::EdgeHandle& edge : mesh.edges()) {
        // Do something with the edge...
        MyMesh::HalfedgeHandle heh = mesh.halfedge_handle(edge, 0);
        MyMesh::FaceHandle fh = mesh.face_handle(heh);
        MyMesh::FaceHandle fh2 = mesh.opposite_face_handle(heh);
        if (fh.idx() < 0 || fh2.idx() < 0) continue;
        auto clusterIdx1 = triangleClusterIndex[fh.idx()];
        auto clusterIdx2 = triangleClusterIndex[fh2.idx()];
        if (clusterIdx1 != clusterIdx2)
        {
            clusterGraph.addEdgeCost(clusterIdx1, clusterIdx2, 1);
            clusterGraph.addEdgeCost(clusterIdx2, clusterIdx1, 1);
        }
    }
}

void Mesh::colorClusterGraph()
{
    std::vector<int> clusterSortedByConnectivity(clusterNum, -1);
    for (size_t i = 0; i < clusterNum; i++)
    {
        clusterSortedByConnectivity[i] = i;
    }
    std::sort(clusterSortedByConnectivity.begin(), clusterSortedByConnectivity.end(), [&](int a, int b) {
        return clusterGraph.adjList[a].size() > clusterGraph.adjList[b].size();
        });

    for (int clusterIndex : clusterSortedByConnectivity)
    {
        std::unordered_set<int> neighbor_colors;
        for (auto tosAndCosts : clusterGraph.adjList[clusterIndex]) {
            auto neighbor = tosAndCosts.first;
            if (clusterColorAssignment.find(neighbor) != clusterColorAssignment.end()) {
                neighbor_colors.insert(clusterColorAssignment[neighbor]);
            }
        }

        int color = 0;
        while (neighbor_colors.find(color) != neighbor_colors.end()) {
            color++;
        }

        clusterColorAssignment[clusterIndex] = color;
    }
}

void Mesh::lockClusterGroupBoundaries(MyMesh& mymesh)
{
    for (const auto & clusterGroup: clusterGroups)
    {
        for (const auto boundaryIndex : clusterGroup.boundaryIndices) 
        {
            mymesh.status(mymesh.vertex_handle(boundaryIndex)).set_locked(true);
        }
    }
}

void Mesh::simplifyMesh(MyMesh & mymesh)
{
    OpenMesh::Decimater::DecimaterT<MyMesh> decimater(mymesh);
    OpenMesh::Decimater::ModQuadricT<MyMesh>::Handle hModQuadric;
    decimater.add(hModQuadric);
    decimater.module(hModQuadric).set_binary(false);

    decimater.initialize();

    for (auto he_it = mymesh.halfedges_begin(); he_it != mymesh.halfedges_end(); ++he_it)
        if (mymesh.is_boundary(*he_it)) {
            mymesh.status(mymesh.to_vertex_handle(*he_it)).set_locked(true);
            mymesh.status(mymesh.from_vertex_handle(*he_it)).set_locked(true);
        }

    size_t original_faces = mymesh.n_faces();
    std::cout << "NUM FACES BEFORE: " << original_faces << std::endl;
    double percentage = 0.2;
    size_t target_faces = static_cast<size_t>(original_faces * percentage);

    decimater.decimate_to_faces(0, target_faces);
    mymesh.garbage_collection();
    size_t actual_faces = mymesh.n_faces();
    std::cout << "NUM FACES AFTER: " << actual_faces << std::endl;
}


void Mesh::generateClusterGroup()
{
    MetisGraph clusterMetisGraph;
    toMetisGraph(clusterGraph, clusterMetisGraph);
    clusterGroupIndex.resize(clusterMetisGraph.nvtxs);

    real_t targetVertexWeight = (real_t)clusterMetisGraph.nvtxs / targetClusterGroupSize;
    idx_t ncon = 1;
    clusterGroupNum = clusterMetisGraph.nvtxs / targetClusterGroupSize;
    clusterGroups.resize(clusterGroupNum);
    if (clusterGroupNum == 1) { // Should quit now
        for (size_t i = 0; i < clusterMetisGraph.nvtxs; i++)
        {
            clusterGroupIndex[i] = 0;
            clusterGroups[0].clusterIndices.push_back(i);
        }
        return;
    }
    
    int clusterGroupSize = std::min(targetClusterGroupSize, clusterMetisGraph.nvtxs);

    real_t* tpwgts = (real_t*)malloc(ncon * clusterGroupNum * sizeof(real_t));
    for (idx_t i = 0; i < clusterGroupNum; ++i) {
        tpwgts[i] = static_cast<float>(targetClusterGroupSize) / clusterMetisGraph.nvtxs;
    }

    idx_t objVal;
    idx_t options[METIS_NOPTIONS];
    METIS_SetDefaultOptions(options);
    options[METIS_OPTION_SEED] = 42;  // Set your desired seed value
    auto res = METIS_PartGraphKway(&clusterMetisGraph.nvtxs, &ncon, clusterMetisGraph.xadj.data(), clusterMetisGraph.adjncy.data(), NULL, NULL, clusterMetisGraph.adjwgt.data(), &clusterGroupNum, tpwgts, NULL, options, &objVal, clusterGroupIndex.data());
    free(tpwgts);
    ASSERT(res, "METIS_PartGraphKway failed");
    
    for (size_t clusterIdx = 0; clusterIdx < clusterNum; clusterIdx++)
    {
        auto cluster = clusters[clusterIdx];
        auto clusterGroupIdx = clusterGroupIndex[clusterIdx];

        cluster.clusterGroupIndex = clusterGroupIdx;
        clusterGroups[clusterGroupIdx].clusterIndices.push_back(clusterIdx);
    }

    // Find the boundary of each cluster group

    for (const MyMesh::EdgeHandle& edge : mesh.edges()) {
        MyMesh::HalfedgeHandle heh = mesh.halfedge_handle(edge, 0);
        MyMesh::FaceHandle fh = mesh.face_handle(heh);
        MyMesh::FaceHandle fh2 = mesh.opposite_face_handle(heh);
        if (mesh.is_boundary(heh) && fh.idx() > 0) {
            auto clusterIdx1 = triangleClusterIndex[fh.idx()];
            auto clusterGroupIdx1 = clusterGroupIndex[clusterIdx1];
            auto vh1 = mesh.to_vertex_handle(heh);
            auto vh2 = mesh.from_vertex_handle(heh);
            clusterGroups[clusterGroupIdx1].boundaryIndices.insert(vh1.idx());
            clusterGroups[clusterGroupIdx1].boundaryIndices.insert(vh2.idx());
        }
        else if (mesh.is_boundary(mesh.opposite_halfedge_handle(heh)) && fh2.idx() > 0) {
            auto clusterIdx2 = triangleClusterIndex[fh2.idx()];
            auto clusterGroupIdx2 = clusterGroupIndex[clusterIdx2];
            auto vh1 = mesh.to_vertex_handle(mesh.opposite_halfedge_handle(heh));
            auto vh2 = mesh.from_vertex_handle(mesh.opposite_halfedge_handle(heh));
            clusterGroups[clusterGroupIdx2].boundaryIndices.insert(vh1.idx());
            clusterGroups[clusterGroupIdx2].boundaryIndices.insert(vh2.idx());
        }
        if (fh.idx() < 0 || fh2.idx() < 0) continue;
        auto clusterIdx1 = triangleClusterIndex[fh.idx()];
        auto clusterIdx2 = triangleClusterIndex[fh2.idx()];
        auto clusterGroupIdx1 = clusterGroupIndex[clusterIdx1];
        auto clusterGroupIdx2 = clusterGroupIndex[clusterIdx2];
        if (clusterGroupIdx1 != clusterGroupIdx2) {
            auto vh1 = mesh.to_vertex_handle(heh);
            auto vh2 = mesh.to_vertex_handle(mesh.opposite_halfedge_handle(heh));

            clusterGroups[clusterGroupIdx1].boundaryIndices.insert(vh1.idx());
            clusterGroups[clusterGroupIdx1].boundaryIndices.insert(vh2.idx());

            clusterGroups[clusterGroupIdx2].boundaryIndices.insert(vh1.idx());
            clusterGroups[clusterGroupIdx2].boundaryIndices.insert(vh2.idx());
        }
    }

    //for (int i = 0; i < clusterGroupNum;++i) {
    //    auto clusterGroup = clusterGroups[i];
    //    std::cout << "Cluster Group size: " << clusterGroup.clusterIndices.size() << std::endl;
    //    std::cout << "Cluster Boundary size: " << clusterGroup.boundaryIndices.size() << std::endl;
    //}

    //for (size_t i = 0; i < clusterGroupNum; i++)
    //{
    //    std::cout << "Cluster Group id: " << i << " size: " << std::count(clusterGroupIndex.begin(), clusterGroupIndex.end(), i) << std::endl;
    //}
    //
    //for (size_t i = 0; i < clusterGroupIndex.size(); i++)
    //{
    //    std::cout << "index: " << i << " clusterGroupIndex[i]: " << clusterGroupIndex[i] << std::endl;
    //}

}

void Mesh::initVertexBuffer(){
    for (MyMesh::FaceIter f_it = mesh.faces_begin(); f_it != mesh.faces_end(); ++f_it) {
        MyMesh::FaceHandle face = *f_it;
        for (MyMesh::FaceVertexIter fv_it = mesh.cfv_iter(face); fv_it.is_valid(); ++fv_it) {
            MyMesh::VertexHandle vertex = *fv_it;
            vkglTF::Vertex v;
            v.pos = glm::vec3(mesh.point(vertex)[0], mesh.point(vertex)[1], mesh.point(vertex)[2]);
            v.normal = glm::vec3(mesh.normal(vertex)[0], mesh.normal(vertex)[1], mesh.normal(vertex)[2]);
            v.uv = glm::vec2(mesh.texcoord2D(vertex)[0], mesh.texcoord2D(vertex)[1]);
            // TODO: v.tangent not assigned. How to assign?
            // Assign clusterId and clusterGroupId
            int clusterId = triangleClusterIndex[face.idx()];
            v.joint0 = glm::vec4(nodeColors[clusterColorAssignment[clusterId]], clusterId);

            int clusterGroupId = clusterGroupIndex[clusterId];
            // Skip coloring clusterGroupGraph for now, it requires extra work. Modulo seems fine for graph coloring
            v.weight0 = glm::vec4(nodeColors[clusterGroupId % nodeColors.size()], clusterGroupId);
            vertexBuffer.push_back(v);
        }
    }
}

void Mesh::createVertexBuffer(vks::VulkanDevice* device, VkQueue transferQueue) {
    size_t vertexBufferSize = vertexBuffer.size() * sizeof(vkglTF::Vertex);
    vertices.count = static_cast<uint32_t>(vertexBuffer.size());

    assert(vertexBufferSize > 0);

    struct StagingBuffer {
        VkBuffer buffer;
        VkDeviceMemory memory;
    } vertexStaging;

    // Create staging buffers
    // Vertex data
    VK_CHECK_RESULT(device->createBuffer(
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        vertexBufferSize,
        &vertexStaging.buffer,
        &vertexStaging.memory,
        vertexBuffer.data()));

    // Create device local buffers
    // Vertex buffer
    VK_CHECK_RESULT(device->createBuffer(
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | vkglTF::memoryPropertyFlags,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        vertexBufferSize,
        &vertices.buffer,
        &vertices.memory));

    // Copy from staging buffers
    VkCommandBuffer copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    VkBufferCopy copyRegion = {};

    copyRegion.size = vertexBufferSize;
    vkCmdCopyBuffer(copyCmd, vertexStaging.buffer, vertices.buffer, 1, &copyRegion);

    device->flushCommandBuffer(copyCmd, transferQueue, true);

    vkDestroyBuffer(device->logicalDevice, vertexStaging.buffer, nullptr);
    vkFreeMemory(device->logicalDevice, vertexStaging.memory, nullptr);
}

void Mesh::draw(VkCommandBuffer commandBuffer, uint32_t renderFlags, VkPipelineLayout pipelineLayout, uint32_t bindImageSet) 
{
    const VkDeviceSize offsets[1] = { 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertices.buffer, offsets);
    vkCmdDraw(commandBuffer, vertices.count, 1, 0, 0);
}