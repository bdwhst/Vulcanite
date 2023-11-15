#pragma once
#include <glm/glm.hpp>
#include <vector>
struct Cluster{
    uint32_t clusterGroupIndex;
    std::vector<uint32_t> triangleIndices;
    std::vector<uint32_t> parentClusterIndices;
    std::vector<uint32_t> childClusterIndices;
};