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

	void simplifyMesh();

	MyMesh mesh;

	Graph triangleGraph;
	int clusterNum;
	const int targetClusterSize = 31;
	std::vector<idx_t> triangleClusterIndex;
	std::unordered_map<int, int> clusterColorAssignment;
	std::vector<Cluster> clusters;

	Graph clusterGraph;
	int clusterGroupNum;
	const int targetClusterGroupSize = 16;
	std::vector<idx_t> clusterGroupIndex;
	std::unordered_map<int, int> clusterGroupColorAssignment;
	std::vector<glm::vec3> clusterGroupColor;
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

	void initVertexBuffer()
	{
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
				// Skip coloring clusterGroupGraph for now, it requires extra work. Modulo seems fine for graph coloring
				v.weight0 = glm::vec4(nodeColors[clusterGroupId % nodeColors.size()], clusterGroupId);
				vertexBuffer.push_back(v);
			}
		}
	}
	void createVertexBuffer(vks::VulkanDevice* device, VkQueue transferQueue);
	void draw(VkCommandBuffer commandBuffer, uint32_t renderFlags, VkPipelineLayout pipelineLayout, uint32_t bindImageSet);
private:
	std::vector<glm::vec3> positions;

	vks::VulkanDevice* device;
	const vkglTF::Model* model;
	vkglTF::Model::Vertices vertices;
	vkglTF::Model::Indices indices;
	std::vector<uint32_t> indexBuffer;
	std::vector<vkglTF::Vertex> vertexBuffer;
	std::vector<vkglTF::Primitive> primitives;
};