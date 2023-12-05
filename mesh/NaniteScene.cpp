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
    clusterIndexOffsets.resize(naniteObjects.size());
    for (int i = 0; i < naniteObjects.size(); ++i) {
        auto& naniteObject = naniteObjects[i];
        naniteObject.buildClusterInfo();
        auto referenceMeshIndex = std::find(naniteMeshes.begin(), naniteMeshes.end(), *(naniteObject.referenceMesh)) - naniteMeshes.begin();
        clusterIndexOffsets[i] = clusterInfo.size();
        for (auto ci : naniteObject.clusterInfo)
        {
            // TODO: Should adjust this part when multiple meshes are imported
            //auto referenceMeshIndex = 0;
            if (referenceMeshIndex != 0) {
                ci.triangleIndicesStart += indexCounts[referenceMeshIndex-1] / 3;
                ci.triangleIndicesEnd += indexCounts[referenceMeshIndex-1] / 3;
            }
            ci.objectIdx = i;
            clusterInfo.push_back(ci);
        }
        errorInfo.insert(errorInfo.end(), naniteObject.errorInfo.begin(), naniteObject.errorInfo.end());
        sceneIndicesCount += indexCounts[referenceMeshIndex];
    }
}

void NaniteScene::createBVHNodeInfos(vks::VulkanDevice* device, VkQueue transferQueue)
{

    virtualRootNode = std::make_shared<NaniteBVHNode>();
    virtualRootNode->nodeStatus = VIRTUAL_NODE;
    virtualRootNode->depth = 0;
    for (int i = 0; i < naniteObjects.size(); ++i) {
        auto& naniteObject = naniteObjects[i];
        naniteObject.reconstructBVH();
        naniteObject.rootNode->objectId = i;
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
            //std::cout << currNode->depth << " " << currNode->objectId;
            if (currNode != virtualRootNode) {
                child->objectId = currNode->objectId;
            }
            child->depth = currNode->depth + 1;
            nodeQueue.push(child);
        }
    }

    nodeInfos.resize(flattenedNonVirtualNodes.size());
    int depth = 0;
    std::vector<uint32_t> depthCounts;
    for (size_t i = 0; i < flattenedNonVirtualNodes.size(); i++)
    {
        auto& currNode = flattenedNonVirtualNodes[i];

        currNode->depth -= 2; // Because we will cut out all virtual nodes from now on
        std::string indent(currNode->depth, '\t');
        std::cout << indent 
            << (currNode->nodeStatus == NaniteBVHNodeStatus::LEAF ? "Leaf " : "Non-leaf ") 
            << currNode->depth << " " 
            << currNode->index << " " 
            << currNode->objectId << " "
            << std::endl;
        //ASSERT(currNode->nodeStatus == VIRTUAL_NODE && currNode->depth >= 0, "A non-virtual node should now be cut out from the bvh tree");
        //ASSERT(currNode->nodeStatus != VIRTUAL_NODE && currNode->depth < 0, "A virtual node should not be cut out from the bvh tree");
        
        auto & nodeInfo = nodeInfos[i];
        nodeInfo.pMinWorld = currNode->pMin;
        nodeInfo.pMaxWorld = currNode->pMax;
        nodeInfo.errorWorld.x = currNode->parentNormalizedError;
        nodeInfo.errorWorld.y = currNode->normalizedlodError;

        ASSERT(currNode->children.size() <= 4, "Invalid node!");
        for (size_t j = 0; j < currNode->children.size(); ++j)
        {
            auto child = currNode->children[j];
            nodeInfo.childrenNodeIndices[j] = child->index;
        }

        //nodeInfo.clusterIndices = currNode->clusterIndices;
        // TODO: Use memcpy
        for (size_t i = 0; i < CLUSTER_GROUP_MAX_SIZE; i++)
        {
            nodeInfo.clusterIndices[i] = currNode->clusterIndices[i];
        }
        if (currNode->nodeStatus == LEAF)
        {
            ASSERT(currNode->clusterIndices.size() <= CLUSTER_GROUP_MAX_SIZE, "this leaf node stores too many cluster indices");
            std::cout << indent << "Curr leaf node clusterIndices size: " << currNode->clusterIndices.size() << std::endl;
        }
        ASSERT(currNode->depth <= depthCounts.size(), "`depth` should never be over depthCounts.size()");
        if (currNode->depth == depthCounts.size())
        {
            depthCounts.push_back(1);
        }
        else 
        {
            depthCounts[depth] += 1;
        }
    }
}
