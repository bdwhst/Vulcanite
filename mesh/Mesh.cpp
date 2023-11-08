#include "Mesh.h"

Mesh::Mesh(const char* filename) 
{
    // Initialize the ASSIMP importer
    Assimp::Importer importer;

    // Specify post-processing options for loading the GLTF file
    const unsigned int flags = 0
        //aiProcess_Triangulate |         // Triangulate the mesh
        //aiProcess_FlipUVs |             // Flip UV coordinates if needed
        //aiProcess_CalcTangentSpace |    // Calculate tangent and bitangent vectors
        //aiProcess_GenNormals |          // Generate normals if they are missing
        //aiProcess_PreTransformVertices  // Pre-transform positions to world space
        ; 

    // Load the GLTF model
    const aiScene* scene = importer.ReadFile(filename, flags);

    // Check if the model was loaded successfully
    if (!scene) {
        ASSERT(false, "Failed to load model");
    }

    auto mesh = scene->mMeshes[0];

    // Init positions and indices
    positions.resize(mesh->mNumVertices);
    indices.resize(mesh->mNumFaces * 3);

    for (uint32_t i = 0; i < mesh->mNumVertices; i++) {
        positions[i] = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
    }

    for (uint32_t i = 0; i < mesh->mNumFaces; i += 3) {
        ASSERT(mesh->mFaces[i / 3].mNumIndices == 3, "Not a triangle mesh");
        indices[i + 0] = mesh->mFaces[i / 3].mIndices[0];
        indices[i + 1] = mesh->mFaces[i / 3].mIndices[1];
        indices[i + 2] = mesh->mFaces[i / 3].mIndices[2];
    }

}

void Mesh::generateCluster()
{
}

void Mesh::generateClusterGroup()
{
}
