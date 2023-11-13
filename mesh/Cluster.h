#pragma once
#include <glm/glm.hpp>
#include <vector>
struct Cluster{
    glm::vec3 pMin = glm::vec3(FLT_MAX);
    glm::vec3 pMax = glm::vec3(-FLT_MAX);

    uint32_t clusterGroupIndex;
    std::vector<uint32_t> triangleIndices;

    void mergeAABB(const glm::vec3& pMinOther, const glm::vec3& pMaxOther) {
        pMin = glm::min(pMin, pMinOther);
        pMax = glm::max(pMax, pMaxOther);
    };
};