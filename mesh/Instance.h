#pragma once
#include "NaniteMesh.h"
#include "Mesh.h"
#include "glm/glm.hpp"
#include "Cluster.h"

//ClusterInfo for drawing
struct ClusterInfo {
    alignas(16) glm::vec3 pMinWorld = glm::vec3(FLT_MAX);
    alignas(16) glm::vec3 pMaxWorld = glm::vec3(-FLT_MAX);
	// [triangleIndicesStart, triangleIndicesEnd) is the range of triangleIndicesSortedByClusterIdx
	// left close, right open
    alignas(4) uint32_t triangleIndicesStart; // Used to index Mesh::triangleIndicesSortedByClusterIdx
    alignas(4) uint32_t triangleIndicesEnd; // Used to index Mesh::triangleIndicesSortedByClusterIdx

	void mergeAABB(const glm::vec3& pMinOther, const glm::vec3& pMaxOther) {
		pMinWorld = glm::min(pMinWorld, pMinOther);
		pMaxWorld = glm::max(pMaxWorld, pMaxOther);
	};
};

struct Instance {
	NaniteMesh* referenceMesh;
	glm::mat4 rootTransform;
	std::vector<ClusterInfo> clusterInfo;
    //TODO: avoid duplication when there are multiple instances of the same model
    vkglTF::Model::Vertices vertices;
    vkglTF::Model::Indices indices;
    Instance(){}
    Instance(NaniteMesh* mesh, const glm::mat4 model):referenceMesh(mesh), rootTransform(model){}
    void createBuffersForNaniteLODs(vks::VulkanDevice* device, VkQueue transferQueue)
    {
        std::vector<vkglTF::Vertex> vertexBuffer;
        std::vector<uint32_t> indexBuffer;
        size_t totalNumVertices = 0;
        size_t totalNumIndices = 0;
        for (int i = 0; i < referenceMesh->meshes.size(); i++)
        {
            assert(referenceMesh->meshes[i].uniqueVertexBuffer.size() > 0);
            totalNumVertices += referenceMesh->meshes[i].uniqueVertexBuffer.size();
            assert(referenceMesh->meshes[i].triangleVertexIndicesSortedByClusterIdx.size() > 0);
            totalNumIndices += referenceMesh->meshes[i].triangleVertexIndicesSortedByClusterIdx.size();
        }
        vertexBuffer.resize(totalNumVertices);
        indexBuffer.resize(totalNumIndices);
        for (int i = 0; i < referenceMesh->meshes.size(); i++)
        {
            for (auto& vert : referenceMesh->meshes[i].uniqueVertexBuffer)
            {
                vertexBuffer.emplace_back(vert);
            }
            for (auto& index : referenceMesh->meshes[i].triangleVertexIndicesSortedByClusterIdx)
            {
                indexBuffer.emplace_back(index);
            }
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
	void buildClusterInfo()
	{
        // Init Clusters
        size_t totalClusterNum = 0;
        for (int i = 0; i < referenceMesh->meshes.size(); i++)
        {
            totalClusterNum += referenceMesh->meshes[i].clusterNum;
        }
        clusterInfo.resize(totalClusterNum);
        size_t currClusterNum = 0, currTriangleNum = 0;
        for (int i = 0; i < referenceMesh->meshes.size(); i++)
        {
            auto& mesh = referenceMesh->meshes[i].mesh;
            for (MyMesh::FaceIter face_it = mesh.faces_begin(); face_it != mesh.faces_end(); ++face_it) {
                MyMesh::FaceHandle fh = *face_it;
                auto clusterIdx = referenceMesh->meshes[i].triangleClusterIndex[fh.idx()] + currClusterNum;
                auto& clusterI = clusterInfo[clusterIdx];

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

                p0 = glm::vec3(rootTransform * glm::vec4(p0, 1.0f));
                p1 = glm::vec3(rootTransform * glm::vec4(p1, 1.0f));
                p2 = glm::vec3(rootTransform * glm::vec4(p2, 1.0f));

                getTriangleAABB(p0, p1, p2, pMinWorld, pMaxWorld);

                clusterI.mergeAABB(pMinWorld, pMaxWorld);
            }


            uint32_t currClusterIdx = -1;
            for (size_t j = 0; j < referenceMesh->meshes[i].triangleIndicesSortedByClusterIdx.size(); j++)
            {
                auto currTriangleIndex = referenceMesh->meshes[i].triangleIndicesSortedByClusterIdx[j];
                if (referenceMesh->meshes[i].triangleClusterIndex[currTriangleIndex] != currClusterIdx)
                {
                    if (currClusterIdx != -1) {
                        //std::cout << "Cluster " << currClusterIdx << " end at " << j << std::endl;
                        clusterInfo[currClusterIdx + currClusterNum].triangleIndicesEnd = j + currTriangleNum;
                    }
                    currClusterIdx = referenceMesh->meshes[i].triangleClusterIndex[currTriangleIndex];
                    clusterInfo[currClusterIdx + currClusterNum].triangleIndicesStart = j + currTriangleNum;
                    //std::cout << "Cluster " << currClusterIdx << " start at " << j << std::endl;
                }
            }
            clusterInfo[currClusterIdx + currClusterNum].triangleIndicesEnd = referenceMesh->meshes[i].triangleIndicesSortedByClusterIdx.size() + currTriangleNum;
            currClusterNum += referenceMesh->meshes[i].clusterNum;
            currTriangleNum += referenceMesh->meshes[i].triangleClusterIndex.size();
        }
        
	}

};