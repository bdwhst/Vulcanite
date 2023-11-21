#include "Mesh.h"

void Mesh::assignTriangleClusterGroup()
{
    for (const auto & heh: mesh.halfedges())
    {
        if (mesh.is_boundary(heh)) continue;
        auto clusterGroupIdx = mesh.property(clusterGroupIndexPropHandle, heh) - 1;
        oldClusterGroups[clusterGroupIdx].clusterGroupHalfedges.push_back(heh);
        oldClusterGroups[clusterGroupIdx].clusterGroupFaces.insert(mesh.face_handle(heh));
    }

    triangleClusterIndex.resize(mesh.n_faces(), -1);
    uint32_t clusterIndexOffset = 0;
    for (auto & oldClusterGroup: oldClusterGroups)
    {
        oldClusterGroup.clusterGroupIndexPropHandle = clusterGroupIndexPropHandle;
        oldClusterGroup.mesh = &mesh;
        oldClusterGroup.buildTriangleIndicesLocalGlobalMapping();
        oldClusterGroup.buildLocalTriangleGraph();
        oldClusterGroup.generateLocalClusters();

        // Merging local cluster indices to global cluster indices
        for (const auto & fh: oldClusterGroup.clusterGroupFaces)
        {
            auto localTriangleIdx = oldClusterGroup.triangleIndicesGlobalLocalMap[fh.idx()];
            ASSERT(triangleClusterIndex[fh.idx()] < 0, "Repeat clsutering");

            triangleClusterIndex[fh.idx()] = clusterIndexOffset + oldClusterGroup.localTriangleClusterIndices[localTriangleIdx];
		}
        clusterIndexOffset += oldClusterGroup.localClusterNum;
    }
    for (size_t i = 0; i < triangleClusterIndex.size(); i++)
    {
        ASSERT(triangleClusterIndex[i] >= 0, "triangleClusterIndex[i] < 0");
    }

    clusterNum = *std::max_element(triangleClusterIndex.begin(), triangleClusterIndex.end()) + 1;   
    triangleIndicesSortedByClusterIdx.resize(mesh.n_faces());
    for (size_t i = 0; i < mesh.n_faces(); i++)
        triangleIndicesSortedByClusterIdx[i] = i;

    std::sort(triangleIndicesSortedByClusterIdx.begin(), triangleIndicesSortedByClusterIdx.end(), [&](uint32_t a, uint32_t b) {
        return triangleClusterIndex[a] < triangleClusterIndex[b];
        });

    triangleVertexIndicesSortedByClusterIdx.resize(mesh.n_faces() * 3);
    for (int i = 0; i < triangleIndicesSortedByClusterIdx.size(); ++i)
    {
        auto triangleIndex = triangleIndicesSortedByClusterIdx[i];
        auto face = mesh.face_handle(triangleIndex);
        auto fv_it = mesh.fv_iter(face);
        triangleVertexIndicesSortedByClusterIdx[i * 3] = fv_it->idx();
        ++fv_it;
        triangleVertexIndicesSortedByClusterIdx[i * 3 + 1] = fv_it->idx();
        ++fv_it;
        triangleVertexIndicesSortedByClusterIdx[i * 3 + 2] = fv_it->idx();
    }
    clusters.resize(clusterNum);
    for (MyMesh::FaceIter face_it = mesh.faces_begin(); face_it != mesh.faces_end(); ++face_it) {
        MyMesh::FaceHandle fh = *face_it;
        auto clusterIdx = triangleClusterIndex[fh.idx()];
        auto& cluster = clusters[clusterIdx];
        cluster.triangleIndices.push_back(fh.idx());
    }

}

void Mesh::buildTriangleGraph()
{
    // Add embedding
    int embeddingSize = targetClusterSize * (1 + (mesh.n_faces() + 1) / targetClusterSize) - mesh.n_faces();
    triangleGraph.init(mesh.n_faces() + embeddingSize);
    isLastLODEdgeVertices.resize(mesh.n_faces() * 3, false);
    for (const MyMesh::EdgeHandle& edge : mesh.edges()) {
        MyMesh::HalfedgeHandle heh = mesh.halfedge_handle(edge, 0);
        MyMesh::FaceHandle fh = mesh.face_handle(heh);
        MyMesh::FaceHandle fh2 = mesh.opposite_face_handle(heh);
        auto clusterGroupIdx1 = mesh.property(clusterGroupIndexPropHandle, heh) - 1;
        auto clusterGroupIdx2 = mesh.property(clusterGroupIndexPropHandle, mesh.opposite_halfedge_handle(heh)) - 1;
        if (clusterGroupIdx1 != clusterGroupIdx2) {
            auto vh1 = mesh.to_vertex_handle(heh);
            auto vh2 = mesh.to_vertex_handle(mesh.opposite_halfedge_handle(heh));
            for (MyMesh::FaceVertexIter fv_it = mesh.cfv_iter(fh); fv_it.is_valid(); ++fv_it) {
                MyMesh::VertexHandle vertex = *fv_it;
                if (vertex == vh1 || vertex == vh2)
                {
                    isLastLODEdgeVertices[vertex.idx()] = true;
                }
            }
        }
    }
    //triangleGraph.init(mesh.n_faces());
    for (const MyMesh::EdgeHandle& edge : mesh.edges()) {
        // Do something with the edge...
        MyMesh::HalfedgeHandle heh = mesh.halfedge_handle(edge, 0);
        MyMesh::FaceHandle fh = mesh.face_handle(heh);
        MyMesh::FaceHandle fh2 = mesh.opposite_face_handle(heh);
        if (fh.idx() < 0 || fh2.idx() < 0) continue;
        auto clusterGroupIdx1 = mesh.property(clusterGroupIndexPropHandle, heh) - 1;
        auto clusterGroupIdx2 = mesh.property(clusterGroupIndexPropHandle, mesh.opposite_halfedge_handle(heh)) - 1;
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
    
    int clusterSize = std::min(targetClusterSize, triangleMetisGraph.nvtxs); // how many triangles does each cluster contain
    clusterNum = triangleMetisGraph.nvtxs / clusterSize; // target cluster num after partition
    if (clusterNum == 1) 
    {
    
    }
    // Set fixed target cluster size
    real_t* tpwgts = (real_t*)malloc(ncon * clusterNum * sizeof(real_t)); // We need to set a weight for each partition
    float sum = 0;
    for (idx_t i = 0; i < clusterNum; ++i) {
        tpwgts[i] = static_cast<float>(clusterSize) / triangleMetisGraph.nvtxs; // 
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
    for (int i = 0; i < triangleIndicesSortedByClusterIdx.size(); ++i)
    {
        auto triangleIndex = triangleIndicesSortedByClusterIdx[i];
        auto face = mesh.face_handle(triangleIndex);
		auto fv_it = mesh.fv_iter(face);
		triangleVertexIndicesSortedByClusterIdx[i * 3] = fv_it->idx();
        ++fv_it;
        triangleVertexIndicesSortedByClusterIdx[i * 3 + 1] = fv_it->idx();
        ++fv_it;
        triangleVertexIndicesSortedByClusterIdx[i * 3 + 2] = fv_it->idx();
	}
    clusters.resize(clusterNum);
    for (MyMesh::FaceIter face_it = mesh.faces_begin(); face_it != mesh.faces_end(); ++face_it) {
        MyMesh::FaceHandle fh = *face_it;
        auto clusterIdx = triangleClusterIndex[fh.idx()];
        auto& cluster = clusters[clusterIdx];
        cluster.triangleIndices.push_back(fh.idx());
    }
}


void Mesh::buildClusterGraph() 
{
    // Add embedding
    //int embeddingSize = targetClusterGroupSize * (1 + (clusterNum + 1) / targetClusterGroupSize) - clusterNum;
    //clusterGraph.init(clusterNum + embeddingSize);
    int embeddedSize = (clusterNum + targetClusterGroupSize - 1) / targetClusterGroupSize * targetClusterGroupSize;
    clusterGraph.init(embeddedSize);

    for (const MyMesh::EdgeHandle& edge : mesh.edges()) {
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

void Mesh::simplifyMesh(MyMesh & mymesh)
{
    OpenMesh::Decimater::DecimaterT<MyMesh> decimater(mymesh);
    OpenMesh::Decimater::ModQuadricT<MyMesh>::Handle hModQuadric;
    decimater.add(hModQuadric);
    decimater.module(hModQuadric).set_max_err(FLT_MAX, false);

    decimater.initialize();

    size_t original_faces = mymesh.n_faces();
    std::cout << "NUM FACES BEFORE: " << original_faces << std::endl;
    const double percentage = 0.5;

    auto currTargetFaceNum = mymesh.n_faces();
    for (uint32_t i = 0; i < clusterGroups.size(); ++i)
    {
        currTargetFaceNum = mymesh.n_faces() - static_cast<uint32_t>(clusterGroups[i].localFaceNum) * (1.0f - percentage);
        MyMesh submesh;
        uint32_t faceCount = 0;
        for (const auto & heh: mymesh.halfedges())
        {
            auto clusterGroupIdx = mymesh.property(clusterGroupIndexPropHandle, heh)-1;
            if (clusterGroupIdx == i)
            {
                auto vh1 = mymesh.to_vertex_handle(heh);
                auto vh2 = mymesh.from_vertex_handle(heh);
                auto vh3 = mymesh.to_vertex_handle(mymesh.next_halfedge_handle(heh));
                std::vector<MyMesh::VertexHandle> face_vhandles;
                auto vh1_new = submesh.add_vertex(mymesh.point(vh1));
                auto vh2_new = submesh.add_vertex(mymesh.point(vh2));
                auto vh3_new = submesh.add_vertex(mymesh.point(vh3));
                submesh.add_face(vh1_new, vh2_new, vh3_new); 
                faceCount++;
                mymesh.status(vh1).set_selected(true);
                mymesh.status(vh2).set_selected(true);
                if (mymesh.is_boundary(mymesh.opposite_halfedge_handle(heh)) 
                    || (mymesh.property(clusterGroupIndexPropHandle, mymesh.opposite_halfedge_handle(heh)) - 1) != i)
                {
                    mymesh.status(vh1).set_locked(true);
                    mymesh.status(vh2).set_locked(true);
                }
            }
        }
#pragma region debug_writeSubMesh
        //{
        //    std::string output_filename = "submesh" + std::to_string(i) + ".obj";
        //    // Export the mesh to the specified file
        //    if (!OpenMesh::IO::write_mesh(submesh, output_filename)) {
        //        std::cerr << "Error exporting mesh to " << output_filename << std::endl;
        //    }
        //}
#pragma endregion

        std::cout << "Cluster group: " << i << " Face num: " << currTargetFaceNum << std::endl;

        auto n_collapses = decimater.decimate_to_faces(0, currTargetFaceNum, true);
        std::cout << "n_collapses: " << n_collapses << std::endl;
        mymesh.garbage_collection();
        //MyMesh submeshAfterDecimation;
        for (const auto & vh: mymesh.vertices())
        {
            mymesh.status(vh).set_selected(false);
        }
#pragma region debug_writeSubMeshAfterDecimation
        //for (const auto& heh : mymesh.halfedges())
        //{
        //    auto clusterGroupIdx = mymesh.property(clusterGroupIndexPropHandle, heh) - 1;
        //    auto clusterGroupIdx2 = mymesh.property(clusterGroupIndexPropHandle, mymesh.opposite_halfedge_handle(heh)) - 1;
        //    if (clusterGroupIdx == i)
        //    {
        //        auto vh1 = mymesh.to_vertex_handle(heh);
        //        auto vh2 = mymesh.from_vertex_handle(heh);
        //        auto vh3 = mymesh.to_vertex_handle(mymesh.next_halfedge_handle(heh));
        //        auto vh1_new = submeshAfterDecimation.add_vertex(mymesh.point(vh1));
        //        auto vh2_new = submeshAfterDecimation.add_vertex(mymesh.point(vh2));
        //        auto vh3_new = submeshAfterDecimation.add_vertex(mymesh.point(vh3));
        //        submeshAfterDecimation.add_face(vh1_new, vh2_new, vh3_new);
        //    }
        //}
        //{
        //    std::string output_filename = "submeshAfterDecimation" + std::to_string(i) + ".obj";
        //
        //    // Export the mesh to the specified file
        //    if (!OpenMesh::IO::write_mesh(submeshAfterDecimation, output_filename)) {
        //        std::cerr << "Error exporting mesh to " << output_filename << std::endl;
        //    }
        //}

#pragma endregion

        std::cout << "NUM FACES AFTER: " << mymesh.n_faces() << std::endl;
    }
    std::cout << "Curr cluster group num: " << clusterGroups.size() << std::endl;
    for (const auto& vh : mymesh.vertices())
    {
        mymesh.status(vh).set_selected(false);
        mymesh.status(vh).set_locked(false);
    }
    size_t target_faces = static_cast<size_t>(original_faces * percentage);

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
        for (auto & heh: mesh.halfedges())
        {
            if (mesh.is_boundary(heh)) continue;
            mesh.property(clusterGroupIndexPropHandle, heh) = 1;
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

    std::cout << std::count(clusterGroupIndex.begin(), clusterGroupIndex.end(), 0) << std::endl;

    // Find the boundary of each cluster group
    isEdgeVertices.resize(mesh.n_faces() * 3, false);
    std::vector<std::unordered_set<MyMesh::FaceHandle>> clusterGroupFaceHandles;
    clusterGroupFaceHandles.resize(clusterGroupNum);
    for (const MyMesh::EdgeHandle& edge : mesh.edges()) {
        MyMesh::HalfedgeHandle heh = mesh.halfedge_handle(edge, 0);
        MyMesh::FaceHandle fh = mesh.face_handle(heh);
        MyMesh::FaceHandle fh2 = mesh.opposite_face_handle(heh);
        if (mesh.is_boundary(mesh.opposite_halfedge_handle(heh)) && fh.idx() >= 0) {
            auto clusterIdx1 = triangleClusterIndex[fh.idx()];
            auto clusterGroupIdx1 = clusterGroupIndex[clusterIdx1];
            auto vh1 = mesh.to_vertex_handle(heh);
            auto vh2 = mesh.from_vertex_handle(heh);
            mesh.property(clusterGroupIndexPropHandle, heh) = clusterGroupIdx1+1;
            clusterGroupFaceHandles[clusterGroupIdx1].insert(fh);
            continue;
        }
        else if (mesh.is_boundary(heh) && fh2.idx() >= 0) {
            auto clusterIdx2 = triangleClusterIndex[fh2.idx()];
            auto clusterGroupIdx2 = clusterGroupIndex[clusterIdx2];
            auto vh1 = mesh.to_vertex_handle(mesh.opposite_halfedge_handle(heh));
            auto vh2 = mesh.from_vertex_handle(mesh.opposite_halfedge_handle(heh));
            mesh.property(clusterGroupIndexPropHandle, heh) = clusterGroupIdx2+1;
            clusterGroupFaceHandles[clusterGroupIdx2].insert(fh2);
            continue;
        }
        if (fh.idx() < 0 || fh2.idx() < 0) continue;
        auto clusterIdx1 = triangleClusterIndex[fh.idx()];
        auto clusterIdx2 = triangleClusterIndex[fh2.idx()];
        auto clusterGroupIdx1 = clusterGroupIndex[clusterIdx1];
        auto clusterGroupIdx2 = clusterGroupIndex[clusterIdx2];
        clusterGroupFaceHandles[clusterGroupIdx1].insert(fh);
        clusterGroupFaceHandles[clusterGroupIdx2].insert(fh2);
        mesh.property(clusterGroupIndexPropHandle, heh) = clusterGroupIdx1+1;
        mesh.property(clusterGroupIndexPropHandle, mesh.opposite_halfedge_handle(heh)) = clusterGroupIdx2+1;
    }

    for (size_t i = 0; i < clusterGroupNum; i++)
    {
        clusterGroups[i].localFaceNum = clusterGroupFaceHandles[i].size();
    }

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
            //// Skip coloring clusterGroupGraph for now, it requires extra work. Modulo seems fine for graph coloring
            v.weight0 = glm::vec4(nodeColors[clusterGroupId % nodeColors.size()], clusterGroupId);
            //v.weight0 = glm::vec4(nodeColors[face.idx() % nodeColors.size()], clusterGroupId);
            vertexBuffer.push_back(v);
        }
    }
}


void Mesh::initUniqueVertexBuffer() {
    for (const auto & vertex: mesh.vertices())
    {
        vkglTF::Vertex v;
        v.pos = glm::vec3(mesh.point(vertex)[0], mesh.point(vertex)[1], mesh.point(vertex)[2]);
        v.normal = glm::vec3(mesh.normal(vertex)[0], mesh.normal(vertex)[1], mesh.normal(vertex)[2]);
        v.uv = glm::vec2(mesh.texcoord2D(vertex)[0], mesh.texcoord2D(vertex)[1]);
        v.joint0 = glm::vec4(0.0f);
        v.weight0 = glm::vec4(0.0f);
        uniqueVertexBuffer.push_back(v);
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
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
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


void Mesh::createUniqueVertexBuffer(vks::VulkanDevice* device, VkQueue transferQueue) {
    size_t vertexBufferSize = uniqueVertexBuffer.size() * sizeof(vkglTF::Vertex);
    uniqueVertices.count = static_cast<uint32_t>(uniqueVertexBuffer.size());

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
        uniqueVertexBuffer.data()));

    // Create device local buffers
    // Vertex buffer
    VK_CHECK_RESULT(device->createBuffer(
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        vertexBufferSize,
        &uniqueVertices.buffer,
        &uniqueVertices.memory));

    // Copy from staging buffers
    VkCommandBuffer copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    VkBufferCopy copyRegion = {};

    copyRegion.size = vertexBufferSize;
    vkCmdCopyBuffer(copyCmd, vertexStaging.buffer, uniqueVertices.buffer, 1, &copyRegion);

    device->flushCommandBuffer(copyCmd, transferQueue, true);

    vkDestroyBuffer(device->logicalDevice, vertexStaging.buffer, nullptr);
    vkFreeMemory(device->logicalDevice, vertexStaging.memory, nullptr);
}

void Mesh::createSortedIndexBuffer(vks::VulkanDevice* device, VkQueue transferQueue)
{
    size_t indexBufferSize = triangleVertexIndicesSortedByClusterIdx.size() * sizeof(uint32_t);
    sortedIndices.count = static_cast<uint32_t>(triangleVertexIndicesSortedByClusterIdx.size());

    assert(indexBufferSize > 0);

    struct StagingBuffer {
        VkBuffer buffer;
        VkDeviceMemory memory;
    } indexStaging;

    // Create staging buffers
    // Vertex data
    VK_CHECK_RESULT(device->createBuffer(
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        indexBufferSize,
        &indexStaging.buffer,
        &indexStaging.memory,
        triangleVertexIndicesSortedByClusterIdx.data()));

    // Create device local buffers
    // Vertex buffer
    VK_CHECK_RESULT(device->createBuffer(
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        indexBufferSize,
        &sortedIndices.buffer,
        &sortedIndices.memory));

    // Copy from staging buffers
    VkCommandBuffer copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    VkBufferCopy copyRegion = {};

    copyRegion.size = indexBufferSize;
    vkCmdCopyBuffer(copyCmd, indexStaging.buffer, sortedIndices.buffer, 1, &copyRegion);

    device->flushCommandBuffer(copyCmd, transferQueue, true);

    vkDestroyBuffer(device->logicalDevice, indexStaging.buffer, nullptr);
    vkFreeMemory(device->logicalDevice, indexStaging.memory, nullptr);
}

void Mesh::draw(VkCommandBuffer commandBuffer, uint32_t renderFlags, VkPipelineLayout pipelineLayout, uint32_t bindImageSet) 
{
    const VkDeviceSize offsets[1] = { 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertices.buffer, offsets);
    vkCmdDraw(commandBuffer, vertices.count, 1, 0, 0);
}