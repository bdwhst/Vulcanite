#include "NaniteScene.h"

void NaniteScene::createVertexIndexBuffer(vks::VulkanDevice* device, VkQueue transferQueue)
{
    std::vector<vkglTF::Vertex> vertexBuffer;
    std::vector<uint32_t> indexBuffer;

    int indexOffset = 0;
    indexOffsets.resize(naniteMeshes.size());
    indexCounts.resize(naniteMeshes.size());

    for (int i = 0; i < naniteMeshes.size(); ++i) {
        auto& naniteMesh = naniteMeshes[i];
        // Init vertex & index buffer
        auto& instance = Instance(&naniteMesh, glm::mat4(1.0f));
        instance.initBufferForNaniteLODs();
        vertexBuffer.insert(vertexBuffer.end(), instance.vertexBuffer.begin(), instance.vertexBuffer.end());
        for (auto index : instance.indexBuffer) {
            index += indexOffset;
            indexBuffer.push_back(index);
        }
        indexOffsets[i] = indexOffset;
        indexCounts[i] = instance.indexBuffer.size();
        indexOffset += instance.vertexBuffer.size();
        maxLodLevelNum = glm::max(maxLodLevelNum, instance.referenceMesh->lodNums);
    }

    size_t vertexBufferSize = vertexBuffer.size() * sizeof(vkglTF::Vertex);
    vertices.count = static_cast<uint32_t>(vertexBuffer.size());

    struct StagingBuffer {
        VkBuffer buffer;
        VkDeviceMemory memory;
    } vertexStaging, indexStaging;

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

    size_t indexBufferSize = indexBuffer.size() * sizeof(uint32_t);
    indices.count = static_cast<uint32_t>(indexBuffer.size());


    // Create staging buffers
    // Index data
    VK_CHECK_RESULT(device->createBuffer(
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        indexBufferSize,
        &indexStaging.buffer,
        &indexStaging.memory,
        indexBuffer.data()));

    // Create device local buffers
    // Index buffer
    VK_CHECK_RESULT(device->createBuffer(
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        indexBufferSize,
        &indices.buffer,
        &indices.memory));

    // Copy from staging buffers
    copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    copyRegion = {};

    copyRegion.size = indexBufferSize;
    vkCmdCopyBuffer(copyCmd, indexStaging.buffer, indices.buffer, 1, &copyRegion);

    device->flushCommandBuffer(copyCmd, transferQueue, true);

    vkDestroyBuffer(device->logicalDevice, indexStaging.buffer, nullptr);
    vkFreeMemory(device->logicalDevice, indexStaging.memory, nullptr);
}


void NaniteScene::createClusterInfos(vks::VulkanDevice* device, VkQueue transferQueue)
{
    sceneIndicesCount = 0;
    clusterIndexOffsets.resize(naniteMeshes.size());
    clusterIndexCounts.resize(naniteMeshes.size());
    for (size_t i = 0; i < naniteMeshes.size(); i++)
    {
        auto& naniteMesh = naniteMeshes[i];
        auto& instance = Instance(&naniteMesh, glm::mat4(1.0f));
        naniteMesh.buildClusterInfo();
		clusterIndexOffsets[i] = clusterInfo.size();
		clusterInfo.insert(clusterInfo.end(), naniteMesh.clusterInfo.begin(), naniteMesh.clusterInfo.end());
        clusterIndexCounts[i] = clusterInfo.size();
        errorInfo.insert(errorInfo.end(), naniteMesh.errorInfo.begin(), naniteMesh.errorInfo.end());
    }
    ASSERT(clusterInfo.size() == errorInfo.size(), "clusterInfo.size() should be equal to errorInfo.size()");
    //for (size_t i = 0; i < clusterInfo.size(); i++)
        for (size_t i = 0; i < 200; i++)
    {
        std::cout << "i: " << i << std::endl;
        std::cout << "errorInfo[i].centerR: " << errorInfo[i].centerR.x << " " << errorInfo[i].centerR.y << " " << errorInfo[i].centerR.z << " " << errorInfo[i].centerR.w << std::endl;
        std::cout << "errorInfo[i].centerRP: " << errorInfo[i].centerRP.x << " " << errorInfo[i].centerRP.y << " " << errorInfo[i].centerRP.z << " " << errorInfo[i].centerRP.w << std::endl;
        std::cout << "errorInfo[i].errorWorld: " << errorInfo[i].errorWorld.x << " " << errorInfo[i].errorWorld.y << std::endl;
        ASSERT(abs(errorInfo[i].centerR.x) > FLT_EPSILON, "Invalid errorR");
    }
        std::cout << alignof(ClusterInfo) << std::endl;
        std::cout << sizeof(ClusterInfo) << std::endl;
    //clusterIndexOffsets.resize(naniteObjects.size());
    for (int i = 0; i < naniteObjects.size(); ++i) {
        auto& naniteObject = naniteObjects[i];
    //    naniteObject.buildClusterInfo();
        auto referenceMeshIndex = std::find(naniteMeshes.begin(), naniteMeshes.end(), *(naniteObject.referenceMesh)) - naniteMeshes.begin();
    //    clusterIndexOffsets[i] = clusterInfo.size();
    //    std::cout << "i " << i << " clusterIndexOffsets[i] " << clusterIndexOffsets[i] << std::endl;
    //    for (auto ci : naniteObject.clusterInfo)
    //    {
    //        // TODO: Should adjust this part when multiple meshes are imported
    //        //auto referenceMeshIndex = 0;
    //        if (referenceMeshIndex != 0) {
    //            ci.triangleIndicesStart += indexCounts[referenceMeshIndex-1] / 3;
    //            ci.triangleIndicesEnd += indexCounts[referenceMeshIndex-1] / 3;
    //        }
    //        ci.objectIdx = i;
    //        clusterInfo.push_back(ci);
    //    }
    //    errorInfo.insert(errorInfo.end(), naniteObject.errorInfo.begin(), naniteObject.errorInfo.end());
        sceneIndicesCount += indexCounts[referenceMeshIndex];
        maxClusterNum += clusterIndexCounts[referenceMeshIndex];
    //    maxClusterNums += naniteObject.referenceMesh->meshes[0].clusterNum;
    }
}

void NaniteScene::createBVHNodeInfos(vks::VulkanDevice* device, VkQueue transferQueue)
{
    // TODO: Modify this part for multi-mesh!
    for (size_t i = 0; i < naniteMeshes.size(); i++)
    {
        auto & naniteMesh = naniteMeshes[i];
        sortedClusterIndices.insert(sortedClusterIndices.end(), naniteMesh.sortedClusterIndices.begin(), naniteMesh.sortedClusterIndices.end());
    }
    std::cout << "sortedClusterIndices.size(): " << sortedClusterIndices.size() << std::endl;
    virtualRootNode = std::make_shared<NaniteBVHNode>();
    virtualRootNode->nodeStatus = VIRTUAL_NODE;
    //virtualRootNode->depth = 0;
    for (int i = 0; i < naniteObjects.size(); ++i) {
        auto& naniteObject = naniteObjects[i];
        naniteObject.reconstructBVH();
        naniteObject.rootNode->objectIdx = i;
        virtualRootNode->children.push_back(naniteObject.rootNode);
    }

    // We should:
    //  Only pack Non-virtual nodes
    //  Only store information that are useful for subsequent traversal
    //      aabb
    //      parent error
    //      children indices(glm::ivec4)
    //  Some meta information about bvh should also be stored (in a uniform buffer)
    //      Node count of each level (At least we should know the node count of level 0 to initiate traversal)


    std::vector<std::shared_ptr<NaniteBVHNode>> flattenedNonVirtualNodes;

    std::queue<std::shared_ptr<NaniteBVHNode>> nodeQueue;
    nodeQueue.push(virtualRootNode);

    uint32_t index = 0;
    // Do a two-pass tree bfs
    //      First pass update cepth & push non-virtual node into vectors
    //      Second pass transform Node to NodeInfo
    while (!nodeQueue.empty())
    {
        auto currNode = nodeQueue.front();
        ASSERT(currNode->nodeStatus != NaniteBVHNodeStatus::INVALID, "Invalid node!");
        nodeQueue.pop();
        if (currNode->nodeStatus != VIRTUAL_NODE) // push all non-virtual nodes into arrays 
        {
            flattenedNonVirtualNodes.push_back(currNode);
            currNode->index = flattenedNonVirtualNodes.size() - 1;
        }
        for (auto child: currNode->children)
        {
            //std::cout << currNode->depth << " " << currNode->objectIdx;
            if (currNode != virtualRootNode) {
                child->objectIdx = currNode->objectIdx;
            }
            //child->depth = currNode->depth + 1;
            nodeQueue.push(child);
        }
    }

    int clusterSize = clusterInfo.size();
    std::unordered_set<int> clusterIndexSets;
    std::cout << "clusterSize: " << clusterSize << std::endl;
    for (size_t i = 0; i < clusterSize; i++)
    {
        clusterIndexSets.insert(i);
    }
    bvhNodeInfos.resize(flattenedNonVirtualNodes.size());

    std::vector<uint32_t> depthLeafCounts;
    for (size_t i = 0; i < flattenedNonVirtualNodes.size(); i++)
    {
        auto& currNode = flattenedNonVirtualNodes[i];

        std::string indent(currNode->depth, '\t');
        //std::cout << indent 
        //    << (currNode->nodeStatus == NaniteBVHNodeStatus::LEAF ? "Leaf " : "Non-leaf ") 
        //    << currNode->depth << " " 
        //    << currNode->index << " " 
        //    << currNode->objectIdx << " "
        //    << std::endl;
        
        //ASSERT(currNode->nodeStatus == VIRTUAL_NODE && currNode->depth >= 0, "A non-virtual node should now be cut out from the bvh tree");
        //ASSERT(currNode->nodeStatus != VIRTUAL_NODE && currNode->depth < 0, "A virtual node should not be cut out from the bvh tree");
        
        auto & nodeInfo = bvhNodeInfos[i];
        nodeInfo.pMinWorld = currNode->pMin;
        nodeInfo.pMaxWorld = currNode->pMax;
        nodeInfo.objectId = currNode->objectIdx;
        nodeInfo.errorWorld.x = currNode->normalizedlodError;
        nodeInfo.errorWorld.y = currNode->parentNormalizedError;
        nodeInfo.errorRP = currNode->parentBoundingSphere;
        nodeInfo.clusterIntervals = glm::ivec2(currNode->start, currNode->end);
        //std::cout << indent << (currNode->nodeStatus == VIRTUAL_NODE ? "Virtual " : "Non-virtual ")
        //    << " pMin: " << nodeInfo.pMinWorld.x << " " << nodeInfo.pMinWorld.y << " " << nodeInfo.pMinWorld.z
        //    << " pMax: " << nodeInfo.pMaxWorld.x << " " << nodeInfo.pMaxWorld.y << " " << nodeInfo.pMaxWorld.z << std::endl;

        ASSERT(currNode->children.size() <= 4, "Invalid node!");
        for (size_t j = 0; j < currNode->children.size(); ++j)
        {
            auto child = currNode->children[j];
            nodeInfo.childrenNodeIndices[j] = child->index;
        }

        //nodeInfo.clusterIndices = currNode->clusterIndices;
        // TODO: Use memcpy
        //for (size_t i = 0; i < CLUSTER_GROUP_MAX_SIZE; i++)
        //{
        //    if (currNode->clusterIndices[i] >= 0) {
        //        //nodeInfo.clusterIndices[i] = currNode->clusterIndices[i] + clusterIndexOffsets[currNode->objectIdx];
        //        // TODO: Modify this part for multi-mesh!
        //        nodeInfo.clusterIndices[i] = currNode->clusterIndices[i];
        //    }
        //}
        //ASSERT(currNode->nodeStatus == LEAF || nodeInfo.clusterIndices[0] == -1, "A non-leaf node should have no cluster index");
        //std::cout << indent << "object: " << currNode->objectIdx
        //    << " lod: " << currNode->lodLevel
        //    << " depth: " << currNode->depth << std::endl;
        ASSERT(currNode->depth <= depthCounts.size(), "`depth` should never be over depthCounts.size()");
        if (currNode->depth == depthCounts.size())
        {
            depthCounts.push_back(1);
        }
        else
        {
            depthCounts[currNode->depth] += 1;
        }
        if (currNode->nodeStatus == LEAF)
        {
            ASSERT(currNode->clusterIndices.size() <= CLUSTER_GROUP_MAX_SIZE, "this leaf node stores too many cluster indices");
            int validClusterIndicesSize = 0;
            bool isValid = true;
            // Final check
            for (size_t i = 0; i < CLUSTER_GROUP_MAX_SIZE; i++)
            {
                if (currNode->clusterIndices[i] >= 0) {
                    ASSERT(isValid, "Invalid cluster indices"); // This assertion is to make sure that all cluster indices are allocated contiguously
					validClusterIndicesSize += nodeInfo.clusterIntervals[1] - nodeInfo.clusterIntervals[0];
					//std::cout << indent
                    //    << " cluster: " << nodeInfo.clusterIndices[i]
                    //    << std::endl;
                    //ASSERT(clusterIndexSets.find(nodeInfo.clusterIndices[i]) != clusterIndexSets.end(), "Already removed, repeat index!");
                    //clusterIndexSets.erase(nodeInfo.clusterIndices[i]);
				}
                else {
                    isValid = false;
                }
            }
            if (currNode->depth >= depthLeafCounts.size()) {
                depthLeafCounts.resize(currNode->depth + 1);
                depthLeafCounts[currNode->depth] = validClusterIndicesSize;
            }
            else {
                depthLeafCounts[currNode->depth] += validClusterIndicesSize;
            }
            //std::cout << indent << "Curr leaf node clusterIndices size: " << validClusterIndicesSize << std::endl;
        }
        //std::cout << indent << "nodeInfo.clusterIndices[0]: " << nodeInfo.clusterIndices[0] << std::endl;
        //for (size_t i = 0; i < CLUSTER_GROUP_MAX_SIZE; i++)
        //{
        //    std::cout << indent << "clusterIndex: " << currNode->clusterIndices[i] << std::endl;
        //    std::cout << indent << "clusterIndex + offset: " << nodeInfo.clusterIndices[i] << std::endl;
        //}
    }

    //ASSERT(clusterIndexSets.size() == 0, "Some clusters are not covered");
    for (size_t i = 0; i < depthCounts.size(); i++)
    {
        std::cout << "i: " << i << " depthCounts[i]: " << depthCounts[i] << std::endl;
        std::cout << "i: " << i << " depthLeafCounts[i]: " << depthLeafCounts[i] << std::endl;
    }
    std::cout << alignof(BVHNodeInfo) << std::endl;
    std::cout << sizeof(BVHNodeInfo) << std::endl;
    //ASSERT(0, "Stop here");
    maxDepthCounts = std::max_element(depthCounts.begin(), depthCounts.end())[0];
    std::cout << "maxDepthCounts: " << maxDepthCounts << std::endl;
    initNodeInfoIndices.resize(depthCounts[0] + 1); // Init node info indices by depth 0 node count
    initNodeInfoIndices[0] = depthCounts[0];
    for (size_t i = 1; i <= depthCounts[0]; i++)
    {
        initNodeInfoIndices[i] = i-1;
    }

    //for (size_t i = 0; i < flattenedNonVirtualNodes.size(); i++)
    //{
    //    auto& currNode = flattenedNonVirtualNodes[i];
    //    auto & nodeInfo = bvhNodeInfos[i];
    //    if (currNode->depth == depthCounts.size() - 1) {
    //        std::cout << "-----" << std::endl;
    //        std::cout << currNode->index << std::endl;
    //        std::cout << nodeInfo.childrenNodeIndices[0] << std::endl;
    //        std::cout << nodeInfo.childrenNodeIndices[1] << std::endl;
    //        std::cout << nodeInfo.childrenNodeIndices[2] << std::endl;
    //        std::cout << nodeInfo.childrenNodeIndices[3] << std::endl;
    //        std::cout << "-----" << std::endl;
    //        ASSERT(glm::all(glm::lessThan(nodeInfo.childrenNodeIndices, glm::ivec4(0))), "A leaf node should have no children");
    //        ASSERT(currNode->nodeStatus == LEAF, "A leaf node should have no children");
    //    }
    //}
}
