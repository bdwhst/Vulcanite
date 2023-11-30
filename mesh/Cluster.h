#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <json/json.hpp>

using json = nlohmann::json;

struct Cluster{
    uint32_t clusterGroupIndex;
    std::vector<uint32_t> triangleIndices;
    std::vector<uint32_t> parentClusterIndices;
    std::vector<uint32_t> childClusterIndices;
    double qemError = -1;
    double lodError = -1;
    double normalizedlodError = -1;
    double childLODErrorMax = 0.0;
    double parentNormalizedError = -1;
    bool isLeaf = true;
    uint32_t lodLevel = -1;

    float surfaceArea = 0.0f;
    float parentSurfaceArea = 0.0f;
    glm::vec3 boundingSphereCenter;
    float boundingSphereRadius;

    json toJson() const {
        return {
            {"parentError", parentNormalizedError},
            {"lodError", lodError},
            {"boundingSphereCenter", {boundingSphereCenter.x, boundingSphereCenter.y, boundingSphereCenter.z}},
            {"boundingSphereRadius", boundingSphereRadius},
            {"parentClusterIndices", parentClusterIndices}
        };
    }

    void fromJson(const json& j) {
        if (j.find("parentError") != j.end()) {
            parentNormalizedError = j["parentError"].get<double>();
        }

        if (j.find("lodError") != j.end()) {
            lodError = j["lodError"].get<double>();
        }

        if (j.find("boundingSphereCenter") != j.end() && j["boundingSphereCenter"].is_array() && j["boundingSphereCenter"].size() == 3) {
            boundingSphereCenter.x = j["boundingSphereCenter"][0].get<float>();
            boundingSphereCenter.y = j["boundingSphereCenter"][1].get<float>();
            boundingSphereCenter.z = j["boundingSphereCenter"][2].get<float>();
        }

        //if (j.contains("boundingSphereRadius")) {
        if (j.find("boundingSphereRadius") != j.end()) {
            boundingSphereRadius = j["boundingSphereRadius"].get<double>();
        }

        if (j.find("parentClusterIndices") != j.end() && j["parentClusterIndices"].is_array()) {
            for (auto& idx : j["parentClusterIndices"]) {
				parentClusterIndices.emplace_back(idx.get<uint32_t>());
			}
		}
    }
};

struct ClusterNode {
    double parentMaxLODError = -1;
    double lodError = -1;
    glm::vec3 boundingSphereCenter;
    float boundingSphereRadius;

    // Serialization method to convert object to JSON
    json toJson() const {
        return {
            {"parentMaxLODError", parentMaxLODError},
            {"lodError", lodError},
            {"boundingSphereCenter", {boundingSphereCenter.x, boundingSphereCenter.y, boundingSphereCenter.z}},
            {"boundingSphereRadius", boundingSphereRadius}
        };
    }

    // Deserialization method to populate object from JSON
    void fromJson(const json& j) {
        //if (j.contains("parentMaxLODError")) {
        if (j.find("parentMaxLODError") != j.end()) {
            parentMaxLODError = j["parentMaxLODError"].get<double>();
        }

        //if (j.contains("lodError")) {
        if (j.find("lodError") != j.end()) {
            lodError = j["lodError"].get<double>();
        }

        //if (j.contains("boundingSphereCenter") && j["boundingSphereCenter"].is_array() && j["boundingSphereCenter"].size() == 3) {
        if (j.find("boundingSphereCenter") != j.end() && j["boundingSphereCenter"].is_array() && j["boundingSphereCenter"].size() == 3) {
            boundingSphereCenter.x = j["boundingSphereCenter"][0].get<float>();
            boundingSphereCenter.y = j["boundingSphereCenter"][1].get<float>();
            boundingSphereCenter.z = j["boundingSphereCenter"][2].get<float>();
        }

        //if (j.contains("boundingSphereRadius")) {
        if (j.find("boundingSphereRadius") != j.end()){
            boundingSphereRadius = j["boundingSphereRadius"].get<double>();
        }
    }
};