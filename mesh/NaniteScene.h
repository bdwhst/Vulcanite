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

	void createVertexIndexBuffer(vks::VulkanDevice* device, VkQueue transferQueue);
	void createClusterInfos(vks::VulkanDevice* device, VkQueue transferQueue);
	//void buildClusterInfo();
};