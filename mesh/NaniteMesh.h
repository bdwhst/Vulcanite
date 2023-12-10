#pragma once
#include "VulkanglTFModel.h"
#include <OpenMesh/Core/IO/MeshIO.hh>
#include <OpenMesh/Core/Mesh/TriMesh_ArrayKernelT.hh>
#include <tinygltf/tiny_gltf.h>

#include <filesystem>

#include "Common.h"
#include "Mesh.h"

struct NaniteMesh {
	uint32_t lodNums = 0;
	glm::mat4 modelMatrix;
	std::vector<Mesh> meshes;
	OpenMesh::HPropHandleT<int32_t> clusterGroupIndexPropHandle;

	/************ Load Mesh *************/
	// Load from vkglTF
	const vkglTF::Model * vkglTFModel;
	const vkglTF::Mesh* vkglTFMesh;
	void setModelPath(const char* path) { filepath = path; };
	void loadvkglTFModel(const vkglTF::Model& model);
	void vkglTFMeshToOpenMesh(MyMesh& mymesh, const vkglTF::Mesh& mesh);
	void vkglTFPrimitiveToOpenMesh(MyMesh& mymesh, const vkglTF::Primitive& prim);
	// TODO: Load from tinygltf
	const tinygltf::Model* tinyglTFModel;
	const tinygltf::Mesh* tinyglTFMesh;
	void loadglTFModel(const tinygltf::Model& model); // TODO
	void glTFMeshToOpenMesh(MyMesh& mymesh, const tinygltf::Mesh& mesh); // TODO

	/************ Process DAG *************/
	std::vector<ClusterNode> flattenedClusterNodes;
	void flattenDAG();

	/************ Flatten BVH *************/
	std::shared_ptr<NaniteBVHNode> virtualBVHRootNode;
	std::vector<NaniteBVHNodeInfo> flattenedBVHNodeInfos;
	void flattenBVH();

	/************ Build Info *************/
	void generateNaniteInfo();

	std::vector<ClusterInfo> clusterInfo;
	std::vector<ErrorInfo> errorInfo;
	std::vector<uint32_t> sortedClusterIndices;
	void buildClusterInfo();

	/************ Serialization *************/
	void serialize(const std::string& filepath);
	void deserialize(const std::string& filepath);


	void initNaniteInfo(const std::string& filepath, bool useCache = true);
	/*
		What data structure should be used to store 
		1. lods
		2. cluster of each lods
		3. cluster group (no need)
	
	*/
	vks::VulkanDevice* device;
	const vkglTF::Model * model;
	vkglTF::Model::Vertices vertices;
	vkglTF::Model::Indices indices;
	std::vector<uint32_t> indexBuffer;
	std::vector<vkglTF::Vertex> vertexBuffer;
	std::vector<vkglTF::Primitive> primitives;
	std::vector<uint32_t> clusterIndexOffset; // This offset is caused by different LODs

	const char* filepath = nullptr;
	const char* cache_time_key = "cache_time";

	std::vector<Mesh> debugMeshes;
	void checkDeserializationResult(const std::string& filepath);

	bool operator==(const NaniteMesh & other) const {
		if (meshes.size() != other.meshes.size()) return false;
		for (int i = 0; i < meshes.size(); i++)
		{
			if (meshes[i].mesh.n_vertices() != other.meshes[i].mesh.n_vertices()) return false;
			if (meshes[i].mesh.n_faces() != other.meshes[i].mesh.n_faces()) return false;
		}
		return true;
	}
};

void loadvkglTFModel(const vkglTF::Model& model, std::vector<NaniteMesh>& naniteMeshes);

void packNaniteMeshesToIndexBuffer(const std::vector<NaniteMesh>& naniteMeshes, std::vector<uint32_t>& indexBuffer);