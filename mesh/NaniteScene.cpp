#include "NaniteScene.h"

void NaniteScene::createBuffersForNaniteObjects(vks::VulkanDevice* device, VkQueue transferQueue)
{
    std::vector<vkglTF::Vertex> vertexBuffer;
    std::vector<uint32_t> indexBuffer;

    size_t totalVertexCount = 0;
    size_t totalIndexCount = 0;

	for (auto& naniteObject : naniteObjects) {
		naniteObject.initBufferForNaniteLODs();
        totalVertexCount += naniteObject.vertexBuffer.size();
        totalIndexCount += naniteObject.indexBuffer.size();
        //naniteObject.createBuffersForNaniteLODs(device, transferQueue);
		naniteObject.buildClusterInfo();
        errorInfo.insert(errorInfo.end(), naniteObject.errorInfo.begin(), naniteObject.errorInfo.end());
	}

    vertexBuffer.reserve(totalVertexCount);
    indexBuffer.reserve(totalIndexCount);
    int indexOffset = 0;
    for (int i = 0; i < naniteObjects.size(); ++i) {
        const auto & naniteObject = naniteObjects[i];
        vertexBuffer.insert(vertexBuffer.end(), naniteObject.vertexBuffer.begin(), naniteObject.vertexBuffer.end());
        for (auto index : naniteObject.indexBuffer) {
			//index += indexOffset;
            indexBuffer.push_back(index);
		}

        for (auto ci: naniteObject.clusterInfo )
        {
            //ci.triangleIndicesStart += indexOffset;
            //ci.triangleIndicesEnd += indexOffset;
            ci.objectIdx = i;
            clusterInfo.push_back(ci);
        }

		indexOffset += naniteObject.indexBuffer.size();
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

// void NaniteScene::buildClusterInfo()
// {
// 
//     for (auto& naniteObject : naniteObjects) {
//         naniteObject.initBufferForNaniteLODs();
//         //naniteObject.createBuffersForNaniteLODs(device, transferQueue);
//         naniteObject.buildClusterInfo();
//         errorInfo.insert(errorInfo.end(), naniteObject.errorInfo.begin(), naniteObject.errorInfo.end());
//     }
// 
//     int indexOffset = 0;
//     for (uint i = 0; i < naniteObjects.size();  i++) {
//         auto& naniteObject = naniteObjects[i];
//         for (auto index : naniteObject.indexBuffer) {
//             index += indexOffset;
//         }
// 
//         for (auto ci : naniteObject.clusterInfo)
//         {
//             ci.triangleIndicesStart += indexOffset;
//             ci.triangleIndicesEnd += indexOffset;
//             ci.objectIdx = 1;
//             clusterInfo.push_back(ci);
//         }
// 
//         indexOffset += naniteObject.indexBuffer.size();
//     }
// }
