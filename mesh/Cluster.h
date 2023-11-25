#pragma once
#include <glm/glm.hpp>
#include <vector>
struct Cluster{
    uint32_t clusterGroupIndex;
    std::vector<uint32_t> triangleIndices;
    std::vector<uint32_t> parentClusterIndices;
    std::vector<uint32_t> childClusterIndices;
    double qemError = -1;
    double lodError = -1;
    double childMaxLODError = -1;
    double parentError = -1;
    bool isLeaf = true;
    uint32_t lodLevel = -1;

    glm::vec3 boundingSphereCenter;
    float boundingSphereRadius;
};

struct ClusterNode {
    double parentMaxLODError = -1;
    double lodError = -1;
    glm::vec3 boundingSphereCenter;
    float boundingSphereRadius;
};