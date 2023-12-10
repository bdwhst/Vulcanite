#pragma once

#include <json/json.hpp>

/*
* About flattening BVH Nodes
*	We need to flatten BVH Nodes two times
*		1. First time is when we build Nanite info before rendering. To do serialization, 
*		we need to flatten the BVH of each `NaniteMesh`.
*		2. Second time is when we are creating useful information to do culling before rendering,
*		this will be implemented in `NaniteScene`. Namely, we need to gather all BVHs of instances
*		in current scene. Connect all the root nodes of these BVHs to a virtual root node, and then
*		flatten this huge BVH into a new array.
*/

enum NaniteBVHNodeStatus 
{
	INVALID, // Default initialization
	VIRTUAL_NODE, // All nodes that do not have a bounding box, only for implementation convenience
	NODE, // All valid nodes
	LEAF  // All nodes that actually stores cluster indices
};

/*
	Stores the **pointers** to all children nodes
	Tree structure
*/

struct NaniteBVHNode
{
	NaniteBVHNode():
		normalizedlodError(-FLT_MAX),
		parentNormalizedError(-FLT_MAX),
		index(-1),
		pMin(glm::vec3(FLT_MAX)),
		pMax(glm::vec3(-FLT_MAX)), 
		nodeStatus(INVALID),
		start(-1),
		end(-1),
		depth(0)
		{
			objectIdx = -1; // Only useful in instancing
			lodLevel = -1;
			for (size_t i = 0; i < CLUSTER_GROUP_MAX_SIZE; i++) clusterIndices[i] = -1; // The non-default initialization
		}
	std::vector<std::shared_ptr<NaniteBVHNode>> children; // should be a fixed size
	double normalizedlodError = -FLT_MAX;
	double parentNormalizedError = -FLT_MAX;
	glm::vec4 parentBoundingSphere;
	int index = -1;
	glm::vec3 pMin = glm::vec3(FLT_MAX);
	glm::vec3 pMax = glm::vec3(-FLT_MAX);
	NaniteBVHNodeStatus nodeStatus = NaniteBVHNodeStatus::INVALID;
	uint32_t start = -1;
	uint32_t end = -1;
	uint32_t depth = 0;
	
	// Only leaf node will have `clusterIndices` filled
	//std::vector<uint32_t> clusterIndices; 
	std::array<int, CLUSTER_GROUP_MAX_SIZE> clusterIndices; // should try to assert index overflow
	
	int objectIdx = -1; // Only useful in instancing
	int lodLevel = -1; 
};

/*
	Stores the **indices** to all children nodes
	Array structure
*/

struct NaniteBVHNodeInfo
{
	double normalizedlodError = -FLT_MAX;
	double parentNormalizedError = -FLT_MAX;
	glm::vec4 parentBoundingSphere;
	int index = -1;
	glm::vec3 pMin = glm::vec3(FLT_MAX);
	glm::vec3 pMax = glm::vec3(-FLT_MAX);
	std::vector<int> children;
	//glm::ivec4 children = glm::ivec4(-1);
	std::array<int, CLUSTER_GROUP_MAX_SIZE> clusterIndices; // should try to assert index overflow
	int start = -1;
	int end = -1;
	NaniteBVHNodeStatus nodeStatus = NaniteBVHNodeStatus::INVALID;
	uint32_t depth = 0;
	int lodLevel = -1;

	json toJson() {
		return {
			{"normalizedlodError", normalizedlodError},
			{"parentNormalizedError", parentNormalizedError},
			{"parentBoundingSphere", {parentBoundingSphere.x, parentBoundingSphere.y, parentBoundingSphere.z, parentBoundingSphere.w}},
			{"index", index},
			{"pMin", {pMin.x, pMin.y, pMin.z}},
			{"pMax", {pMax.x, pMax.y, pMax.z}},
			{"children", children},
			{"clusterIndices", clusterIndices},
			{"start", start},
			{"end", end},
			{"nodeStatus", nodeStatus},
			{"depth", depth},
			{"lodLevel", lodLevel}
		};
	}

	void fromJson(const json& j) 
	{
		ASSERT(j.find("normalizedlodError") != j.end(), "normalizedlodError not found");
		normalizedlodError = j["normalizedlodError"].get<double>();

		ASSERT(j.find("parentNormalizedError") != j.end(), "parentNormalizedError not found");
		parentNormalizedError = j["parentNormalizedError"].get<double>();

		ASSERT(j.find("parentBoundingSphere") != j.end() && j["parentBoundingSphere"].is_array() && j["parentBoundingSphere"].size() == 4,
			"parentBoundingSphere not found or not properly set");
		if (j.find("parentBoundingSphere") != j.end() && j["parentBoundingSphere"].is_array() && j["parentBoundingSphere"].size() == 4) {
			parentBoundingSphere.x = j["parentBoundingSphere"][0].get<float>();
			parentBoundingSphere.y = j["parentBoundingSphere"][1].get<float>();
			parentBoundingSphere.z = j["parentBoundingSphere"][2].get<float>();
			parentBoundingSphere.w = j["parentBoundingSphere"][3].get<float>();
		}

		ASSERT(j.find("index") != j.end(), "index not found");
		index = j["index"].get<int>();

		ASSERT(j.find("pMin") != j.end() && j["pMin"].is_array() && j["pMin"].size() == 3,
			"pMin not found or not properly set");
		if (j.find("pMin") != j.end() && j["pMin"].is_array() && j["pMin"].size() == 3) {
			pMin.x = j["pMin"][0].get<float>();
			pMin.y = j["pMin"][1].get<float>();
			pMin.z = j["pMin"][2].get<float>();
		}

		ASSERT(j.find("pMax") != j.end() && j["pMax"].is_array() && j["pMax"].size() == 3,
			"pMax not found or not properly set");
		if (j.find("pMax") != j.end() && j["pMax"].is_array() && j["pMax"].size() == 3) {
			pMax.x = j["pMax"][0].get<float>();
			pMax.y = j["pMax"][1].get<float>();
			pMax.z = j["pMax"][2].get<float>();
		}

		ASSERT(j.find("children") != j.end() && j["children"].is_array(), "children not found");
		children.resize(j["children"].size());
		for (int i = 0; i < j["children"].size(); ++i) {
			children[i] = j["children"].at(i).get<uint32_t>();
		}

		ASSERT(j.find("start") != j.end(), "start not found");
		start = j["start"].get<int>();

		ASSERT(j.find("end") != j.end(), "end not found");
		end = j["end"].get<int>();

		ASSERT(j.find("clusterIndices") != j.end() && j["clusterIndices"].is_array(), "clusterIndices not found");
		for (int i = 0; i < CLUSTER_GROUP_MAX_SIZE; ++i) {
			clusterIndices[i] = j["clusterIndices"].at(i).get<uint32_t>();
		}

		ASSERT(j.find("nodeStatus") != j.end(), "nodeStatus not found");
		nodeStatus = j["nodeStatus"].get<NaniteBVHNodeStatus>();

		ASSERT(j.find("depth") != j.end(), "depth not found");
		depth = j["depth"].get<uint32_t>();

		ASSERT(j.find("lodLevel") != j.end(), "lodLevel not found");
		lodLevel = j["lodLevel"].get<int>();
	}
};


/*
	Stores the **indices** of all children nodes
	Array structure
	Use this struct to do bvh traversal on GPU, so only stores useful information
*/
struct BVHNode 
{
	
};