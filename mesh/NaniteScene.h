#pragma once

#include <map>

#include "Instance.h"

class NaniteScene {
public:
	std::vector<Instance> naniteObjects;
	std::vector<NaniteMesh> naniteMeshes;
	std::map<std::string, uint32_t> naniteMeshSceneIndices;
	std::vector<uint32_t> indexOffsets;
	std::vector<uint32_t> indexCounts;

	vkglTF::Model::Vertices vertices;
	vkglTF::Model::Indices indices;

	std::shared_ptr<NaniteBVHNode> virtualRootNode; // The root node that connects all instances' root nodes
	std::vector<BVHNodeInfo> bvhNodeInfos; // cleaned version
	std::vector<uint32_t> clusterIndexOffsets; 
	std::vector<uint32_t> depthCounts;
	std::vector<uint32_t> depthLeafCounts; // Just for stats, not in usage
	std::vector<uint32_t> initNodeInfoIndices;
	uint32_t maxDepthCounts = 0;

	std::vector<uint32_t> clusterIndexCounts;
	uint32_t maxClusterNum = 0;

	std::vector<uint32_t> sortedClusterIndices;

	uint32_t maxLodLevelNum = -1;

	uint32_t sceneIndicesCount = 0;

	std::vector<ClusterInfo> clusterInfo;
	std::vector<ErrorInfo> errorInfo;

	void addNaniteMesh(const std::string & meshName, NaniteMesh& naniteMesh) {
		//naniteMeshes[meshName] = naniteMesh;
	};

	Instance& addInstance(const std::string & meshName, glm::mat4 transform) {
		//Instance instance(&naniteMeshes[meshName], transform);
		//naniteObjects.push_back(instance);
		//return naniteObjects.back();
	}

	void createNaniteSceneInfo(vks::VulkanDevice* device, VkQueue transferQueue);
	void createVertexIndexBuffer(vks::VulkanDevice* device, VkQueue transferQueue);
	void createClusterInfos(vks::VulkanDevice* device, VkQueue transferQueue);
	void createBVHNodeInfos(vks::VulkanDevice* device, VkQueue transferQueue);
};