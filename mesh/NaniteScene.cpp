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
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
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
    for (int i = 0; i < naniteObjects.size(); ++i) {
        auto& naniteObject = naniteObjects[i];
        naniteObject.buildClusterInfo();
        auto referenceMeshIndex = std::find(naniteMeshes.begin(), naniteMeshes.end(), *(naniteObject.referenceMesh)) - naniteMeshes.begin();
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
        visibleIndicesCount += naniteObject.referenceMesh->meshes[0].triangleVertexIndicesSortedByClusterIdx.size();
    }
}
