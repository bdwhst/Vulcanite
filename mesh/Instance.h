#pragma once
#include "Mesh.h"
#include "glm/glm.hpp"
#include "Cluster.h"

//ClusterInfo for drawing
struct ClusterInfo {
    alignas(16) glm::vec3 pMinWorld = glm::vec3(FLT_MAX);
    alignas(16) glm::vec3 pMaxWorld = glm::vec3(-FLT_MAX);
	// [triangleIndicesStart, triangleIndicesEnd) is the range of triangleIndicesSortedByClusterIdx
	// left close, right open
    alignas(4) uint32_t triangleIndicesStart; // Used to index Mesh::triangleIndicesSortedByClusterIdx
    alignas(4) uint32_t triangleIndicesEnd; // Used to index Mesh::triangleIndicesSortedByClusterIdx

	void mergeAABB(const glm::vec3& pMinOther, const glm::vec3& pMaxOther) {
		pMinWorld = glm::min(pMinWorld, pMinOther);
		pMaxWorld = glm::max(pMaxWorld, pMaxOther);
	};
};

struct Instance {
	Mesh* referenceMesh;
	glm::mat4 rootTransform;
	std::vector<ClusterInfo> clusterInfo;
    Instance(){}
    Instance(Mesh* mesh, const glm::mat4 model):referenceMesh(mesh), rootTransform(model){}

	void buildClusterInfo()
	{
        // Init Clusters
        clusterInfo.resize(referenceMesh->clusterNum);
        auto& mesh = referenceMesh->mesh;
        for (MyMesh::FaceIter face_it = mesh.faces_begin(); face_it != mesh.faces_end(); ++face_it) {
            MyMesh::FaceHandle fh = *face_it;
            auto clusterIdx = referenceMesh->triangleClusterIndex[fh.idx()];
            auto& clusterI = clusterInfo[clusterIdx];
            
            glm::vec3 pMinWorld, pMaxWorld;
            glm::vec3 p0, p1, p2;
            MyMesh::FaceVertexIter fv_it = mesh.fv_iter(fh);

            // Get the positions of the three vertices
            auto point0 = mesh.point(*fv_it);
            ++fv_it;
            auto point1 = mesh.point(*fv_it);
            ++fv_it;
            auto point2 = mesh.point(*fv_it);

            p0[0] = point0[0];
            p0[1] = point0[1];
            p0[2] = point0[2];

            p1[0] = point1[0];
            p1[1] = point1[1];
            p1[2] = point1[2];

            p2[0] = point2[0];
            p2[1] = point2[1];
            p2[2] = point2[2];

            p0 = glm::vec3(rootTransform * glm::vec4(p0, 1.0f));
            p1 = glm::vec3(rootTransform * glm::vec4(p1, 1.0f));
            p2 = glm::vec3(rootTransform * glm::vec4(p2, 1.0f));

            getTriangleAABB(p0, p1, p2, pMinWorld, pMaxWorld);

            clusterI.mergeAABB(pMinWorld, pMaxWorld);
        }


        uint32_t currClusterIdx = -1;
        for (size_t i = 0; i < referenceMesh->triangleIndicesSortedByClusterIdx.size(); i++)
        {
            auto currTriangleIndex = referenceMesh->triangleIndicesSortedByClusterIdx[i];
            if (referenceMesh->triangleClusterIndex[currTriangleIndex] != currClusterIdx)
            {
                if (currClusterIdx != -1)
                    clusterInfo[currClusterIdx].triangleIndicesEnd = i;
                currClusterIdx = referenceMesh->triangleClusterIndex[currTriangleIndex];
                clusterInfo[currClusterIdx].triangleIndicesStart = i;
            }
        }
        clusterInfo[currClusterIdx].triangleIndicesEnd = referenceMesh->triangleIndicesSortedByClusterIdx.size();
	}

};