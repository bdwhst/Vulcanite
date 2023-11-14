#pragma once
#include <vector>
#include <unordered_set>
struct ClusterGroup {
	std::vector<uint32_t> clusterIndices;
	std::unordered_set<uint32_t> boundaryIndices;
};