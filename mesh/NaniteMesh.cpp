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
			float normalizedParentError = (i == meshes.size() - 1) ? cluster.parentNormalizedError / cluster.parentSurfaceArea : FLT_MAX;
			float normalizedLodError = cluster.lodError / cluster.surfaceArea;
			ASSERT(cluster.parentSurfaceArea > 0 || i == meshes.size()-1, "parentSurfaceArea should be positive");
			ASSERT(cluster.surfaceArea > 0, "surfaceArea should be positive");
			flattenedClusterNodes.emplace_back(ClusterNode({ 
				normalizedParentError,
				normalizedLodError,
				cluster.boundingSphereCenter, 
				cluster.boundingSphereRadius 
			}));
		}
	}
}

void NaniteMesh::flattenBVH()
{
	virtualBVHRootNode = std::make_shared<NaniteBVHNode>();
	virtualBVHRootNode->nodeStatus = NaniteBVHNodeStatus::VIRTUAL_NODE;
	for (const auto & mesh: meshes)
	{
		virtualBVHRootNode->children.push_back(mesh.rootBVHNode);
	}

	std::queue<std::shared_ptr<NaniteBVHNode>> nodeQueue;
	nodeQueue.push(virtualBVHRootNode);

	uint32_t index = 0;

	std::vector<uint32_t> depthCounts;
	uint32_t maxLevels = 0;
	// Do a two-pass tree bfs
	//      First pass update flattened index
	//      Second pass update children index
	while (!nodeQueue.empty())
	{
		auto currNode = nodeQueue.front();
		currNode->index = index;
		std::string indent(currNode->depth, '\t');
		ASSERT(currNode->nodeStatus != NaniteBVHNodeStatus::INVALID, "Invalid node!");

		//std::cout << indent << (currNode->nodeStatus == NaniteBVHNodeStatus::LEAF ? "Leaf " : "Non-leaf ") << currNode->depth << " " << currNode->index << std::endl;
		//ASSERT(currNode->depth <= depthCounts.size(), "currNode->level should never be greater than size of depthCounts, check traversal implementation");
		if (currNode->depth == depthCounts.size())
		{
			depthCounts.push_back(1);
		}
		else
		{
			depthCounts[currNode->depth] += 1;
		}
		index++;
		nodeQueue.pop();
		std::cout << indent << "currNode->index: " << currNode->index 
			<< std::endl << indent << "currNode->parentBoundingSphereCenter: " << currNode->parentBoundingSphere.x << " " << currNode->parentBoundingSphere.y << " " << currNode->parentBoundingSphere.z
			<< std::endl << indent << "currNode->parentBoundingSphereRadius: " << currNode->parentBoundingSphere.w
			<< std::endl;
		if (currNode->nodeStatus == NaniteBVHNodeStatus::LEAF)
		{
			//std::cout << "Leaf node: " <<
			//    "pMin: " << currNode->pMin.x << " " << currNode->pMin.y << " " << currNode->pMin.z <<
			//    "pMax: " << currNode->pMax.x << " " << currNode->pMax.y << " " << currNode->pMax.z <<
			//    std::endl;
		}
		else
		{
			//std::cout << "Non-leaf node: "
			//    "pMin: " << currNode->pMin.x << " " << currNode->pMin.y << " " << currNode->pMin.z <<
			//    "pMax: " << currNode->pMax.x << " " << currNode->pMax.y << " " << currNode->pMax.z <<
			//    std::endl;
			for (auto child : currNode->children)
			{
				//std::cout << indent << child->depth << std::endl;
				nodeQueue.push(child);
			}
		}
	}
	flattenedBVHNodeInfos.resize(index);  // `index` now is the size of nodes
	//for (size_t i = 0; i < depthCounts.size(); i++)
	//{
	//    std::cout << "i " << i << " depthCounts[i]: " << depthCounts[i] << std::endl;
	//}

	nodeQueue.push(virtualBVHRootNode);
	while (!nodeQueue.empty())
	{

		auto currNode = nodeQueue.front();
		NaniteBVHNodeInfo nodeInfo;
		//std::cout << currNode->children.size() << std::endl;
		ASSERT(currNode->nodeStatus == VIRTUAL_NODE || currNode->children.size() <= 4, "size of non-virtual nodes' children should never be over 4");
		nodeInfo.children.resize(currNode->children.size());
		for (int i = 0; i < currNode->children.size(); ++i)
		{
			nodeInfo.children[i] = currNode->children[i]->index;
		}
		nodeInfo.pMax = currNode->pMax;
		nodeInfo.pMin = currNode->pMin;
		nodeInfo.parentNormalizedError = currNode->parentNormalizedError;
		nodeInfo.normalizedlodError = currNode->normalizedlodError;
		nodeInfo.parentBoundingSphere = currNode->parentBoundingSphere;
		nodeInfo.nodeStatus = currNode->nodeStatus;
		nodeInfo.depth = currNode->depth;
		nodeInfo.index = currNode->index;
		nodeInfo.clusterIndices = currNode->clusterIndices;
		nodeInfo.lodLevel = currNode->lodLevel;
		ASSERT(flattenedBVHNodeInfos[currNode->index].nodeStatus == INVALID, "Repeated index!");
		ASSERT(currNode->index < flattenedBVHNodeInfos.size(), "index over flattenedBVHNodeInfos.size()");
		nodeQueue.pop();
		if (currNode->nodeStatus == NaniteBVHNodeStatus::LEAF)
		{
			ASSERT(currNode->clusterIndices.size() <= CLUSTER_GROUP_MAX_SIZE, "cluster group size over threshold!");
			//for (size_t i = 0; i < CLUSTER_GROUP_MAX_SIZE; i++)
			//{
			//	std::cout << std::string(currNode->depth, '\t') << "nodeInfo: " << nodeInfo.clusterIndices[i] << std::endl;
			//	std::cout << std::string(currNode->depth, '\t') << "currNode: " << currNode->clusterIndices[i] << std::endl;
			//}
			//std::cout << "Leaf node: " <<
			//	"pMin: " << currNode->pMin.x << " " << currNode->pMin.y << " " << currNode->pMin.z <<
			//	"pMax: " << currNode->pMax.x << " " << currNode->pMax.y << " " << currNode->pMax.z <<
			//	std::endl;
		}
		else
		{
			//std::cout << "Non-leaf node: "
			//	"pMin: " << currNode->pMin.x << " " << currNode->pMin.y << " " << currNode->pMin.z <<
			//	"pMax: " << currNode->pMax.x << " " << currNode->pMax.y << " " << currNode->pMax.z <<
			//	std::endl;
			for (auto child : currNode->children)
			{
				nodeQueue.push(child);
			}
		}
		flattenedBVHNodeInfos[currNode->index] = nodeInfo;
	}
}

void NaniteMesh::generateNaniteInfo() {
	MyMesh mymesh;
	vkglTFMeshToOpenMesh(mymesh, *vkglTFMesh);
	int clusterGroupNum = -1;
	int target = 6;
	int currFaceNum = -1;
	/*if (!OpenMesh::IO::read_mesh(mymesh, "D:\\AndrewChen\\CIS565\\Vulcanite\\assets\\models\\bunny.obj")) {
		ASSERT(0, "failed to load mesh");
	}*/
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
	
	//flattenDAG();
	for (size_t i = 0; i < meshes.size(); i++)
	{
		meshes[i].createBVH();
	}
	// Linearize BVH
	flattenBVH();
	
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

void NaniteMesh::serialize(const std::string& filepath)
{
	std::filesystem::path directoryPath(filepath);

	try {
		if (std::filesystem::create_directory(directoryPath)) {
			std::cout << "Directory created successfully." << std::endl;
		}
		else {
			std::cout << "Failed to create directory or it already exists. Dir:" << filepath << std::endl;
		}
	}
	catch (const std::filesystem::filesystem_error& e) {
		ASSERT(0, "Error creating directory");
	}

	for (size_t i = 0; i < meshes.size(); i++)
	{
		auto& mesh = meshes[i];
		std::string output_filename = std::string(filepath) + "LOD_" + std::to_string(i) + ".obj";
		// Export the mesh to the specified file
		if (!OpenMesh::IO::write_mesh(mesh.mesh, output_filename, OpenMesh::IO::Options::VertexNormal  | OpenMesh::IO::Options::VertexTexCoord)) {
			std::cerr << "Error exporting mesh to " << output_filename << std::endl;
		}
	}

	json result;
	for (size_t i = 0; i < meshes.size(); i++)
	{
		result["mesh"][i] = meshes[i].toJson();
	}
	for (size_t i = 0; i < flattenedBVHNodeInfos.size(); i++)
	{
		result["flattenedBVHNodeInfos"][i] = flattenedBVHNodeInfos[i].toJson();
	}
	result["flattenedBVHNodeCounts"] = flattenedBVHNodeInfos.size();
	result[cache_time_key] = std::time(nullptr);
	result["lodNums"] = lodNums;

	// Save the JSON data to a file
	std::ofstream file(std::string(filepath) + "nanite_info.json");
	if (file.is_open()) {
		file << result.dump(2); // Pretty-print with an indentation of 2 spaces
		file.close();
	}
	else {
		ASSERT(0, "Error opening file for serialization");
	}
}

void NaniteMesh::deserialize(const std::string & filepath)
{
	std::ifstream inputFile(std::string(filepath) + "nanite_info.json");

	ASSERT(inputFile.is_open(), "Error opening file for deserialization");
	json loadedJson;
	inputFile >> loadedJson;
		
	lodNums = loadedJson["lodNums"].get<uint32_t>();
	meshes.resize(lodNums);
	for (int i = 0; i < lodNums; ++i) {
		auto& meshLOD = meshes[i];
		meshLOD.fromJson(loadedJson["mesh"][i]);

		float percentage = static_cast<float>(i+1) / lodNums * 100.0;
		std::cout << "\r";
		std::cout << "[Loading] Mesh Info: " << std::fixed << std::setw(6) << std::setprecision(2) << percentage << "%";
		std::cout.flush();
	}
	std::cout << std::endl;

	int flattenedBVHNodeCounts = loadedJson["flattenedBVHNodeCounts"].get<uint32_t>();
	flattenedBVHNodeInfos.resize(flattenedBVHNodeCounts, NaniteBVHNodeInfo());
	for (int i = 0; i < flattenedBVHNodeCounts; ++i) {
		auto& nodeInfo = flattenedBVHNodeInfos[i];
		nodeInfo.fromJson(loadedJson["flattenedBVHNodeInfos"][i]);
		std::string indent(nodeInfo.depth, '\t');
		//if (nodeInfo.nodeStatus == LEAF) {
		//	std::cout << "Index: " << nodeInfo.index << std::endl;
		//	for (size_t i = 0; i < nodeInfo.clusterIndices.size(); i++)
		//	{
		//		std::cout << '\t' << nodeInfo.clusterIndices[i] << std::endl;
		//	}
		//}
		float percentage = static_cast<float>(i + 1) / flattenedBVHNodeCounts * 100.0;
		std::cout << "\r";
		std::cout << "[Loading] BVH Info: " << std::fixed << std::setw(6) << std::setprecision(2) << percentage << "%";
		std::cout.flush();
	}
	std::cout << std::endl;

	for (size_t i = 0; i < lodNums; i++)
	{
		std::string output_filename = std::string(filepath) + "LOD_" + std::to_string(i) + ".obj";
		meshes[i].mesh.request_vertex_normals();
		meshes[i].mesh.request_vertex_texcoords2D();
		OpenMesh::IO::Options opt = OpenMesh::IO::Options::VertexNormal | OpenMesh::IO::Options::VertexTexCoord;
		if (!OpenMesh::IO::read_mesh(meshes[i].mesh, output_filename, opt)) {
			ASSERT(0, "failed to load mesh");
		}
		ASSERT(meshes[i].mesh.has_vertex_normals(), "mesh has no normals");
		meshes[i].lodLevel = i;

		std::cout << "\r";
		float percentage = static_cast<float>(i+1) / lodNums * 100.0;
		std::cout << "[Loading] Mesh LOD: " << std::fixed << std::setw(6) << std::setprecision(2) << percentage << "%";
		std::cout.flush();
	}
	std::cout << std::endl;
}

void NaniteMesh::initNaniteInfo(const std::string & filepath, bool useCache) {
	bool hasCache = false;
	bool hasInitialized = false;
	std::string cachePath;
	if (filepath.find_last_of(".") != std::string::npos) {
		cachePath = filepath.substr(0, filepath.find_last_of('.')) + "_naniteCache\\";
	}
	else {
		ASSERT(0, "Invalid file path, no ext");
	}

	if (useCache) {
		std::ifstream inputFile(cachePath + "nanite_info.json");
		// TODO: Check cache time to see if cache needs to be rebuilt
		if (inputFile.is_open()) {
			deserialize(cachePath);
			hasInitialized = true;
		}
		else {
			std::cerr << "No cache, need to initialize from now" << std::endl;
		}
	}	

	if (!hasInitialized) {
		std::cerr << "Start building..." << std::endl;
		generateNaniteInfo();
		serialize(cachePath);
		std::cout << cachePath << "nanite_info.json" << " generated" << std::endl;
		//checkDeserializationResult(cachePath);
	}
}

void NaniteMesh::checkDeserializationResult(const std::string& filepath)
{
	std::ifstream inputFile(std::string(filepath) + "nanite_info.json");

	ASSERT(inputFile.is_open(), "Error opening file for deserialization");
	json loadedJson;
	inputFile >> loadedJson;

	lodNums = loadedJson["lodNums"].get<uint32_t>();
	debugMeshes.resize(lodNums);
	for (int i = 0; i < lodNums; ++i) {
		auto& meshLOD = debugMeshes[i];
		meshLOD.fromJson(loadedJson["mesh"][i]);
	}

	for (size_t i = 0; i < lodNums; i++)
	{
		std::string output_filename = std::string(filepath) + "LOD_" + std::to_string(i) + ".obj";
		debugMeshes[i].mesh.request_vertex_normals();
		OpenMesh::IO::Options opt = OpenMesh::IO::Options::VertexNormal;
		if (!OpenMesh::IO::read_mesh(debugMeshes[i].mesh, output_filename, opt)) {
			ASSERT(0, "failed to load mesh");
		}
		ASSERT(debugMeshes[i].mesh.has_vertex_normals(), "mesh has no normals");
		debugMeshes[i].lodLevel = i;
	}

	for (size_t i = 0; i < lodNums; i++)
	{
		auto& mesh = meshes[i];
		auto& debugMesh = debugMeshes[i];
		TEST(mesh.clusters.size() == debugMesh.clusters.size(), "cluster size match");
		for (size_t clusterIdx = 0; clusterIdx < mesh.clusters.size(); clusterIdx++)
		{
			const auto& cluster = mesh.clusters[clusterIdx];
			const auto& debugCluster = debugMesh.clusters[clusterIdx];
			TEST(cluster.parentNormalizedError == debugCluster.parentNormalizedError, "parentNormalizedError match");
			TEST(cluster.lodError == debugCluster.lodError, "lodError match");
			TEST(cluster.boundingSphereCenter == debugCluster.boundingSphereCenter, "boundingSphereCenter match");
			TEST(cluster.boundingSphereRadius == debugCluster.boundingSphereRadius, "boundingSphereRadius match");
		}
		TEST(mesh.mesh.n_faces() == debugMesh.mesh.n_faces(), "face size match");
		TEST(mesh.mesh.n_vertices() == debugMesh.mesh.n_vertices(), "vertex size match");
		for (const auto & vhandle: mesh.mesh.vertices())
		{
			auto& debugVhandle = debugMesh.mesh.vertex_handle(vhandle.idx());
			//std::cout << "mesh normal: " << mesh.mesh.normal(vhandle)[0] << " " << mesh.mesh.normal(vhandle)[1] << " " << mesh.mesh.normal(vhandle)[2] << std::endl;
			//std::cout << "debug normal: " << debugMesh.mesh.normal(debugVhandle)[0] << " " << debugMesh.mesh.normal(debugVhandle)[1] << " " << debugMesh.mesh.normal(debugVhandle)[2] << std::endl;
			TEST((mesh.mesh.point(vhandle) - debugMesh.mesh.point(debugVhandle)).length() < 1e-5f, "vertex position match");
			TEST((mesh.mesh.normal(vhandle) - debugMesh.mesh.normal(debugVhandle)).length() < 1e-5f, "vertex normal match");
			//ASSERT((mesh.mesh.texcoord2D(vhandle) - debugMesh.mesh.texcoord2D(debugVhandle)).length() < 1e-5f, "vertex texcoord not match");
		}

	}
}

void loadvkglTFModel(const vkglTF::Model& model, std::vector<NaniteMesh>& naniteMeshes) {
	ASSERT(0, "Not implemented");
}

void packNaniteMeshesToIndexBuffer(const std::vector<NaniteMesh>& naniteMeshes, std::vector<uint32_t>& indexBuffer){

}