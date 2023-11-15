#pragma once
#include <glm/glm.hpp>
#include <vector>
struct Cluster{
    glm::vec3 pMinWorld = glm::vec3(FLT_MAX);
    glm::vec3 pMaxWorld = glm::vec3(-FLT_MAX);

    uint32_t clusterGroupIndex;
    std::vector<uint32_t> triangleIndices;
    
    std::vector<uint32_t> parentClusterIndices;
    std::vector<uint32_t> childClusterIndices;

    void mergeAABB(const glm::vec3& pMinOther, const glm::vec3& pMaxOther) {
        pMinWorld = glm::min(pMinWorld, pMinOther);
        pMaxWorld = glm::max(pMaxWorld, pMaxOther);
    };
};