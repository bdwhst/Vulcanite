#pragma once
#include "vulkan/vulkan.h"
#include "VulkanDevice.h"
#include "VulkanglTFModel.h"
#include <vector>

#include <OpenMesh/Core/IO/MeshIO.hh>
#include <OpenMesh/Core/Mesh/TriMesh_ArrayKernelT.hh>
#include <OpenMesh/Tools/Decimater/DecimaterT.hh>
#include <OpenMesh/Tools/Decimater/ModQuadricT.hh>
#include <OpenMesh/Core/Geometry/QuadricT.hh>

struct MyTraits : public OpenMesh::DefaultTraits
{
	VertexAttributes(OpenMesh::Attributes::Normal |
		OpenMesh::Attributes::TexCoord2D);
};
typedef OpenMesh::TriMesh_ArrayKernelT<MyTraits> MyMesh;


class MeshHandler {
	vks::VulkanDevice* device;
	const vkglTF::Model* model;
	vkglTF::Model::Vertices vertices;
	vkglTF::Model::Indices indices;
	std::vector<uint32_t> indexBuffer;
	std::vector<vkglTF::Vertex> vertexBuffer;
	std::vector<vkglTF::Primitive> primitives;
public:
	void loadFromvkglTFModel(const vkglTF::Model& model)
	{
		this->model = &model;
	}
	void simplifyModel(vks::VulkanDevice* device, VkQueue transferQueue)
	{
		for (auto& node : model->linearNodes)
		{
			if(node->mesh)
				simplifyMesh(*node->mesh);
		}
		this->device = device;
		createVertexBuffer(device, transferQueue);
	}
	void drawSimplifiedModel(VkCommandBuffer commandBuffer, uint32_t renderFlags, VkPipelineLayout pipelineLayout, uint32_t bindImageSet)
	{
		const VkDeviceSize offsets[1] = { 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertices.buffer, offsets);
		vkCmdBindIndexBuffer(commandBuffer, indices.buffer, 0, VK_INDEX_TYPE_UINT32);
		for (auto& prim : primitives)
		{
			if (renderFlags & vkglTF::RenderFlags::BindImages) {
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, bindImageSet, 1, &prim.material.descriptorSet, 0, nullptr);
			}
			vkCmdDrawIndexed(commandBuffer, prim.indexCount, 1, prim.firstIndex, 0, 0);
		}
	}
	~MeshHandler() {
		vkDestroyBuffer(device->logicalDevice, vertices.buffer, nullptr);
		vkFreeMemory(device->logicalDevice, vertices.memory, nullptr);
		vkDestroyBuffer(device->logicalDevice, indices.buffer, nullptr);
		vkFreeMemory(device->logicalDevice, indices.memory, nullptr);
	}
private:
	void createVertexBuffer(vks::VulkanDevice* device, VkQueue transferQueue)
	{
		size_t vertexBufferSize = vertexBuffer.size() * sizeof(vkglTF::Vertex);
		size_t indexBufferSize = indexBuffer.size() * sizeof(uint32_t);
		indices.count = static_cast<uint32_t>(indexBuffer.size());
		vertices.count = static_cast<uint32_t>(vertexBuffer.size());

		assert((vertexBufferSize > 0) && (indexBufferSize > 0));

		struct StagingBuffer {
			VkBuffer buffer;
			VkDeviceMemory memory;
		} vertexStaging, indexStaging;

		// Create staging buffers
		// Vertex data
		VK_CHECK_RESULT(device->createBuffer(
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			vertexBufferSize,
			&vertexStaging.buffer,
			&vertexStaging.memory,
			vertexBuffer.data()));
		// Index data
		VK_CHECK_RESULT(device->createBuffer(
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			indexBufferSize,
			&indexStaging.buffer,
			&indexStaging.memory,
			indexBuffer.data()));

		// Create device local buffers
		// Vertex buffer
		VK_CHECK_RESULT(device->createBuffer(
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | vkglTF::memoryPropertyFlags,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			vertexBufferSize,
			&vertices.buffer,
			&vertices.memory));
		// Index buffer
		VK_CHECK_RESULT(device->createBuffer(
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | vkglTF::memoryPropertyFlags,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			indexBufferSize,
			&indices.buffer,
			&indices.memory));

		// Copy from staging buffers
		VkCommandBuffer copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		VkBufferCopy copyRegion = {};

		copyRegion.size = vertexBufferSize;
		vkCmdCopyBuffer(copyCmd, vertexStaging.buffer, vertices.buffer, 1, &copyRegion);

		copyRegion.size = indexBufferSize;
		vkCmdCopyBuffer(copyCmd, indexStaging.buffer, indices.buffer, 1, &copyRegion);

		device->flushCommandBuffer(copyCmd, transferQueue, true);

		vkDestroyBuffer(device->logicalDevice, vertexStaging.buffer, nullptr);
		vkFreeMemory(device->logicalDevice, vertexStaging.memory, nullptr);
		vkDestroyBuffer(device->logicalDevice, indexStaging.buffer, nullptr);
		vkFreeMemory(device->logicalDevice, indexStaging.memory, nullptr);
	}
	void simplifyMesh(const vkglTF::Mesh& mesh)
	{
		for (auto& prim : mesh.primitives)
		{
			MyMesh mymesh;
			int vertStart = prim->firstVertex;
			int vertEnd = prim->firstVertex + prim->vertexCount;
			std::vector<MyMesh::VertexHandle> vhandles;
			for (int i = vertStart; i != vertEnd; i++)
			{
				auto& vert = model->vertexBuffer[i];
				auto vhandle = mymesh.add_vertex(MyMesh::Point(vert.pos.x, vert.pos.y, vert.pos.z));
				mymesh.set_normal(vhandle, MyMesh::Normal(vert.normal.x, vert.normal.y, vert.normal.z));
				mymesh.set_texcoord2D(vhandle, MyMesh::TexCoord2D(vert.uv.x, vert.uv.y));
				vhandles.emplace_back(vhandle);
			}
			int indStart = prim->firstIndex;
			int indEnd = prim->firstIndex + prim->indexCount;
			for (int i = indStart; i != indEnd; i += 3)
			{
				int i0 = model->indexBuffer[i] - vertStart, i1 = model->indexBuffer[i + 1] - vertStart, i2 = model->indexBuffer[i + 2] - vertStart;
				std::vector<MyMesh::VertexHandle> face_vhandles;
				face_vhandles.clear();
				face_vhandles.emplace_back(vhandles[i0]);
				face_vhandles.emplace_back(vhandles[i1]);
				face_vhandles.emplace_back(vhandles[i2]);
				mymesh.add_face(face_vhandles);
			}

			mymesh.request_face_status();
			mymesh.request_edge_status();
			mymesh.request_vertex_status();

			OpenMesh::Decimater::DecimaterT<MyMesh> decimater(mymesh);
			/*OpenMesh::Decimater::ModBaseT<MyMesh>::Handle hModBoundary;
			decimater.add(hModBoundary);
			decimater.module(hModBoundary).set_binary(false);
			decimater.module(hModBoundary).set_fixed_boundaries(true);*/
			OpenMesh::Decimater::ModQuadricT<MyMesh>::Handle hModQuadric;
			decimater.add(hModQuadric);
			decimater.module(hModQuadric).set_binary(false);
			/*decimater.module(hModQuadric).set_normal_constraint(true);
			decimater.module(hModQuadric).set_normal_boundary_constraint(true);
			decimater.module(hModQuadric).set_texture_constraint(true);*/

			decimater.initialize();

			for (auto he_it = mymesh.halfedges_begin(); he_it != mymesh.halfedges_end(); ++he_it)
				if (mymesh.is_boundary(*he_it)) {
					mymesh.status(mymesh.to_vertex_handle(*he_it)).set_locked(true);
					mymesh.status(mymesh.from_vertex_handle(*he_it)).set_locked(true);
				}
			size_t original_faces = mymesh.n_faces();
			double percentage = 0.2; 
			size_t target_faces = static_cast<size_t>(original_faces * percentage);

			decimater.decimate_to_faces(0, target_faces);
			mymesh.garbage_collection();

			int newVertStart = vertexBuffer.size();
			vkglTF::Primitive reducedPrim(*prim);
			reducedPrim.firstVertex = newVertStart;
			for (MyMesh::VertexIter v_it = mymesh.vertices_begin(); v_it != mymesh.vertices_end(); ++v_it) {
				vkglTF::Vertex vert;
				vert.pos = glm::vec3(mymesh.point(v_it)[0], mymesh.point(v_it)[1], mymesh.point(v_it)[2]);
				vert.normal = glm::vec3(mymesh.normal(v_it)[0], mymesh.normal(v_it)[1], mymesh.normal(v_it)[2]);
				vert.uv = glm::vec2(mymesh.texcoord2D(v_it)[0], mymesh.texcoord2D(v_it)[1]);
				//TODO: interpolate and add tangent
				vertexBuffer.push_back(vert);
			}
			reducedPrim.vertexCount = vertexBuffer.size() - newVertStart;
			reducedPrim.firstIndex = indexBuffer.size();
			for (MyMesh::FaceIter f_it = mymesh.faces_begin(); f_it != mymesh.faces_end(); ++f_it) {
				std::vector<int> indices;
				for (MyMesh::FaceVertexIter fv_it = mymesh.fv_iter(*f_it); fv_it.is_valid(); ++fv_it) {
					indexBuffer.push_back(fv_it->idx() + newVertStart);
				}
			}
			reducedPrim.indexCount = indexBuffer.size() - reducedPrim.firstIndex;
			primitives.emplace_back(reducedPrim);
		}
	}

};