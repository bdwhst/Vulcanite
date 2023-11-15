#pragma once
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <OpenMesh/Core/IO/MeshIO.hh>
#include <OpenMesh/Core/Mesh/TriMesh_ArrayKernelT.hh>
#include <OpenMesh/Tools/Decimater/DecimaterT.hh>
#include <OpenMesh/Tools/Decimater/ModQuadricT.hh>
#include <OpenMesh/Core/Geometry/QuadricT.hh>
#include <vulkan/vulkan.h>

#include "VulkanDevice.h"
#include "VulkanglTFModel.h"
#include "glm/glm.hpp"

#include "Graph.h"
#include "Cluster.h"
#include "ClusterGroup.h"
#include "utils.h"

struct MyTraits : public OpenMesh::DefaultTraits
{
	VertexAttributes(OpenMesh::Attributes::Normal |
		OpenMesh::Attributes::TexCoord2D);
};
typedef OpenMesh::TriMesh_ArrayKernelT<MyTraits> MyMesh;


struct Mesh {

public:
	//Mesh();
	//Mesh(const char* filename);
	void buildTriangleGraph();
	void generateCluster();

	void buildClusterGraph();
	void generateClusterGroup();

	void colorClusterGraph();
	void colorClusterGroupGraph();

	void lockClusterGroupBoundaries(MyMesh& mymesh);
	void simplifyMesh(MyMesh& mymesh);

	MyMesh mesh;
	glm::mat4 modelMatrix;

	std::vector<uint32_t> triangleIndicesSortedByClusterIdx; // face_idx sort by cluster
	std::vector<uint32_t> triangleVertexIndicesSortedByClusterIdx; // (vert1, vert2, vert3) sort by cluster

	Graph triangleGraph;
	int clusterNum;
	const int targetClusterSize = 31;
	std::vector<idx_t> triangleClusterIndex;
	std::unordered_map<int, int> clusterColorAssignment;
	std::vector<Cluster> clusters;

	Graph clusterGraph;
	int clusterGroupNum;
	const int targetClusterGroupSize = 15;
	std::vector<idx_t> clusterGroupIndex;
	std::unordered_map<int, int> clusterGroupColorAssignment;
	std::vector<ClusterGroup> clusterGroups;

	const std::vector<glm::vec3> nodeColors =
	{
		glm::vec3(1.0f, 0.0f, 0.0f), // red
		glm::vec3(0.0f, 1.0f, 0.0f), // green
		glm::vec3(0.0f, 0.0f, 1.0f), // blue
		glm::vec3(1.0f, 1.0f, 0.0f), // yellow
		glm::vec3(1.0f, 0.0f, 1.0f), // purple
		glm::vec3(0.0f, 1.0f, 1.0f), // cyan
		glm::vec3(1.0f, 0.5f, 0.0f), // orange
		glm::vec3(0.5f, 1.0f, 0.0f), // lime
	};

	void initVertexBuffer();
	void createVertexBuffer(vks::VulkanDevice* device, VkQueue transferQueue);
	void draw(VkCommandBuffer commandBuffer, uint32_t renderFlags, VkPipelineLayout pipelineLayout, uint32_t bindImageSet);
	std::vector<glm::vec3> positions;

	vks::VulkanDevice* device;
	const vkglTF::Model* model;
	vkglTF::Model::Vertices vertices;
	vkglTF::Model::Indices indices;
	std::vector<uint32_t> indexBuffer;
	std::vector<vkglTF::Vertex> vertexBuffer;
	std::vector<vkglTF::Primitive> primitives;
};