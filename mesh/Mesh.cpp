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

    // Init Clusters
    clusters.resize(clusterNum);
    for (MyMesh::FaceIter face_it = mesh.faces_begin(); face_it != mesh.faces_end(); ++face_it) {
        MyMesh::FaceHandle fh = *face_it;
        auto clusterIdx = triangleClusterIndex[fh.idx()];
        auto & cluster = clusters[clusterIdx];
        cluster.triangleIndices.push_back(fh.idx());
        glm::vec3 pMin, pMax;
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

        getTriangleAABB(p0, p1, p2, pMin, pMax);

        cluster.mergeAABB(pMin, pMax);  
    }

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

void Mesh::simplifyMesh()
{
    OpenMesh::Decimater::DecimaterT<MyMesh> decimater(mesh);
    OpenMesh::Decimater::ModQuadricT<MyMesh>::Handle hModQuadric;
    decimater.add(hModQuadric);
    decimater.module(hModQuadric).set_binary(false);

    decimater.initialize();

    for (auto he_it = mesh.halfedges_begin(); he_it != mesh.halfedges_end(); ++he_it)
        if (mesh.is_boundary(*he_it)) {
            mesh.status(mesh.to_vertex_handle(*he_it)).set_locked(true);
            mesh.status(mesh.from_vertex_handle(*he_it)).set_locked(true);
        }

    size_t original_faces = mesh.n_faces();
    std::cout << "NUM FACES BEFORE: " << original_faces << std::endl;
    double percentage = 0.2;
    size_t target_faces = static_cast<size_t>(original_faces * percentage);

    decimater.decimate_to_faces(0, target_faces);
    mesh.garbage_collection();
    size_t actual_faces = mesh.n_faces();
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

    for (size_t i = 0; i < clusterGroupNum; i++)
    {
        std::cout << "Cluster Group id: " << i << " size: " << std::count(clusterGroupIndex.begin(), clusterGroupIndex.end(), i) << std::endl;
    }
    
    for (size_t i = 0; i < clusterGroupIndex.size(); i++)
    {
        std::cout << "index: " << i << " clusterGroupIndex[i]: " << clusterGroupIndex[i] << std::endl;
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