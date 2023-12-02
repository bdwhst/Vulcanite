#include "Instance.h"

class NaniteScene {
public:
	std::vector<Instance> naniteObjects;
	vkglTF::Model::Vertices vertices;
	vkglTF::Model::Indices indices;

	std::vector<ClusterInfo> clusterInfo;
	std::vector<ErrorInfo> errorInfo;

	void createBuffersForNaniteObjects(vks::VulkanDevice* device, VkQueue transferQueue);
	//void buildClusterInfo();
};