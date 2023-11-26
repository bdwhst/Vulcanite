#pragma once
#include "VulkanglTFModel.h"
#include <OpenMesh/Core/IO/MeshIO.hh>
#include <OpenMesh/Core/Mesh/TriMesh_ArrayKernelT.hh>
#include <tinygltf/tiny_gltf.h>

#include <filesystem>

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

	/************ Build Info *************/
	void generateNaniteInfo();

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

	const char* filepath = nullptr;
	const char* cache_time_key = "cache_time";
};

void loadvkglTFModel(const vkglTF::Model& model, std::vector<NaniteMesh>& naniteMeshes);

void packNaniteMeshesToIndexBuffer(const std::vector<NaniteMesh>& naniteMeshes, std::vector<uint32_t>& indexBuffer);