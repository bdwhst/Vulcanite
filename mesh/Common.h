#pragma once
#include <string>
#include <OpenMesh/Core/Mesh/TriMesh_ArrayKernelT.hh>
#include <glm/glm.hpp>
#include "Config.h"

struct MyTraits : public OpenMesh::DefaultTraits
{
	VertexAttributes(OpenMesh::Attributes::Normal |
		OpenMesh::Attributes::TexCoord2D);

	//// TODO: When loading from disk, we can set HalfedgeAttributes to be None to reduce memory usage
	//HalfedgeAttributes(OpenMesh::Attributes::PrevHalfedge| 
	//	OpenMesh::Attributes::Status);
	//HalfedgeTraits
	//{
	//	uint32_t clusterGroupIndex = -1;
	//};
};
typedef OpenMesh::TriMesh_ArrayKernelT<MyTraits> MyMesh;

void writeMyMeshToFile(const MyMesh& mesh, const std::string & filename);


//  Only store information that are useful for subsequent traversal
//      aabb
//      parent error
//      children indices(glm::ivec4)
//  Some meta information about bvh should also be stored (in a uniform buffer)
//      Node count of each level (At least we should know the node count of level 0 to initiate traversal)
struct BVHNodeInfo {
    BVHNodeInfo() : pMinWorld(FLT_MAX), pMaxWorld(-FLT_MAX), childrenNodeIndices(-1), errorWorld(FLT_MAX), clusterIntervals(-1)
    {
        //for (size_t i = 0; i < CLUSTER_GROUP_MAX_SIZE; i++)
        //{
        //    clusterIndices[i] = -1;
        //}
    }
    alignas(16) glm::ivec2 clusterIntervals = glm::ivec2(-1, -1); // [clusterIntervals.x, clusterIntervals.y) is the range of clusterIndices
    alignas(16) glm::vec3 pMinWorld = glm::vec3(FLT_MAX);
    alignas(16) glm::vec3 pMaxWorld = glm::vec3(-FLT_MAX);
    alignas(4) uint32_t objectId;
    alignas(16)  glm::vec4 errorR;//node error and parent error. Node error should be non-neccessary, kept for now
    alignas(16)  glm::vec4 errorRP;//node error and parent error. Node error should be non-neccessary, kept for now
    alignas(8)  glm::vec2 errorWorld;//node error and parent error. Node error should be non-neccessary, kept for now
    alignas(16) glm::ivec4 childrenNodeIndices;
    // Note: The annotated one is wrong!
    //alignas(CLUSTER_GROUP_MAX_SIZE * 4) int clusterIndices[CLUSTER_GROUP_MAX_SIZE];
    //alignas(16) int clusterIndices[CLUSTER_GROUP_MAX_SIZE]; // if clusterIndices[0] == -1, then this node is not a leaf node
};

//ClusterInfo for drawing
struct ClusterInfo {
    alignas(16) glm::vec3 pMinWorld = glm::vec3(FLT_MAX);
    alignas(16) glm::vec3 pMaxWorld = glm::vec3(-FLT_MAX);
    // [triangleIndicesStart, triangleIndicesEnd) is the range of triangleIndicesSortedByClusterIdx
    // left close, right open
    alignas(4) uint32_t triangleIndicesStart; // Used to index Mesh::triangleIndicesSortedByClusterIdx
    alignas(4) uint32_t triangleIndicesEnd; // Used to index Mesh::triangleIndicesSortedByClusterIdx
    alignas(4) uint32_t objectIdx;

    void mergeAABB(const glm::vec3& pMinOther, const glm::vec3& pMaxOther) {
        pMinWorld = glm::min(pMinWorld, pMinOther);
        pMaxWorld = glm::max(pMaxWorld, pMaxOther);
    };
};

struct ErrorInfo
{
    alignas(8)  glm::vec2 errorWorld;//node error and parent error in world
    alignas(16) glm::vec4 centerR;
    alignas(16) glm::vec4 centerRP;
};
