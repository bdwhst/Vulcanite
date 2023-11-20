#pragma once
#include <string>
#include <OpenMesh/Core/Mesh/TriMesh_ArrayKernelT.hh>

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