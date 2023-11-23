#pragma once
#include <vector>
#include <unordered_set>
#include "Common.h"
#include "Graph.h"
#include "utils.h"

/*
	Need to refactor ClusterGroup
	ClusterGroup will do graph partition separately
		To accomplish this, we need a 1-1 mapping between local triangle indices and global triangle indices
		We also need to manually merge cluster index when all graph partition is done

		Triangles:
			Local:
				0, 1, ..., localTriangleNum
			Global:
			    f_0, f_1, ..., f_localTriangleNum
			(localTriangleNum = clusterGroupFaces.size())
		Cluster:
			Local:
				0, 1, ..., localClusterNum
			Global:
				c_0, c_1, ..., c_localClusterNum
*/
struct ClusterGroup {
	MyMesh * mesh;
	OpenMesh::HPropHandleT<int32_t> clusterGroupIndexPropHandle;

	std::vector<uint32_t> clusterIndices; // Stores the cluster indices of meshLOD N-1

	uint32_t localFaceNum; // Number of faces in clusterGroup in meshLOD N-1

	std::unordered_set<MyMesh::FaceHandle> clusterGroupFaces; // Stores all faces of clusterGroup in meshLOD N
	std::vector<MyMesh::HalfedgeHandle> clusterGroupHalfedges; // Stores all half-edges of clusterGroup in meshLOD N
	
	// Local triangle graph partition related
	std::vector<idx_t> localTriangleClusterIndices; // Stores the local cluster indices of local triangles
	const int targetClusterSize = 31;
	idx_t localClusterNum;
	Graph localTriangleGraph;
	
	// Stores a 1-1 mapping between local triangle indices and global triangle indices in meshLOD N
	std::vector<uint32_t> triangleIndicesLocalGlobalMap; 
	std::unordered_map<uint32_t, uint32_t> triangleIndicesGlobalLocalMap;

	float qemError;

	void buildTriangleIndicesLocalGlobalMapping();
	void buildLocalTriangleGraph();
	void generateLocalClusters();
};