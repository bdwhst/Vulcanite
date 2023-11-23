#include "NaniteMesh.h"

void NaniteMesh::loadvkglTFModel(const vkglTF::Model& model)
{
	vkglTFModel = &model;
	for (auto& node : vkglTFModel->linearNodes)
	{
		// TODO: Only support naniting one mesh within a model
		if (node->mesh) {
			vkglTFMesh = node->mesh;
			modelMatrix = node->getMatrix();
			break;
		}
	}
}

void NaniteMesh::vkglTFPrimitiveToOpenMesh(MyMesh& mymesh, const vkglTF::Primitive& prim)
{
	int vertStart = prim.firstVertex;
	int vertEnd = prim.firstVertex + prim.vertexCount;
	std::vector<MyMesh::VertexHandle> vhandles;
	for (int i = vertStart; i != vertEnd; i++)
	{
		auto& vert = vkglTFModel->vertexBuffer[i];
		auto vhandle = mymesh.add_vertex(MyMesh::Point(vert.pos.x, vert.pos.y, vert.pos.z));
		mymesh.set_normal(vhandle, MyMesh::Normal(vert.normal.x, vert.normal.y, vert.normal.z));
		mymesh.set_texcoord2D(vhandle, MyMesh::TexCoord2D(vert.uv.x, vert.uv.y));
		vhandles.emplace_back(vhandle);
	}
	int indStart = prim.firstIndex;
	int indEnd = prim.firstIndex + prim.indexCount;
	for (int i = indStart; i != indEnd; i += 3)
	{
		int i0 = vkglTFModel->indexBuffer[i] - vertStart, i1 = vkglTFModel->indexBuffer[i + 1] - vertStart, i2 = vkglTFModel->indexBuffer[i + 2] - vertStart;
		std::vector<MyMesh::VertexHandle> face_vhandles;
		face_vhandles.clear();
		face_vhandles.emplace_back(vhandles[i0]);
		face_vhandles.emplace_back(vhandles[i1]);
		face_vhandles.emplace_back(vhandles[i2]);
		mymesh.add_face(face_vhandles);
	}
}


void NaniteMesh::vkglTFMeshToOpenMesh(MyMesh& mymesh, const vkglTF::Mesh& mesh) {
	for (auto& prim : mesh.primitives)
	{
		vkglTFPrimitiveToOpenMesh(mymesh, *prim);

		mymesh.request_face_status();
		mymesh.request_edge_status();
		mymesh.request_vertex_status();
	}
}

void NaniteMesh::flattenDAG()
{
	for (int i = meshes.size()-1; i >= 0; i--)
	{
		auto& mesh = meshes[i];
		for (size_t clusterIdx = 0; clusterIdx < mesh.clusters.size(); clusterIdx++)
		{
			const auto & cluster = mesh.clusters[clusterIdx];
			ASSERT(i == cluster.lodLevel, "lod level not match");
			//std::cout << "Cluster " << clusterIdx 
			//	<< " lod " << i 
			//	<< " parentError " << cluster.parentError
			//	<< " lodError " << cluster.lodError
			//	<< std::endl;
			flattenedClusterNodes.emplace_back(ClusterNode({ cluster.parentError, cluster.lodError }));
		}
	}
}

void NaniteMesh::generateNaniteInfo() {
	MyMesh mymesh;
	vkglTFMeshToOpenMesh(mymesh, *vkglTFMesh);
	int clusterGroupNum = -1;
	int target = 3;
	int currFaceNum = -1;
	//if (!OpenMesh::IO::read_mesh(mymesh, "D:\\AndrewChen\\CIS565\\Vulcanite\\assets\\models\\bunny.obj")) {
	//	ASSERT(0, "failed to load mesh");
	//}
	// Add a customized property to store clusterGroupIndex of last level of detail
	mymesh.add_property(clusterGroupIndexPropHandle);
	do
	{
		// For each lod mesh
		Mesh meshLOD;
		meshLOD.mesh = mymesh;
		meshLOD.lodLevel = lodNums;
		meshLOD.clusterGroupIndexPropHandle = clusterGroupIndexPropHandle;
		if (clusterGroupNum > 0) {
			meshLOD.oldClusterGroups.resize(clusterGroupNum);
			meshLOD.assignTriangleClusterGroup(meshes.back());
		}
		else {
			meshLOD.buildTriangleGraph();
			meshLOD.generateCluster();
		}
		if (meshes.size() > 0) {
			auto& lastMeshLOD = meshes[meshes.size() - 1];
			// Maintain DAG

		}
		// Generate cluster group by partitioning cluster graph
		meshLOD.buildClusterGraph();
		meshLOD.colorClusterGraph(); // Cluster graph is needed to assign adjacent cluster different colors
		meshLOD.generateClusterGroup();
		currFaceNum = meshLOD.mesh.n_faces();
		clusterGroupNum = meshLOD.clusterGroupNum;

		mymesh = meshLOD.mesh;
		if (clusterGroupNum > 1) 
		{
			meshLOD.simplifyMesh(mymesh); 
			// Save LOD mesh for debugging
			//{
			//	std::string output_filename = "meshLOD_" + std::to_string(lodNums) + ".obj";
			//
			//	// Export the mesh to the specified file
			//	if (!OpenMesh::IO::write_mesh(mymesh, output_filename)) {
			//		std::cerr << "Error exporting mesh to " << output_filename << std::endl;
			//	}
			//}
		}
		meshes.emplace_back(meshLOD);
		std::cout << "LOD " << lodNums++ << " generated" << std::endl;

	} 
	//while (clusterGroupNum != 1 &&
	//  mymesh.n_faces() != currFaceNum // Decimation no longer decrease faces
	//); 
	while (--target); // Only do one time for testing

	// Linearize DAG
	flattenDAG();
	
	// Save mesh for debugging
	//{
	//	std::string output_filename = "output.obj";
	//
	//	// Export the mesh to the specified file
	//	if (!OpenMesh::IO::write_mesh(mymesh, output_filename)) {
	//		std::cerr << "Error exporting mesh to " << output_filename << std::endl;
	//	}
	//}
}

void loadvkglTFModel(const vkglTF::Model& model, std::vector<NaniteMesh>& naniteMeshes) {
	ASSERT(0, "Not implemented");
}

void packNaniteMeshesToIndexBuffer(const std::vector<NaniteMesh>& naniteMeshes, std::vector<uint32_t>& indexBuffer){

}