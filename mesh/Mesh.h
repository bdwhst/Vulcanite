#pragma once
#include <OpenMesh/Core/IO/MeshIO.hh>
#include <OpenMesh/Core/Mesh/TriMesh_ArrayKernelT.hh>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "glm/glm.hpp"

typedef OpenMesh::TriMesh_ArrayKernelT<> TriMesh;

#include "utils.h"

class Mesh {

public:
	Mesh()=delete;
	Mesh(const char* filename);
	void generateCluster();
	void generateClusterGroup();

private:
	std::vector<uint32_t> indices;
	std::vector<glm::vec3> positions;

};
