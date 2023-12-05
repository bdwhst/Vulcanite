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
	std::vector<std::shared_ptr<NaniteBVHNode>> children; // should be a fixed size
	double normalizedlodError = -FLT_MAX;
	double parentNormalizedError = -FLT_MAX;
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
	
	uint32_t objectId = -1; // Only useful in instancing
};

/*
	Stores the **indices** to all children nodes
	Array structure
*/

struct NaniteBVHNodeInfo
{
	double normalizedlodError = -FLT_MAX;
	double parentNormalizedError = -FLT_MAX;
	int index = -1;
	glm::vec3 pMin = glm::vec3(FLT_MAX);
	glm::vec3 pMax = glm::vec3(-FLT_MAX);
	std::vector<int> children;
	//glm::ivec4 children = glm::ivec4(-1);
	std::array<int, CLUSTER_GROUP_MAX_SIZE> clusterIndices; // should try to assert index overflow
	NaniteBVHNodeStatus nodeStatus = NaniteBVHNodeStatus::INVALID;
	uint32_t depth = 0;

	json toJson() {
		return {
			{"normalizedlodError", normalizedlodError},
			{"parentNormalizedError", parentNormalizedError},
			{"index", index},
			{"pMin", {pMin.x, pMin.y, pMin.z}},
			{"pMax", {pMax.x, pMax.y, pMax.z}},
			{"children", children},
			{"clusterIndices", clusterIndices},
			{"nodeStatus", nodeStatus},
			{"depth", depth},
		};
	}

	void fromJson(const json& j) 
	{
		ASSERT(j.find("normalizedlodError") != j.end(), "normalizedlodError not found");
		normalizedlodError = j["normalizedlodError"].get<double>();

		ASSERT(j.find("parentNormalizedError") != j.end(), "parentNormalizedError not found");
		parentNormalizedError = j["parentNormalizedError"].get<double>();

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

		ASSERT(j.find("clusterIndices") != j.end() && j["clusterIndices"].is_array(), "clusterIndices not found");
		for (int i = 0; i < CLUSTER_GROUP_MAX_SIZE; ++i) {
			clusterIndices[i] = j["clusterIndices"].at(i).get<uint32_t>();
		}

		ASSERT(j.find("nodeStatus") != j.end(), "nodeStatus not found");
		nodeStatus = j["nodeStatus"].get<NaniteBVHNodeStatus>();

		ASSERT(j.find("depth") != j.end(), "depth not found");
		depth = j["depth"].get<uint32_t>();
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