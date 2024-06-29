/*
* Vulkan Example - Physical based rendering a textured object (metal/roughness workflow) with image based lighting
*
* Note: Requires the separate asset pack (see data/README.md)
*
* Copyright (C) 2016-2017 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

// For reference see http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf

#include "vulkanexamplebase.h"
#include "VulkanglTFModel.h"
#include "MeshHandler.h"
#include "NaniteMesh.h"
#include "Instance.h"
#include "NaniteScene.h"
#include "VulkanDescriptorSetManager.h"

#define ENABLE_VALIDATION true

class VulkanExample : public VulkanExampleBase
{
public:
	bool displaySkybox = true;

	struct Textures {
		vks::TextureCubeMap environmentCube;
		// Generated at runtime
		vks::Texture2D lutBrdf;
		vks::TextureCubeMap irradianceCube;
		vks::TextureCubeMap prefilteredCube;
		// Object texture maps
		vks::Texture2D albedoMap;
		vks::Texture2D normalMap;
		vks::Texture2D aoMap;
		vks::Texture2D metallicMap;
		vks::Texture2D roughnessMap;
		vks::Texture2D hizbuffer;
	} textures;


	struct {
		VkImage image;
		VkDeviceMemory mem;
		VkImageView view;
		VkSampler sampler;
	} HWRZBuffer;

	struct {
		VkImage image;
		VkDeviceMemory mem;
		VkImageView view;
	} HWRVisBuffer;

	struct {
		VkImage image;
		VkDeviceMemory mem;
		VkImageView view;
	} SWRBuffer;

	VkFramebuffer HWRFramebuffer;

	struct {
		VkImage image;
		VkDeviceMemory mem;
		VkImageView view;
	} FinalZBuffer;

	struct {
		VkImage image;
		VkDeviceMemory mem;
		VkImageView view;
	} FinalVisBuffer;

	std::vector<VkImageView> hizImageViews;

	VkPipelineLayout hizComputePipelineLayout;
	VkPipeline hizComputePipeline;

	VkPipelineLayout depthCopyPipelineLayout;
	VkPipeline depthCopyPipeline;

	VkPipelineLayout debugQuadPipelineLayout;
	VkPipeline debugQuadPipeline;

	VkPipelineLayout bvhTraversalPipelineLayout;
	VkPipeline bvhTraversalPipeline;

	VkPipelineLayout cullingPipelineLayout;
	VkPipeline cullingPipeline;

	VkPipelineLayout errorProjPipelineLayout;
	VkPipeline errorProjPipeline;

	VkPipelineLayout shadingPipelineLayout;
	VkPipeline shadingPipeline;

	VkPipelineLayout hwrastPipelineLayout;
	VkPipeline hwrastPipeline;

	VkPipelineLayout swrComputePipelineLayout;
	VkPipeline swrComputePipeline;

	VkPipelineLayout mergeRastPipelineLayout;
	VkPipeline mergeRastPipeline;

	VkPipelineLayout clearImagePipelineLayout;
	VkPipeline clearImagePipeline;

	VkSampler depthStencilSampler;

	uint32_t workgroupX = 8, workgroupY = 8;

	uint32_t numCulledClusters;

	struct Meshes {
		vkglTF::Model skybox;
		vkglTF::Model object;
		vkglTF::Model cube;
	} models;

	MeshHandler reducedModel;
	NaniteMesh naniteMesh;
	//Instance instance1;
	NaniteScene scene;

	std::vector<ClusterInfo> clusterinfos;
	std::vector<ErrorInfo> errorinfos;

	struct {
		vks::Buffer object;
		vks::Buffer skybox;
		vks::Buffer cube;
		vks::Buffer topObject;
		vks::Buffer topSkybox;
		vks::Buffer topCube;
		vks::Buffer params;
		vks::Buffer modelMats;
		vks::Buffer shadingMats;
	} uniformBuffers;

	struct UBOMatrices {
		glm::mat4 projection;
		glm::mat4 model;
		glm::mat4 view;
		glm::vec3 camPos;
	} uboMatrices1, uboMatrices2, uboMatrices3, uboMatrices4;

	struct UBOShading {
		glm::mat4 invView;
		glm::mat4 invProj;
		glm::vec3 camPos;
	} uboshading;

	struct ModelMatrices {
		glm::mat4 model;
	};

	struct UBOCullingMatrices {
		glm::mat4 model;
		glm::mat4 lastView;
		glm::mat4 lastProj;
		glm::mat4 currView;
		glm::mat4 currProj;
	}uboCullingMatrices;

	struct UBOErrorMatrices {
		glm::mat4 view;
		glm::mat4 proj;
		alignas(16) glm::vec3 camUp;
		alignas(16) glm::vec3 camRight;
	}uboErrorMatrices;

	glm::mat4 model0 = glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, 3)), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	glm::mat4 model1 = glm::scale(glm::mat4(1.0f), glm::vec3(0.2f, 0.2f, 0.03f));
	std::vector<glm::mat4> modelMats = {
		glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, 3)), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
		glm::mat4(1.0f),
		glm::translate(glm::mat4(1.0f), glm::vec3(2.0f, 0.0f, 0.0f)),
		glm::mat4(1.0f),
		glm::translate(glm::mat4(1.0f), glm::vec3(2.0f, 0.0f, 0.0f)),
	};

	struct DrawIndexedIndirect {
		uint32_t indexCount;
		uint32_t instanceCount;
		uint32_t firstIndex;
		uint32_t vertexOffset;
		uint32_t firstInstance;
	}hwrDrawIndexedIndirect;

	struct UBOParams {
		glm::vec4 lights[4];
		float exposure = 4.5f;
		float gamma = 2.2f;
	} uboParams;

	struct {
		VkPipeline skybox;
		VkPipeline pbr;
		VkPipeline topView;
	} pipelines;

	int thresholdInt = 500;
	double thresholdIntDiv = 1e6;

	struct BVHTraversalPushConstants {
		alignas(8) glm::vec2 screenSize;
		alignas(4) float threshold;
	} bvhTraversalPushConstants;

	struct CullingPushConstants {
		int numClusters;
		float threshold;
		alignas(4) bool useFrustrumOcclusionCulling = true;
		alignas(4) bool useSoftwareRasterization = true;
	} cullingPushConstants;

	struct RenderingPushConstants {
		int vis_clusters = 0;
	} renderingPushConstants;

	vks::Buffer HWRIndicesBuffer;
	//vks::Buffer culledObjectIndicesBuffer;
	vks::Buffer HWRIDBuffer;
	vks::Buffer SWRIndicesBuffer;
	vks::Buffer SWRIDBuffer;

	vks::Buffer bvhNodeInfosBuffer;
	vks::Buffer initNodeInfosBuffer;
	vks::Buffer currNodeInfosBuffer;
	vks::Buffer nextNodeInfosBuffer;
	vks::Buffer sortedClusterIndicesBuffer; // Cluster indices sorted by BVH
	vks::Buffer culledClusterIndicesBuffer; // Cluster indices after BVH culling
	vks::Buffer culledClusterObjectIndicesBuffer;

	vks::Buffer culledIndicesBuffer;
	vks::Buffer modelMatsBuffer;
	vks::Buffer clustersInfoBuffer;
	vks::Buffer cullingUniformBuffer;
	vks::Buffer hwrDrawIndexedIndirectBuffer;

	struct SWRIndirectBuffer {
		uint32_t x = 0;
		uint32_t y = 0;
		uint32_t z = 0;
	}swrIndirectBuffer;

	vks::Buffer swrIndirectDispatchBuffer;
	vks::Buffer swrNumVerticesBuffer;

	struct ErrorPushConstants {
		alignas(4) int numClusters;
		alignas(8) glm::vec2 screenSize;
	} errorPushConstants;

	vks::Buffer errorInfoBuffer;
	vks::Buffer projectedErrorBuffer;
	vks::Buffer errorUniformBuffer;

	VkPipelineLayout pipelineLayout;

	VkRenderPass topViewRenderPass;
	VkRenderPass hwRastRenderPass;

	VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT imageAtomicInt64Feature{};

	VkFence inFlightFence;
	VkFence cullingFinishedFence;

	std::vector<VkCommandBuffer> rasterizeCommandBuffers;

	int vis_clusters_level = 0;
	int topViewWidth = width / 5, topViewHeight = height / 5;

	bool vis_topView = false;

	VulkanExample() : VulkanExampleBase(ENABLE_VALIDATION)
	{
		title = "Vulkanite: A dynamic LOD rendering pipeline";

		camera.type = Camera::CameraType::firstperson;
		camera.movementSpeed = 4.0f;
		camera.setPerspective(60.0f, (float)width / (float)height, 0.1f, 256.0f);
		camera.rotationSpeed = 0.25f;

		camera.setRotation({ -7.75f, 150.25f, 0.0f });
		camera.setPosition({ 0.7f, 0.1f, 1.7f });
	}

	~VulkanExample()
	{
		vkDestroyPipeline(device, pipelines.skybox, nullptr);
		vkDestroyPipeline(device, pipelines.pbr, nullptr);

		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		VulkanDescriptorSetManager::getManager()->destory();
		//vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

		uniformBuffers.object.destroy();
		uniformBuffers.skybox.destroy();
		uniformBuffers.params.destroy();
		uniformBuffers.topCube.destroy();
		uniformBuffers.topSkybox.destroy();
		modelMatsBuffer.destroy();

		textures.environmentCube.destroy();
		textures.irradianceCube.destroy();
		textures.prefilteredCube.destroy();
		textures.lutBrdf.destroy();
		textures.albedoMap.destroy();
		textures.normalMap.destroy();
		textures.aoMap.destroy();
		textures.metallicMap.destroy();
		textures.roughnessMap.destroy();
	}

	virtual void getEnabledFeatures()
	{
		if (deviceFeatures.samplerAnisotropy) {
			enabledFeatures.samplerAnisotropy = VK_TRUE;
		}
		if (deviceFeatures.fillModeNonSolid) {
			enabledFeatures.fillModeNonSolid = VK_TRUE;
		}
		if (deviceFeatures.geometryShader) {
			enabledFeatures.geometryShader = VK_TRUE;
		}
		if (deviceFeatures.shaderInt64) {
			enabledFeatures.shaderInt64 = VK_TRUE;
		}
		if (deviceFeatures.tessellationShader) {
			enabledFeatures.tessellationShader = VK_TRUE;
		}
		if (deviceFeatures.fragmentStoresAndAtomics) {
			enabledFeatures.fragmentStoresAndAtomics = VK_TRUE;
		}
	}

	virtual void getEnabledInstanceExtensions()
	{
		enabledInstanceExtensions.emplace_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
	}

	virtual void getEnabledDeviceExtensions()
	{
		enabledDeviceExtensions.emplace_back(VK_EXT_SHADER_IMAGE_ATOMIC_INT64_EXTENSION_NAME);
		enabledDeviceExtensions.emplace_back(VK_KHR_SHADER_ATOMIC_INT64_EXTENSION_NAME);
		
		imageAtomicInt64Feature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_IMAGE_ATOMIC_INT64_FEATURES_EXT;
		imageAtomicInt64Feature.shaderImageInt64Atomics = VK_TRUE;
		deviceCreatepNextChain = &imageAtomicInt64Feature;
	}

	void buildCommandBuffers()
	{
		VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();
		auto descManager = VulkanDescriptorSetManager::getManager();

		VkClearValue clearValues[2];
		clearValues[0].color = { { 0.1f, 0.1f, 0.1f, 1.0f } };
		clearValues[1].depthStencil = { 1.0f, 0 };

		VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
		renderPassBeginInfo.renderPass = renderPass;
		renderPassBeginInfo.renderArea.offset.x = 0;
		renderPassBeginInfo.renderArea.offset.y = 0;
		renderPassBeginInfo.renderArea.extent.width = width;
		renderPassBeginInfo.renderArea.extent.height = height;
		renderPassBeginInfo.clearValueCount = 2;
		renderPassBeginInfo.pClearValues = clearValues;

		for (size_t i = 0; i < drawCmdBuffers.size(); ++i)
		{
			// Set target frame buffer
			renderPassBeginInfo.framebuffer = frameBuffers[i];

			VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));

			/*
			* Naive BVH Traversal
			*/

			VkImageMemoryBarrier imageMemBarrier = vks::initializers::imageMemoryBarrier();
			imageMemBarrier.image = textures.hizbuffer.image;
			imageMemBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
			imageMemBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageMemBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			imageMemBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			imageMemBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageMemBarrier.subresourceRange.baseMipLevel = 0;
			imageMemBarrier.subresourceRange.levelCount = textures.hizbuffer.mipLevels;
			imageMemBarrier.subresourceRange.baseArrayLayer = 0;
			imageMemBarrier.subresourceRange.layerCount = 1;
			vkCmdPipelineBarrier(drawCmdBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, 0, 0, 0, 1, &imageMemBarrier);
			
			VkBufferCopy copyRegion = {};
			copyRegion.size = scene.initNodeInfoIndices.size() * sizeof(uint32_t);
			copyRegion.srcOffset = 0;
			copyRegion.dstOffset = 0;
			vkCmdCopyBuffer(drawCmdBuffers[i], initNodeInfosBuffer.buffer, currNodeInfosBuffer.buffer, 1, &copyRegion);
			
			VkBufferMemoryBarrier bufferBarrier = {};
			bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			bufferBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			bufferBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferBarrier.buffer = currNodeInfosBuffer.buffer;
			bufferBarrier.offset = 0;
			bufferBarrier.size = VK_WHOLE_SIZE;
			vkCmdPipelineBarrier(drawCmdBuffers[i], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &bufferBarrier, 0, nullptr);
			
			vkCmdFillBuffer(drawCmdBuffers[i], culledClusterIndicesBuffer.buffer, 0, 5 * sizeof(uint32_t), 0); // save the first 5 uint32_t for atomic counters
			bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			bufferBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			bufferBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferBarrier.buffer = culledClusterIndicesBuffer.buffer;
			bufferBarrier.offset = 0;
			bufferBarrier.size = VK_WHOLE_SIZE;
			vkCmdPipelineBarrier(drawCmdBuffers[i], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &bufferBarrier, 0, nullptr);
			//vkDeviceWaitIdle(device);
			for (size_t j = 0; j < scene.depthCounts.size(); j++)
			{
				// Refresh dst buffer
				vkCmdFillBuffer(drawCmdBuffers[i], (j & 1) ? currNodeInfosBuffer.buffer : nextNodeInfosBuffer.buffer, 0, sizeof(uint32_t), 0);
			
				VkBufferMemoryBarrier bufferBarrier = {};
				bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
				bufferBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				bufferBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
				bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				bufferBarrier.buffer = (j & 1) ? currNodeInfosBuffer.buffer : nextNodeInfosBuffer.buffer;
				bufferBarrier.offset = 0;
				bufferBarrier.size = VK_WHOLE_SIZE;
				vkCmdPipelineBarrier(drawCmdBuffers[i], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &bufferBarrier, 0, nullptr);
				
				vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, bvhTraversalPipeline);
				bvhTraversalPushConstants.threshold = thresholdInt / thresholdIntDiv;
				bvhTraversalPushConstants.screenSize = glm::vec2(width, height);
				vkCmdPushConstants(drawCmdBuffers[i], bvhTraversalPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(BVHTraversalPushConstants), &bvhTraversalPushConstants);
				vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, bvhTraversalPipelineLayout, 0, 1, &descManager->getSet("bvhTraversal", j & 1), 0, 0);
				vkCmdDispatch(drawCmdBuffers[i], (scene.depthCounts[j] + 31) / 32, 1, 1);
				
				// Add barrier for next src buffer
				// TODO: Consider how to launch the compute shader
				bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
				bufferBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
				bufferBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				bufferBarrier.buffer = (j & 1) ? currNodeInfosBuffer.buffer : nextNodeInfosBuffer.buffer;
				bufferBarrier.offset = 0;
				bufferBarrier.size = VK_WHOLE_SIZE;
				vkCmdPipelineBarrier(drawCmdBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &bufferBarrier, 0, nullptr);
			
				// Add barrier for next dst buffer, prevent it from early refreshing
				bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
				bufferBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
				bufferBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				bufferBarrier.buffer = (j & 1) ? nextNodeInfosBuffer.buffer: currNodeInfosBuffer.buffer;
				bufferBarrier.offset = 0;
				bufferBarrier.size = VK_WHOLE_SIZE;
				vkCmdPipelineBarrier(drawCmdBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &bufferBarrier, 0, nullptr);
			
			}
			
			bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			bufferBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			bufferBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferBarrier.buffer = culledClusterIndicesBuffer.buffer;
			bufferBarrier.offset = 0;
			bufferBarrier.size = VK_WHOLE_SIZE;
			vkCmdPipelineBarrier(drawCmdBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &bufferBarrier, 0, nullptr);

			bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			bufferBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			bufferBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferBarrier.buffer = culledClusterObjectIndicesBuffer.buffer;
			bufferBarrier.offset = 0;
			bufferBarrier.size = VK_WHOLE_SIZE;
			vkCmdPipelineBarrier(drawCmdBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &bufferBarrier, 0, nullptr);

			bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			bufferBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			bufferBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferBarrier.buffer = projectedErrorBuffer.buffer;
			bufferBarrier.offset = 0;
			bufferBarrier.size = VK_WHOLE_SIZE;

			vkCmdPipelineBarrier(drawCmdBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &bufferBarrier, 0, nullptr);

			/*
			*
			*  Screen space Error compute
			*
			*/
			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, errorProjPipeline);
			//errorPushConstants.numClusters = clusterinfos.size();
			errorPushConstants.numClusters = scene.maxClusterNum;
			errorPushConstants.screenSize = glm::vec2(width, height);
			vkCmdPushConstants(drawCmdBuffers[i], errorProjPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ErrorPushConstants), &errorPushConstants);
			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, errorProjPipelineLayout, 0, 1, &descManager->getSet("errorProj", 0), 0, 0);
			//vkDeviceWaitIdle(device);

			vkCmdDispatch(drawCmdBuffers[i], (errorPushConstants.numClusters + 31) / 32, 1, 1);

			bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			bufferBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			bufferBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferBarrier.buffer = projectedErrorBuffer.buffer;
			bufferBarrier.offset = 0;
			bufferBarrier.size = VK_WHOLE_SIZE;

			vkCmdPipelineBarrier(drawCmdBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &bufferBarrier, 0, nullptr);

			//VkImageMemoryBarrier imageMemBarrier = vks::initializers::imageMemoryBarrier();
			//imageMemBarrier.image = textures.hizbuffer.image;
			//imageMemBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
			//imageMemBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			//imageMemBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			//imageMemBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			//imageMemBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			//imageMemBarrier.subresourceRange.baseMipLevel = 0;
			//imageMemBarrier.subresourceRange.levelCount = textures.hizbuffer.mipLevels;
			//imageMemBarrier.subresourceRange.baseArrayLayer = 0;
			//imageMemBarrier.subresourceRange.layerCount = 1;
			//vkCmdPipelineBarrier(drawCmdBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, 0, 0, 0, 1, &imageMemBarrier);


			/*
			*
			*  Culling
			*
			*/
			bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			bufferBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
			bufferBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
			bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferBarrier.buffer = hwrDrawIndexedIndirectBuffer.buffer;
			bufferBarrier.offset = 0;
			bufferBarrier.size = VK_WHOLE_SIZE;
			vkCmdPipelineBarrier(drawCmdBuffers[i], VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &bufferBarrier, 0, nullptr);

			bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			bufferBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
			bufferBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
			bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferBarrier.buffer = swrNumVerticesBuffer.buffer;
			bufferBarrier.offset = 0;
			bufferBarrier.size = VK_WHOLE_SIZE;
			vkCmdPipelineBarrier(drawCmdBuffers[i], VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &bufferBarrier, 0, nullptr);


			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, cullingPipeline);
			//cullingPushConstants.numClusters = naniteMesh.meshes[0].clusters.size();
			cullingPushConstants.threshold = thresholdInt / thresholdIntDiv;
			cullingPushConstants.numClusters = scene.maxClusterNum;
			vkCmdPushConstants(drawCmdBuffers[i], cullingPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(CullingPushConstants), &cullingPushConstants);
			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, cullingPipelineLayout, 0, 1, &descManager->getSet("culling", 0), 0, 0);
			vkCmdDispatch(drawCmdBuffers[i], (cullingPushConstants.numClusters + 31) / 32, 1, 1);

			bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			bufferBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
			bufferBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
			bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferBarrier.buffer = hwrDrawIndexedIndirectBuffer.buffer;
			bufferBarrier.offset = 0;
			bufferBarrier.size = VK_WHOLE_SIZE;
			vkCmdPipelineBarrier(drawCmdBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr, 1, &bufferBarrier, 0, nullptr);

			bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			bufferBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
			bufferBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
			bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferBarrier.buffer = swrNumVerticesBuffer.buffer;
			bufferBarrier.offset = 0;
			bufferBarrier.size = VK_WHOLE_SIZE;
			vkCmdPipelineBarrier(drawCmdBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr, 1, &bufferBarrier, 0, nullptr);

			imageMemBarrier.image = textures.hizbuffer.image;
			imageMemBarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageMemBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
			imageMemBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			imageMemBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			imageMemBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageMemBarrier.subresourceRange.baseMipLevel = 0;
			imageMemBarrier.subresourceRange.levelCount = textures.hizbuffer.mipLevels;
			imageMemBarrier.subresourceRange.baseArrayLayer = 0;
			imageMemBarrier.subresourceRange.layerCount = 1;
			vkCmdPipelineBarrier(drawCmdBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, 0, 0, 0, 1, &imageMemBarrier);

			bufferBarrier = {};
			bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			bufferBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			bufferBarrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
			bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferBarrier.buffer = hwrDrawIndexedIndirectBuffer.buffer;
			bufferBarrier.offset = 0;
			bufferBarrier.size = VK_WHOLE_SIZE;

			vkCmdPipelineBarrier(drawCmdBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0, 0, nullptr, 1, &bufferBarrier, 0, nullptr);

			bufferBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			bufferBarrier.dstAccessMask = VK_ACCESS_INDEX_READ_BIT;
			bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferBarrier.buffer = HWRIndicesBuffer.buffer;
			bufferBarrier.offset = 0;
			bufferBarrier.size = VK_WHOLE_SIZE;
			vkCmdPipelineBarrier(drawCmdBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, nullptr, 1, &bufferBarrier, 0, nullptr);

			bufferBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			bufferBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferBarrier.buffer = HWRIDBuffer.buffer;
			bufferBarrier.offset = 0;
			bufferBarrier.size = VK_WHOLE_SIZE;
			vkCmdPipelineBarrier(drawCmdBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT, 0, 0, nullptr, 1, &bufferBarrier, 0, nullptr);


			bufferBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
			bufferBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferBarrier.buffer = swrNumVerticesBuffer.buffer;
			bufferBarrier.offset = 0;
			bufferBarrier.size = VK_WHOLE_SIZE;

			vkCmdPipelineBarrier(drawCmdBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &bufferBarrier, 0, nullptr);

			bufferBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
			bufferBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferBarrier.buffer = SWRIndicesBuffer.buffer;
			bufferBarrier.offset = 0;
			bufferBarrier.size = VK_WHOLE_SIZE;
			vkCmdPipelineBarrier(drawCmdBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &bufferBarrier, 0, nullptr);

			bufferBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			bufferBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferBarrier.buffer = SWRIDBuffer.buffer;
			bufferBarrier.offset = 0;
			bufferBarrier.size = VK_WHOLE_SIZE;
			vkCmdPipelineBarrier(drawCmdBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &bufferBarrier, 0, nullptr);

			VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));

			VK_CHECK_RESULT(vkBeginCommandBuffer(rasterizeCommandBuffers[i], &cmdBufInfo));

			/*
			*
			*  Software Rasterize
			*
			*/
			imageMemBarrier.image = SWRBuffer.image;
			imageMemBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
			imageMemBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
			imageMemBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
			imageMemBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			imageMemBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageMemBarrier.subresourceRange.baseMipLevel = 0;
			imageMemBarrier.subresourceRange.levelCount = 1;
			imageMemBarrier.subresourceRange.baseArrayLayer = 0;
			imageMemBarrier.subresourceRange.layerCount = 1;
			vkCmdPipelineBarrier(rasterizeCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, 0, 0, 0, 1, &imageMemBarrier);

			vkCmdBindPipeline(rasterizeCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, clearImagePipeline);
			vkCmdBindDescriptorSets(rasterizeCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, clearImagePipelineLayout, 0, 1, &descManager->getSet("clearImage", 0), 0, 0);
			vkCmdDispatch(rasterizeCommandBuffers[i], (width + workgroupX - 1) / workgroupX, (height + workgroupY - 1) / workgroupY, 1);

			bufferBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			bufferBarrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
			bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferBarrier.buffer = swrIndirectDispatchBuffer.buffer;
			bufferBarrier.offset = 0;
			bufferBarrier.size = VK_WHOLE_SIZE;
			vkCmdPipelineBarrier(rasterizeCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0, 0, nullptr, 1, &bufferBarrier, 0, nullptr);

			bufferBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
			bufferBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferBarrier.buffer = uniformBuffers.object.buffer;
			bufferBarrier.offset = 0;
			bufferBarrier.size = VK_WHOLE_SIZE;
			vkCmdPipelineBarrier(rasterizeCommandBuffers[i], VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &bufferBarrier, 0, nullptr);

			imageMemBarrier.image = SWRBuffer.image;
			imageMemBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
			imageMemBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
			imageMemBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			imageMemBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
			imageMemBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageMemBarrier.subresourceRange.baseMipLevel = 0;
			imageMemBarrier.subresourceRange.levelCount = 1;
			imageMemBarrier.subresourceRange.baseArrayLayer = 0;
			imageMemBarrier.subresourceRange.layerCount = 1;
			vkCmdPipelineBarrier(rasterizeCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, 0, 0, 0, 1, &imageMemBarrier);


			vkCmdBindPipeline(rasterizeCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, swrComputePipeline);
			vkCmdBindDescriptorSets(rasterizeCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, swrComputePipelineLayout, 0, 1, &descManager->getSet("swRast", 0), 0, 0);
			vkCmdDispatchIndirect(rasterizeCommandBuffers[i], swrIndirectDispatchBuffer.buffer, 0);
			//vkCmdDispatch(drawCmdBuffers[i], (scene.visibleIndicesCount / 3 + 31) / 32, 1, 1);

			imageMemBarrier.image = SWRBuffer.image;
			imageMemBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
			imageMemBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
			imageMemBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
			imageMemBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			imageMemBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageMemBarrier.subresourceRange.baseMipLevel = 0;
			imageMemBarrier.subresourceRange.levelCount = 1;
			imageMemBarrier.subresourceRange.baseArrayLayer = 0;
			imageMemBarrier.subresourceRange.layerCount = 1;
			vkCmdPipelineBarrier(rasterizeCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, 0, 0, 0, 1, &imageMemBarrier);


			/*
			* 
			*  Hardware Rasterize
			* 
			*/
			VkViewport viewport = vks::initializers::viewport((float)width, (float)height, 0.0f, 1.0f);
			VkRect2D scissor = vks::initializers::rect2D(width, height, 0, 0);
			VkClearValue clearValues1[2] = {};
			clearValues1[0].color.uint32[0] = UINT32_MAX;
			clearValues1[0].color.uint32[1] = UINT32_MAX;
			clearValues1[0].color.uint32[2] = UINT32_MAX;
			clearValues1[0].color.uint32[3] = UINT32_MAX;
			clearValues1[1].depthStencil = { 1.0f, 0 };
			VkRenderPassBeginInfo renderPassBeginInfo1 = vks::initializers::renderPassBeginInfo();
			renderPassBeginInfo1.framebuffer = HWRFramebuffer;
			renderPassBeginInfo1.renderPass = hwRastRenderPass;
			renderPassBeginInfo1.renderArea.offset.x = 0;
			renderPassBeginInfo1.renderArea.offset.y = 0;
			renderPassBeginInfo1.renderArea.extent.width = width;
			renderPassBeginInfo1.renderArea.extent.height = height;
			renderPassBeginInfo1.clearValueCount = 2;
			renderPassBeginInfo1.pClearValues = clearValues1;
			vkCmdBeginRenderPass(rasterizeCommandBuffers[i], &renderPassBeginInfo1, VK_SUBPASS_CONTENTS_INLINE);
			vkCmdBindPipeline(rasterizeCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, hwrastPipeline);
			vkCmdSetViewport(rasterizeCommandBuffers[i], 0, 1, &viewport);
			vkCmdSetScissor(rasterizeCommandBuffers[i], 0, 1, &scissor);
			vkCmdBindDescriptorSets(rasterizeCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, hwrastPipelineLayout, 0, 1, &descManager->getSet("hwRast", 0), 0, NULL);
			vkCmdBindIndexBuffer(rasterizeCommandBuffers[i], HWRIndicesBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
			VkDeviceSize offsets[1] = { 0 };
			vkCmdBindVertexBuffers(rasterizeCommandBuffers[i], 0, 1, &scene.vertices.buffer, offsets);
			vkCmdDrawIndexedIndirect(rasterizeCommandBuffers[i], hwrDrawIndexedIndirectBuffer.buffer, 0, 1, 0);
			vkCmdEndRenderPass(rasterizeCommandBuffers[i]);

			bufferBarrier.srcAccessMask = VK_ACCESS_INDEX_READ_BIT;
			bufferBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferBarrier.buffer = HWRIndicesBuffer.buffer;
			bufferBarrier.offset = 0;
			bufferBarrier.size = VK_WHOLE_SIZE;
			vkCmdPipelineBarrier(rasterizeCommandBuffers[i], VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &bufferBarrier, 0, nullptr);

			bufferBarrier.srcAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
			bufferBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferBarrier.buffer = HWRIndicesBuffer.buffer;
			bufferBarrier.offset = 0;
			bufferBarrier.size = VK_WHOLE_SIZE;
			vkCmdPipelineBarrier(rasterizeCommandBuffers[i], VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &bufferBarrier, 0, nullptr);

			imageMemBarrier.image = HWRZBuffer.image;
			imageMemBarrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			imageMemBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageMemBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			imageMemBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			imageMemBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			imageMemBarrier.subresourceRange.baseMipLevel = 0;
			imageMemBarrier.subresourceRange.levelCount = 1;
			imageMemBarrier.subresourceRange.baseArrayLayer = 0;
			imageMemBarrier.subresourceRange.layerCount = 1;
			vkCmdPipelineBarrier(rasterizeCommandBuffers[i], VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, 0, 0, 0, 1, &imageMemBarrier);

			imageMemBarrier.image = HWRVisBuffer.image;
			imageMemBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			imageMemBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
			imageMemBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			imageMemBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			imageMemBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageMemBarrier.subresourceRange.baseMipLevel = 0;
			imageMemBarrier.subresourceRange.levelCount = 1;
			imageMemBarrier.subresourceRange.baseArrayLayer = 0;
			imageMemBarrier.subresourceRange.layerCount = 1;
			vkCmdPipelineBarrier(rasterizeCommandBuffers[i], VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, 0, 0, 0, 1, &imageMemBarrier);

			

			/*
			*
			*  Merge Rasterize results
			*
			*/
			vkCmdPushConstants(rasterizeCommandBuffers[i], mergeRastPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(RenderingPushConstants), &renderingPushConstants);
			vkCmdBindPipeline(rasterizeCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, mergeRastPipeline);
			vkCmdBindDescriptorSets(rasterizeCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, mergeRastPipelineLayout, 0, 1, &descManager->getSet("mergeRast", 0), 0, NULL);
			vkCmdDispatch(rasterizeCommandBuffers[i], (width + workgroupX - 1) / workgroupX, (height + workgroupY - 1) / workgroupY, 1);

			imageMemBarrier.image = HWRZBuffer.image;
			imageMemBarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageMemBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			imageMemBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			imageMemBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			imageMemBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			imageMemBarrier.subresourceRange.baseMipLevel = 0;
			imageMemBarrier.subresourceRange.levelCount = 1;
			imageMemBarrier.subresourceRange.baseArrayLayer = 0;
			imageMemBarrier.subresourceRange.layerCount = 1;
			vkCmdPipelineBarrier(rasterizeCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, 0, 0, 0, 0, 0, 1, &imageMemBarrier);

			imageMemBarrier.image = HWRVisBuffer.image;
			imageMemBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
			imageMemBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			imageMemBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			imageMemBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			imageMemBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageMemBarrier.subresourceRange.baseMipLevel = 0;
			imageMemBarrier.subresourceRange.levelCount = 1;
			imageMemBarrier.subresourceRange.baseArrayLayer = 0;
			imageMemBarrier.subresourceRange.layerCount = 1;
			vkCmdPipelineBarrier(rasterizeCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, 0, 0, 0, 1, &imageMemBarrier);


			imageMemBarrier.image = FinalZBuffer.image;
			imageMemBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
			imageMemBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
			imageMemBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			imageMemBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			imageMemBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageMemBarrier.subresourceRange.baseMipLevel = 0;
			imageMemBarrier.subresourceRange.levelCount = 1;
			imageMemBarrier.subresourceRange.baseArrayLayer = 0;
			imageMemBarrier.subresourceRange.layerCount = 1;
			vkCmdPipelineBarrier(rasterizeCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, 0, 0, 0, 1, &imageMemBarrier);

			imageMemBarrier.image = FinalVisBuffer.image;
			imageMemBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
			imageMemBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
			imageMemBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			imageMemBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			imageMemBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageMemBarrier.subresourceRange.baseMipLevel = 0;
			imageMemBarrier.subresourceRange.levelCount = 1;
			imageMemBarrier.subresourceRange.baseArrayLayer = 0;
			imageMemBarrier.subresourceRange.layerCount = 1;
			vkCmdPipelineBarrier(rasterizeCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, 0, 0, 0, 1, &imageMemBarrier);

			imageMemBarrier.image = SWRBuffer.image;
			imageMemBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
			imageMemBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
			imageMemBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			imageMemBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
			imageMemBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageMemBarrier.subresourceRange.baseMipLevel = 0;
			imageMemBarrier.subresourceRange.levelCount = 1;
			imageMemBarrier.subresourceRange.baseArrayLayer = 0;
			imageMemBarrier.subresourceRange.layerCount = 1;
			vkCmdPipelineBarrier(rasterizeCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, 0, 0, 0, 1, &imageMemBarrier);

			

			/*
			*
			*  Shading
			*
			*/
			vkCmdBeginRenderPass(rasterizeCommandBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
			vkCmdSetViewport(rasterizeCommandBuffers[i], 0, 1, &viewport);
			vkCmdSetScissor(rasterizeCommandBuffers[i], 0, 1, &scissor);
			// Skybox
			if (displaySkybox)
			{
				vkCmdBindDescriptorSets(rasterizeCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descManager->getSet("objectDraw", 4), 0, NULL);
				vkCmdBindPipeline(rasterizeCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.skybox);
				vkCmdPushConstants(rasterizeCommandBuffers[i], pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(RenderingPushConstants), &renderingPushConstants);
				models.skybox.draw(rasterizeCommandBuffers[i]);
			}

			
			// Objects shading
			vkCmdBindPipeline(rasterizeCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, shadingPipeline);
			vkCmdBindDescriptorSets(rasterizeCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, shadingPipelineLayout, 0, 1, &descManager->getSet("shading", 0), 0, 0);
			vkCmdPushConstants(rasterizeCommandBuffers[i], shadingPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(RenderingPushConstants), &renderingPushConstants);
			vkCmdDraw(rasterizeCommandBuffers[i], 3, 1, 0, 0);

			// Objects
			//vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descManager->getSet("objectDraw", 0), 0, NULL);
			////vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descManager->getSet("objectDraw", 10), 0, NULL);
			//vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.pbr);
			////models.object.draw(drawCmdBuffers[i]);

			//if (renderingPushConstants.vis_clusters != 2)
			//{
			//	vkCmdBindVertexBuffers(drawCmdBuffers[i], 0, 1, &scene.vertices.buffer, offsets);
			//	vkCmdBindIndexBuffer(drawCmdBuffers[i], HWRIndicesBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
			//	//vkCmdBindIndexBuffer(drawCmdBuffers[i], instance1.indices.buffer, 0, VK_INDEX_TYPE_UINT32);
			//	vkCmdPushConstants(drawCmdBuffers[i], pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(RenderingPushConstants), &renderingPushConstants);
			//	vkCmdDrawIndexedIndirect(drawCmdBuffers[i], drawIndexedIndirectBuffer.buffer, 0, 1, 0);
			//}
			//else
			//{
			//	naniteMesh.meshes[vis_clusters_level].draw(drawCmdBuffers[i], 0, nullptr, 1);
			//}

			/*vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descManager->getSet("objectDraw", 2), 0, NULL);
			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.pbr);*/
			//naniteMesh.meshes[0].draw(drawCmdBuffers[i], 0, nullptr, 1);


			//vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descManager->getSet("objectDraw", 2), 0, NULL);
			//vkCmdPushConstants(drawCmdBuffers[i], pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(RenderingPushConstants), &renderingPushConstants);
			//models.cube.draw(drawCmdBuffers[i]);
			drawUI(rasterizeCommandBuffers[i]);
			vkCmdEndRenderPass(rasterizeCommandBuffers[i]);

			imageMemBarrier.image = FinalZBuffer.image;
			imageMemBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
			imageMemBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
			imageMemBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			imageMemBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			imageMemBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageMemBarrier.subresourceRange.baseMipLevel = 0;
			imageMemBarrier.subresourceRange.levelCount = 1;
			imageMemBarrier.subresourceRange.baseArrayLayer = 0;
			imageMemBarrier.subresourceRange.layerCount = 1;
			vkCmdPipelineBarrier(rasterizeCommandBuffers[i], VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, 0, 0, 0, 1, &imageMemBarrier);

			imageMemBarrier.image = FinalVisBuffer.image;
			imageMemBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
			imageMemBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
			imageMemBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			imageMemBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			imageMemBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageMemBarrier.subresourceRange.baseMipLevel = 0;
			imageMemBarrier.subresourceRange.levelCount = 1;
			imageMemBarrier.subresourceRange.baseArrayLayer = 0;
			imageMemBarrier.subresourceRange.layerCount = 1;
			vkCmdPipelineBarrier(rasterizeCommandBuffers[i], VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, 0, 0, 0, 1, &imageMemBarrier);

			/*
			*
			*  Depth copy
			*
			*/
			std::vector<VkImageMemoryBarrier> imageMemBarriers(1);
			imageMemBarriers[0] = vks::initializers::imageMemoryBarrier();
			imageMemBarriers[0].image = depthStencil.image;
			imageMemBarriers[0].oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			imageMemBarriers[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageMemBarriers[0].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			imageMemBarriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			imageMemBarriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
			imageMemBarriers[0].subresourceRange.baseMipLevel = 0;
			imageMemBarriers[0].subresourceRange.levelCount = 1;
			imageMemBarriers[0].subresourceRange.baseArrayLayer = 0;
			imageMemBarriers[0].subresourceRange.layerCount = 1;

			vkCmdPipelineBarrier(rasterizeCommandBuffers[i], VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, 0, 0, 0, imageMemBarriers.size(), imageMemBarriers.data());
			
			vkCmdBindPipeline(rasterizeCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, depthCopyPipeline);
			vkCmdBindDescriptorSets(rasterizeCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, depthCopyPipelineLayout, 0, 1, &descManager->getSet("depthCopy", 0), 0, 0);
			///vkDeviceWaitIdle(device);
			///std::cout << "444" << std::endl;
			///ASSERT(depthStencil.view != VK_NULL_HANDLE, "test");
			vkCmdDispatch(rasterizeCommandBuffers[i], (width + workgroupX - 1) / workgroupX, (height + workgroupY - 1) / workgroupY, 1);
			//std::cout << "45" << std::endl;

			imageMemBarriers[0] = vks::initializers::imageMemoryBarrier();
			imageMemBarriers[0].image = depthStencil.image;
			imageMemBarriers[0].oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageMemBarriers[0].newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			imageMemBarriers[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			imageMemBarriers[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			imageMemBarriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
			imageMemBarriers[0].subresourceRange.baseMipLevel = 0;
			imageMemBarriers[0].subresourceRange.levelCount = 1;
			imageMemBarriers[0].subresourceRange.baseArrayLayer = 0;
			imageMemBarriers[0].subresourceRange.layerCount = 1;
			vkCmdPipelineBarrier(rasterizeCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, 0, 0, 0, 0, 0, imageMemBarriers.size(), imageMemBarriers.data());


			imageMemBarriers[0] = vks::initializers::imageMemoryBarrier();
			imageMemBarriers[0].image = textures.hizbuffer.image;
			imageMemBarriers[0].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
			imageMemBarriers[0].newLayout = VK_IMAGE_LAYOUT_GENERAL;
			imageMemBarriers[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			imageMemBarriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			imageMemBarriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageMemBarriers[0].subresourceRange.baseMipLevel = 0;
			imageMemBarriers[0].subresourceRange.levelCount = 1;
			imageMemBarriers[0].subresourceRange.baseArrayLayer = 0;
			imageMemBarriers[0].subresourceRange.layerCount = 1;
			vkCmdPipelineBarrier(rasterizeCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, 0, 0, 0, 1, imageMemBarriers.data());

			/*
			*
			*  Top view
			*
			*/

			if(vis_topView)
			{
				renderPassBeginInfo.renderPass = topViewRenderPass;
				vkCmdBeginRenderPass(rasterizeCommandBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

				VkViewport viewport1 = vks::initializers::viewport(topViewWidth, topViewHeight, 0.0f, 1.0f);
				viewport1.x = width - topViewWidth;
				viewport1.y = height - topViewHeight;
				vkCmdSetViewport(rasterizeCommandBuffers[i], 0, 1, &viewport1);

				VkRect2D scissor1 = vks::initializers::rect2D(topViewWidth, topViewHeight, viewport1.x, viewport1.y);
				vkCmdSetScissor(rasterizeCommandBuffers[i], 0, 1, &scissor1);
				if (displaySkybox)
				{
					vkCmdBindDescriptorSets(rasterizeCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descManager->getSet("objectDraw", 5), 0, NULL);
					vkCmdBindPipeline(rasterizeCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.skybox);
					vkCmdPushConstants(rasterizeCommandBuffers[i], pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(RenderingPushConstants), &renderingPushConstants);
					models.skybox.draw(rasterizeCommandBuffers[i]);
				}
				vkCmdBindDescriptorSets(rasterizeCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descManager->getSet("objectDraw", 1), 0, NULL);
				vkCmdBindPipeline(rasterizeCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.pbr);
				//vkCmdBindVertexBuffers(drawCmdBuffers[i], 0, 1, &models.object.vertices.buffer, offsets);
				vkCmdBindVertexBuffers(rasterizeCommandBuffers[i], 0, 1, &scene.vertices.buffer, offsets);
				vkCmdBindIndexBuffer(rasterizeCommandBuffers[i], HWRIndicesBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
				vkCmdDrawIndexedIndirect(rasterizeCommandBuffers[i], hwrDrawIndexedIndirectBuffer.buffer, 0, 1, 0);

				vkCmdBindDescriptorSets(rasterizeCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descManager->getSet("objectDraw", 3), 0, NULL);
				vkCmdPushConstants(rasterizeCommandBuffers[i], pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(RenderingPushConstants), &renderingPushConstants);
				models.cube.draw(rasterizeCommandBuffers[i]);
				vkCmdEndRenderPass(rasterizeCommandBuffers[i]);
			}

			/*
			*  HZB build
			*/
			//vkDeviceWaitIdle(device);
			//std::cout << "555" << std::endl;

			vkCmdBindPipeline(rasterizeCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, hizComputePipeline);
			for (int j = 0; j < textures.hizbuffer.mipLevels - 1; j++)
			{
				vkCmdBindDescriptorSets(rasterizeCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, hizComputePipelineLayout, 0, 1, &descManager->getSet("hizBuild", j), 0, 0);
				vkCmdDispatch(rasterizeCommandBuffers[i], (width + workgroupX - 1) / workgroupX, (height + workgroupY - 1) / workgroupY, 1);
				VkImageMemoryBarrier imageMemBarrier = vks::initializers::imageMemoryBarrier();
				imageMemBarrier.image = textures.hizbuffer.image;
				imageMemBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
				imageMemBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
				imageMemBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
				imageMemBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				imageMemBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				imageMemBarrier.subresourceRange.baseMipLevel = j + 1;
				imageMemBarrier.subresourceRange.levelCount = 1;
				imageMemBarrier.subresourceRange.baseArrayLayer = 0;
				imageMemBarrier.subresourceRange.layerCount = 1;
				vkCmdPipelineBarrier(rasterizeCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, 0, 0, 0, 1, &imageMemBarrier);
			}

			/*
			*  HZB view
			*/

			if(0)
			{
				imageMemBarrier = vks::initializers::imageMemoryBarrier();
				imageMemBarrier.image = textures.hizbuffer.image;
				imageMemBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
				imageMemBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				imageMemBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
				imageMemBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				imageMemBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				imageMemBarrier.subresourceRange.baseMipLevel = 0;
				imageMemBarrier.subresourceRange.levelCount = textures.hizbuffer.mipLevels;
				imageMemBarrier.subresourceRange.baseArrayLayer = 0;
				imageMemBarrier.subresourceRange.layerCount = 1;
				vkCmdPipelineBarrier(rasterizeCommandBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, 0, 0, 0, 1, &imageMemBarrier);

				renderPassBeginInfo.renderPass = renderPass;
				vkCmdBeginRenderPass(rasterizeCommandBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
				vkCmdSetViewport(rasterizeCommandBuffers[i], 0, 1, &viewport);
				vkCmdSetScissor(rasterizeCommandBuffers[i], 0, 1, &scissor);
				vkCmdBindDescriptorSets(rasterizeCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, debugQuadPipelineLayout, 0, 1, &descManager->getSet("debugQuad", 0), 0, NULL);
				vkCmdBindPipeline(rasterizeCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, debugQuadPipeline);
				vkCmdDraw(rasterizeCommandBuffers[i], 3, 1, 0, 0);
				vkCmdEndRenderPass(rasterizeCommandBuffers[i]);

				imageMemBarrier.image = textures.hizbuffer.image;
				imageMemBarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				imageMemBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
				imageMemBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
				imageMemBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
				imageMemBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				imageMemBarrier.subresourceRange.baseMipLevel = 0;
				imageMemBarrier.subresourceRange.levelCount = textures.hizbuffer.mipLevels;
				imageMemBarrier.subresourceRange.baseArrayLayer = 0;
				imageMemBarrier.subresourceRange.layerCount = 1;
				vkCmdPipelineBarrier(rasterizeCommandBuffers[i], VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, 0, 0, 0, 1, &imageMemBarrier);
			}

			VK_CHECK_RESULT(vkEndCommandBuffer(rasterizeCommandBuffers[i]));
		}
		//ASSERT(false, "debug interrupt");
	}

	void loadAssets()
	{
		const uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::PreMultiplyVertexColors | vkglTF::FileLoadingFlags::FlipY;
		models.skybox.loadFromFile(getAssetPath() + "models/cube.gltf", vulkanDevice, queue, glTFLoadingFlags);
		//models.object.loadFromFile(getAssetPath() + "models/cerberus/cerberus.gltf", vulkanDevice, queue, glTFLoadingFlags);
		//reducedModel.generateClusterInfos(models.object, vulkanDevice, queue);
		
		models.object.loadFromFile(getAssetPath() + "models/dragon.gltf", vulkanDevice, queue, glTFLoadingFlags);
		naniteMesh.setModelPath((getAssetPath() + "models/dragon/").c_str());
		naniteMesh.loadvkglTFModel(models.object);
		naniteMesh.initNaniteInfo(getAssetPath() + "models/dragon.gltf", true);

		for (int i = 0; i < naniteMesh.meshes.size(); i++)
		{
			naniteMesh.meshes[i].initUniqueVertexBuffer();
			naniteMesh.meshes[i].initVertexBuffer();
			naniteMesh.meshes[i].createVertexBuffer(vulkanDevice, queue);
		}
		scene.naniteMeshes.emplace_back(naniteMesh);

		// Uncomment this part for performance test scene
		modelMats.clear();
		for (int i = -17; i <= 17; i++)
		{
			for (int j = -17; j <= 17; j++) 
			{
				auto& modelMat = glm::translate(glm::mat4(1.0f), glm::vec3(i * 3, 1.2f, j * 3));
				auto& instance = Instance(&naniteMesh, modelMat);
				modelMats.emplace_back(modelMat);
				scene.naniteObjects.emplace_back(instance);
			}
		}

		// Uncomment this part for normal scene
		//auto & instance1 = Instance(&naniteMesh, modelMats[0]);
		//auto & instance2 = Instance(&naniteMesh, modelMats[1]);
		////auto & instance3 = Instance(&naniteMesh, modelMats[2]);
		//scene.naniteObjects.push_back(instance1);
		//scene.naniteObjects.push_back(instance2);
		////scene.naniteObjects.push_back(instance3);

		// Uncomment this part for normal multi-mesh scene
		/*auto & instance1 = Instance(&naniteMesh, modelMats[0]);
		scene.naniteObjects.push_back(instance1);
		NaniteMesh naniteMesh2;
		naniteMesh2.setModelPath((getAssetPath() + "models/bunny/").c_str());
		naniteMesh2.loadvkglTFModel(models.object);
		naniteMesh2.initNaniteInfo(getAssetPath() + "models/bunny.gltf", true);
		for (int i = 0; i < naniteMesh2.meshes.size(); i++)
		{
			naniteMesh2.meshes[i].initUniqueVertexBuffer();
			naniteMesh2.meshes[i].initVertexBuffer();
			naniteMesh2.meshes[i].createVertexBuffer(vulkanDevice, queue);
		}
		scene.naniteMeshes.push_back(naniteMesh2);
		auto& instance2 = Instance(&naniteMesh2, modelMats[1]);
		scene.naniteObjects.push_back(instance2);*/

		// Uncomment this part for performance test multi-mesh scene
		//NaniteMesh naniteMesh2;
		//naniteMesh2.setModelPath((getAssetPath() + "models/bunny/").c_str());
		//naniteMesh2.loadvkglTFModel(models.object);
		//naniteMesh2.initNaniteInfo(getAssetPath() + "models/bunny.gltf", true);
		//for (int i = 0; i < naniteMesh2.meshes.size(); i++)
		//{
		//	naniteMesh2.meshes[i].initUniqueVertexBuffer();
		//	naniteMesh2.meshes[i].initVertexBuffer();
		//	naniteMesh2.meshes[i].createVertexBuffer(vulkanDevice, queue);
		//}
		//scene.naniteMeshes.push_back(naniteMesh2);
		//
		//modelMats.clear();
		//for (int i = -2; i <= 2; i++)
		//{
		//	for (int j = -2; j <= 2; j++) 
		//	{
		//		auto& modelMat = glm::translate(glm::mat4(1.0f), glm::vec3(i * 3, j * 3, 0.0f));
		//		if (i % 2) {
		//			auto& instance = Instance(&naniteMesh, modelMat);
		//			scene.naniteObjects.push_back(instance);
		//		}
		//		else{
		//			auto& instance = Instance(&naniteMesh2, modelMat);
		//			scene.naniteObjects.push_back(instance);
		//		}
		//		modelMats.push_back(modelMat);
		//
		//	}
		//}

		////
		scene.createNaniteSceneInfo(vulkanDevice, queue);
		//reducedModel.simplifyModel(vulkanDevice, queue);
		textures.environmentCube.loadFromFile(getAssetPath() + "textures/hdr/gcanyon_cube.ktx", VK_FORMAT_R16G16B16A16_SFLOAT, vulkanDevice, queue);
		textures.albedoMap.loadFromFile(getAssetPath() + "models/cerberus/albedo.ktx", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, queue);
		textures.normalMap.loadFromFile(getAssetPath() + "models/cerberus/normal.ktx", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, queue);
		textures.aoMap.loadFromFile(getAssetPath() + "models/cerberus/ao.ktx", VK_FORMAT_R8_UNORM, vulkanDevice, queue);
		textures.metallicMap.loadFromFile(getAssetPath() + "models/cerberus/metallic.ktx", VK_FORMAT_R8_UNORM, vulkanDevice, queue);
		textures.roughnessMap.loadFromFile(getAssetPath() + "models/cerberus/roughness.ktx", VK_FORMAT_R8_UNORM, vulkanDevice, queue);

		models.cube.loadFromFile(getAssetPath() + "models/cube.gltf", vulkanDevice, queue, glTFLoadingFlags);
	}

	void setupDescriptors()
	{
		
		auto manager = VulkanDescriptorSetManager::getManager();
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings;
		setLayoutBindings = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 3),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 4),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 5),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 6),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 7),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 8),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 9),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT, 10),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT, 11),
		};
		manager->addSetLayout("objectDraw", setLayoutBindings, 6);

		setLayoutBindings = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 0),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 1),
		};
		manager->addSetLayout("hizBuild", setLayoutBindings, textures.hizbuffer.mipLevels - 1);

		setLayoutBindings = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
		};
		manager->addSetLayout("debugQuad", setLayoutBindings, 1);

		setLayoutBindings = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 0),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 1)
		};
		manager->addSetLayout("depthCopy", setLayoutBindings, 1);

		setLayoutBindings = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 0),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 2),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 3),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 4),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 5),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 6),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 7),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 8),
		};
		manager->addSetLayout("bvhTraversal", setLayoutBindings, 2);

		setLayoutBindings = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 0),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 2),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 3),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 4),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 5),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 6),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 7),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 8),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 9),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 10),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 11),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 12),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 13),
		};
		manager->addSetLayout("culling", setLayoutBindings, 1);

		setLayoutBindings = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 0),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 2),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 3),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 4),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 5),
		};
		manager->addSetLayout("errorProj", setLayoutBindings, 1);

		setLayoutBindings = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_GEOMETRY_BIT, 0),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_GEOMETRY_BIT, 1),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_GEOMETRY_BIT, 2),
		};
		manager->addSetLayout("hwRast", setLayoutBindings, 1);

		setLayoutBindings = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 0),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 2),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 3),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 4),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 5),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 6),
		};
		manager->addSetLayout("swRast", setLayoutBindings, 1);

		setLayoutBindings = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 0),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 2),
		};
		manager->addSetLayout("clearImage", setLayoutBindings, 1);

		setLayoutBindings = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 0),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 1),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 2),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 3),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 4),
		};
		manager->addSetLayout("mergeRast", setLayoutBindings, 1);

		setLayoutBindings = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 2),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 3),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT, 4),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT, 5),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 6),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 7),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 8),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 9),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 10),
		};
		manager->addSetLayout("shading", setLayoutBindings, 1);

		manager->createLayoutsAndSets(device);


		//culledObjectIndicesBuffer.setupDescriptor();
		modelMatsBuffer.setupDescriptor();

		//Object camera view
		manager->writeToSet("objectDraw", 0, 0, &uniformBuffers.object.descriptor);
		manager->writeToSet("objectDraw", 0, 1, &uniformBuffers.params.descriptor);
		manager->writeToSet("objectDraw", 0, 2, &textures.irradianceCube.descriptor);
		manager->writeToSet("objectDraw", 0, 3, &textures.lutBrdf.descriptor);
		manager->writeToSet("objectDraw", 0, 4, &textures.prefilteredCube.descriptor);
		manager->writeToSet("objectDraw", 0, 5, &textures.albedoMap.descriptor);
		manager->writeToSet("objectDraw", 0, 6, &textures.normalMap.descriptor);
		manager->writeToSet("objectDraw", 0, 7, &textures.aoMap.descriptor);
		manager->writeToSet("objectDraw", 0, 8, &textures.metallicMap.descriptor);
		manager->writeToSet("objectDraw", 0, 9, &textures.roughnessMap.descriptor);
		//manager->writeToSet("objectDraw", 0, 10, &culledObjectIndicesBuffer.descriptor);
		manager->writeToSet("objectDraw", 0, 11, &modelMatsBuffer.descriptor);


		//Object top view
		manager->writeToSet("objectDraw", 1, 0, &uniformBuffers.topObject.descriptor);
		manager->writeToSet("objectDraw", 1, 1, &uniformBuffers.params.descriptor);
		manager->writeToSet("objectDraw", 1, 2, &textures.irradianceCube.descriptor);
		manager->writeToSet("objectDraw", 1, 3, &textures.lutBrdf.descriptor);
		manager->writeToSet("objectDraw", 1, 4, &textures.prefilteredCube.descriptor);
		manager->writeToSet("objectDraw", 1, 5, &textures.albedoMap.descriptor);
		manager->writeToSet("objectDraw", 1, 6, &textures.normalMap.descriptor);
		manager->writeToSet("objectDraw", 1, 7, &textures.aoMap.descriptor);
		manager->writeToSet("objectDraw", 1, 8, &textures.metallicMap.descriptor);
		manager->writeToSet("objectDraw", 1, 9, &textures.roughnessMap.descriptor);
		//manager->writeToSet("objectDraw", 1, 10, &culledObjectIndicesBuffer.descriptor);
		manager->writeToSet("objectDraw", 1, 11, &modelMatsBuffer.descriptor);

		//Cube camera view
		manager->writeToSet("objectDraw", 2, 0, &uniformBuffers.cube.descriptor);
		manager->writeToSet("objectDraw", 2, 1, &uniformBuffers.params.descriptor);
		manager->writeToSet("objectDraw", 2, 2, &textures.irradianceCube.descriptor);
		manager->writeToSet("objectDraw", 2, 3, &textures.lutBrdf.descriptor);
		manager->writeToSet("objectDraw", 2, 4, &textures.prefilteredCube.descriptor);
		manager->writeToSet("objectDraw", 2, 5, &textures.albedoMap.descriptor);
		manager->writeToSet("objectDraw", 2, 6, &textures.normalMap.descriptor);
		manager->writeToSet("objectDraw", 2, 7, &textures.aoMap.descriptor);
		manager->writeToSet("objectDraw", 2, 8, &textures.metallicMap.descriptor);
		manager->writeToSet("objectDraw", 2, 9, &textures.roughnessMap.descriptor);
		//manager->writeToSet("objectDraw", 2, 10, &culledObjectIndicesBuffer.descriptor);
		manager->writeToSet("objectDraw", 2, 11, &modelMatsBuffer.descriptor);
		//Cube top view
		manager->writeToSet("objectDraw", 3, 0, &uniformBuffers.topCube.descriptor);
		manager->writeToSet("objectDraw", 3, 1, &uniformBuffers.params.descriptor);
		manager->writeToSet("objectDraw", 3, 2, &textures.irradianceCube.descriptor);
		manager->writeToSet("objectDraw", 3, 3, &textures.lutBrdf.descriptor);
		manager->writeToSet("objectDraw", 3, 4, &textures.prefilteredCube.descriptor);
		manager->writeToSet("objectDraw", 3, 5, &textures.albedoMap.descriptor);
		manager->writeToSet("objectDraw", 3, 6, &textures.normalMap.descriptor);
		manager->writeToSet("objectDraw", 3, 7, &textures.aoMap.descriptor);
		manager->writeToSet("objectDraw", 3, 8, &textures.metallicMap.descriptor);
		manager->writeToSet("objectDraw", 3, 9, &textures.roughnessMap.descriptor);
		//manager->writeToSet("objectDraw", 3, 10, &culledObjectIndicesBuffer.descriptor);
		manager->writeToSet("objectDraw", 3, 11, &modelMatsBuffer.descriptor);

		//Skybox camera view
		manager->writeToSet("objectDraw", 4, 0, &uniformBuffers.skybox.descriptor);
		manager->writeToSet("objectDraw", 4, 1, &uniformBuffers.params.descriptor);
		manager->writeToSet("objectDraw", 4, 2, &textures.environmentCube.descriptor);

		//Skybox top view
		manager->writeToSet("objectDraw", 5, 0, &uniformBuffers.topSkybox.descriptor);
		manager->writeToSet("objectDraw", 5, 1, &uniformBuffers.params.descriptor);
		manager->writeToSet("objectDraw", 5, 2, &textures.environmentCube.descriptor);

		//Hiz building
		for (int i = 0; i < textures.hizbuffer.mipLevels - 1; i++)
		{
			VkDescriptorImageInfo inputImageInfo = {};
			inputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			inputImageInfo.imageView = hizImageViews[i];
			VkDescriptorImageInfo outputImageInfo = {};
			outputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			outputImageInfo.imageView = hizImageViews[i + 1];
			manager->writeToSet("hizBuild", i, 0, &inputImageInfo);
			manager->writeToSet("hizBuild", i, 1, &outputImageInfo);
		}

		//Debug quad 
		manager->writeToSet("debugQuad", 0, 0, &textures.hizbuffer.descriptor);

		//Depth copy
		VkDescriptorImageInfo depthImageInfo = {};
		depthImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		depthImageInfo.imageView = FinalZBuffer.view;
		VkDescriptorImageInfo outputImageInfo = {};
		outputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		outputImageInfo.imageView = hizImageViews[0];
		manager->writeToSet("depthCopy", 0, 0, &depthImageInfo);
		manager->writeToSet("depthCopy", 0, 1, &outputImageInfo);

		//BVH Traversal
		bvhNodeInfosBuffer.setupDescriptor();
		currNodeInfosBuffer.setupDescriptor();
		nextNodeInfosBuffer.setupDescriptor();
		culledClusterIndicesBuffer.setupDescriptor();
		cullingUniformBuffer.setupDescriptor();
		errorUniformBuffer.setupDescriptor();
		culledClusterObjectIndicesBuffer.setupDescriptor();
		sortedClusterIndicesBuffer.setupDescriptor();
		manager->writeToSet("bvhTraversal", 0, 0, &bvhNodeInfosBuffer.descriptor);
		manager->writeToSet("bvhTraversal", 0, 1, &currNodeInfosBuffer.descriptor);
		manager->writeToSet("bvhTraversal", 0, 2, &nextNodeInfosBuffer.descriptor);
		manager->writeToSet("bvhTraversal", 0, 3, &culledClusterIndicesBuffer.descriptor);
		manager->writeToSet("bvhTraversal", 0, 4, &cullingUniformBuffer.descriptor);
		manager->writeToSet("bvhTraversal", 0, 5, &textures.hizbuffer.descriptor);
		manager->writeToSet("bvhTraversal", 0, 6, &errorUniformBuffer.descriptor);
		manager->writeToSet("bvhTraversal", 0, 7, &culledClusterObjectIndicesBuffer.descriptor);
		manager->writeToSet("bvhTraversal", 0, 8, &sortedClusterIndicesBuffer.descriptor);
		
		manager->writeToSet("bvhTraversal", 1, 0, &bvhNodeInfosBuffer.descriptor);
		manager->writeToSet("bvhTraversal", 1, 1, &nextNodeInfosBuffer.descriptor);
		manager->writeToSet("bvhTraversal", 1, 2, &currNodeInfosBuffer.descriptor);
		manager->writeToSet("bvhTraversal", 1, 3, &culledClusterIndicesBuffer.descriptor);
		manager->writeToSet("bvhTraversal", 1, 4, &cullingUniformBuffer.descriptor);
		manager->writeToSet("bvhTraversal", 1, 5, &textures.hizbuffer.descriptor);
		manager->writeToSet("bvhTraversal", 1, 6, &errorUniformBuffer.descriptor);
		manager->writeToSet("bvhTraversal", 1, 7, &culledClusterObjectIndicesBuffer.descriptor);
		manager->writeToSet("bvhTraversal", 1, 8, &sortedClusterIndicesBuffer.descriptor);

		//Culling
		clustersInfoBuffer.setupDescriptor();
		HWRIndicesBuffer.setupDescriptor();
		HWRIDBuffer.setupDescriptor();
		hwrDrawIndexedIndirectBuffer.setupDescriptor();
		cullingUniformBuffer.setupDescriptor();
		projectedErrorBuffer.setupDescriptor();
		SWRIndicesBuffer.setupDescriptor();
		SWRIDBuffer.setupDescriptor();
		swrNumVerticesBuffer.setupDescriptor();

		//culledObjectIndicesBuffer.setupDescriptor();
		culledClusterObjectIndicesBuffer.setupDescriptor();
		modelMatsBuffer.setupDescriptor();
		VkDescriptorBufferInfo inputIndicesInfo{};
		inputIndicesInfo.buffer = scene.indices.buffer;
		inputIndicesInfo.range = VK_WHOLE_SIZE;
		manager->writeToSet("culling", 0, 0, &clustersInfoBuffer.descriptor);
		manager->writeToSet("culling", 0, 1, &inputIndicesInfo);
		manager->writeToSet("culling", 0, 2, &HWRIndicesBuffer.descriptor);
		manager->writeToSet("culling", 0, 3, &hwrDrawIndexedIndirectBuffer.descriptor);
		manager->writeToSet("culling", 0, 4, &cullingUniformBuffer.descriptor);
		manager->writeToSet("culling", 0, 5, &textures.hizbuffer.descriptor);
		manager->writeToSet("culling", 0, 6, &projectedErrorBuffer.descriptor);
		manager->writeToSet("culling", 0, 7, &HWRIDBuffer.descriptor);
		manager->writeToSet("culling", 0, 8, &SWRIndicesBuffer.descriptor);
		manager->writeToSet("culling", 0, 9, &SWRIDBuffer.descriptor);
		manager->writeToSet("culling", 0, 10, &swrNumVerticesBuffer.descriptor);
		manager->writeToSet("culling", 0, 11, &culledClusterIndicesBuffer.descriptor);
		manager->writeToSet("culling", 0, 12, &culledClusterObjectIndicesBuffer.descriptor);
		manager->writeToSet("culling", 0, 13, &modelMatsBuffer.descriptor);

		//Error projection
		errorInfoBuffer.setupDescriptor();
		projectedErrorBuffer.setupDescriptor();
		errorUniformBuffer.setupDescriptor();
		culledClusterObjectIndicesBuffer.setupDescriptor();
		modelMatsBuffer.setupDescriptor();
		manager->writeToSet("errorProj", 0, 0, &errorInfoBuffer.descriptor);
		manager->writeToSet("errorProj", 0, 1, &projectedErrorBuffer.descriptor);
		manager->writeToSet("errorProj", 0, 2, &errorUniformBuffer.descriptor);
		manager->writeToSet("errorProj", 0, 3, &culledClusterIndicesBuffer.descriptor);
		manager->writeToSet("errorProj", 0, 4, &culledClusterObjectIndicesBuffer.descriptor);
		manager->writeToSet("errorProj", 0, 5, &modelMatsBuffer.descriptor);

		//Hardware Rasterization
		manager->writeToSet("hwRast", 0, 0, &modelMatsBuffer.descriptor);
		manager->writeToSet("hwRast", 0, 1, &HWRIDBuffer.descriptor);
		manager->writeToSet("hwRast", 0, 2, &uniformBuffers.object.descriptor);

		//Software Rasterization
		VkDescriptorBufferInfo inputVertInfo{};
		inputVertInfo.buffer = scene.vertices.buffer;
		inputVertInfo.range = VK_WHOLE_SIZE;
		VkDescriptorImageInfo SWRImageInfo = {};
		SWRImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		SWRImageInfo.imageView = SWRBuffer.view;
		swrIndirectDispatchBuffer.setupDescriptor();
		manager->writeToSet("swRast", 0, 0, &inputVertInfo);
		manager->writeToSet("swRast", 0, 1, &SWRIndicesBuffer.descriptor);
		manager->writeToSet("swRast", 0, 2, &modelMatsBuffer.descriptor);
		manager->writeToSet("swRast", 0, 3, &SWRIDBuffer.descriptor);
		manager->writeToSet("swRast", 0, 4, &swrNumVerticesBuffer.descriptor);
		manager->writeToSet("swRast", 0, 5, &SWRImageInfo);
		manager->writeToSet("swRast", 0, 6, &uniformBuffers.object.descriptor);

		//Clear image
		manager->writeToSet("clearImage", 0, 0, &SWRImageInfo);
		manager->writeToSet("clearImage", 0, 1, &swrNumVerticesBuffer.descriptor);
		manager->writeToSet("clearImage", 0, 2, &swrIndirectDispatchBuffer.descriptor);

		//Merge Rasterization Result
		VkDescriptorImageInfo HWRImageInfo = {};
		HWRImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		HWRImageInfo.imageView = HWRZBuffer.view;
		HWRImageInfo.sampler = HWRZBuffer.sampler;
		manager->writeToSet("mergeRast", 0, 0, &HWRImageInfo);
		HWRImageInfo.sampler = 0;
		HWRImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		HWRImageInfo.imageView = HWRVisBuffer.view;
		manager->writeToSet("mergeRast", 0, 1, &HWRImageInfo);
		manager->writeToSet("mergeRast", 0, 2, &SWRImageInfo);
		HWRImageInfo.sampler = 0;
		HWRImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		HWRImageInfo.imageView = FinalZBuffer.view;
		manager->writeToSet("mergeRast", 0, 3, &HWRImageInfo);
		HWRImageInfo.sampler = 0;
		HWRImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		HWRImageInfo.imageView = FinalVisBuffer.view;
		manager->writeToSet("mergeRast", 0, 4, &HWRImageInfo);


		//Shading
		manager->writeToSet("shading", 0, 0, &clustersInfoBuffer.descriptor);
		
		manager->writeToSet("shading", 0, 1, &inputVertInfo);
		manager->writeToSet("shading", 0, 2, &inputIndicesInfo);
		manager->writeToSet("shading", 0, 3, &modelMatsBuffer.descriptor);
		HWRImageInfo.sampler = 0;
		HWRImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		HWRImageInfo.imageView = FinalVisBuffer.view;
		manager->writeToSet("shading", 0, 4, &HWRImageInfo);
		HWRImageInfo.sampler = 0;
		HWRImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		HWRImageInfo.imageView = FinalZBuffer.view;
		manager->writeToSet("shading", 0, 5, &HWRImageInfo);
		uniformBuffers.shadingMats.setupDescriptor();
		manager->writeToSet("shading", 0, 6, &uniformBuffers.shadingMats.descriptor);
		manager->writeToSet("shading", 0, 7, &uniformBuffers.params.descriptor);
		manager->writeToSet("shading", 0, 8, &textures.irradianceCube.descriptor);
		manager->writeToSet("shading", 0, 9, &textures.lutBrdf.descriptor);
		manager->writeToSet("shading", 0, 10, &textures.prefilteredCube.descriptor);

		//ASSERT(false, "debug");
	}

	void preparePipelines()
	{
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
		VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1);
		VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
		std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
		std::array<VkPipelineShaderStageCreateInfo, 3> shaderStages;
		std::array<VkPipelineShaderStageCreateInfo, 3> pbrShaderStages;


		auto descManager = VulkanDescriptorSetManager::getManager();
		// Pipeline layout
		VkPushConstantRange push_constant{};
		push_constant.size = sizeof(RenderingPushConstants);
		push_constant.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&descManager->getSetLayout("objectDraw"), 1);
		pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
		pipelineLayoutCreateInfo.pPushConstantRanges = &push_constant;
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));

		// Pipelines
		VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(pipelineLayout, renderPass);
		pipelineCI.pInputAssemblyState = &inputAssemblyState;
		pipelineCI.pRasterizationState = &rasterizationState;
		pipelineCI.pColorBlendState = &colorBlendState;
		pipelineCI.pMultisampleState = &multisampleState;
		pipelineCI.pViewportState = &viewportState;
		pipelineCI.pDepthStencilState = &depthStencilState;
		pipelineCI.pDynamicState = &dynamicState;
		//pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineCI.stageCount = 2;
		pipelineCI.pStages = shaderStages.data();
		pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ 
			vkglTF::VertexComponent::Position, 
			vkglTF::VertexComponent::Normal, 
			vkglTF::VertexComponent::UV, 
			vkglTF::VertexComponent::Tangent,
			vkglTF::VertexComponent::Joint0,
			vkglTF::VertexComponent::Weight0});

		// Skybox pipeline (background cube)
		rasterizationState.cullMode = VK_CULL_MODE_FRONT_BIT;
		shaderStages[0] = loadShader(getShadersPath() + "pbrtexture/skybox.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "pbrtexture/skybox.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.skybox));

		pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
		//pipelineCI.pStages = pbrShaderStages.data();
		// PBR pipeline
		rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
		//rasterizationState.cullMode = VK_CULL_MODE_NONE;
		shaderStages[0] = loadShader(getShadersPath() + "pbrtexture/pbrtexture.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "pbrtexture/pbrtexture.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		shaderStages[2] = loadShader(getShadersPath() + "pbrtexture/pbrtexture.geom.spv", VK_SHADER_STAGE_GEOMETRY_BIT);
		// Enable depth test and write
		depthStencilState.depthWriteEnable = VK_TRUE;
		depthStencilState.depthTestEnable = VK_TRUE;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.pbr));
		
		pipelineCI.stageCount = 2;

		{
			// Top view pipeline
			rasterizationState.cullMode = VK_CULL_MODE_NONE;
			pipelineCI.renderPass = topViewRenderPass;
			pipelineCI.layout = pipelineLayout;
			VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.topView));
		}

		{
			// Hardware Rasterize pipeline
			pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&descManager->getSetLayout("hwRast"), 1);
			VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &hwrastPipelineLayout));
			pipelineCI.layout = hwrastPipelineLayout;
			pipelineCI.renderPass = hwRastRenderPass;
			std::array<VkPipelineShaderStageCreateInfo, 3> shaderStages1;
			shaderStages1[0] = loadShader(getShadersPath() + "pbrtexture/hwrasterize.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
			shaderStages1[1] = loadShader(getShadersPath() + "pbrtexture/hwrasterize.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
			shaderStages1[2] = loadShader(getShadersPath() + "pbrtexture/hwrasterize.geom.spv", VK_SHADER_STAGE_GEOMETRY_BIT);
			pipelineCI.pStages = shaderStages1.data();
			pipelineCI.stageCount = 3;
			rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
			//VkPipelineTessellationStateCreateInfo tessellationState = {};
			//tessellationState.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
			//tessellationState.patchControlPoints = 3; // Number of control points per patch
			//pipelineCI.pTessellationState = &tessellationState;
			//inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
			VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &hwrastPipeline));
			//pipelineCI.pTessellationState = nullptr;
			//inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		}

		{
			// Debug Draw Quad pipeline
			pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&descManager->getSetLayout("debugQuad"), 1);
			VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &debugQuadPipelineLayout));
			pipelineCI.layout = debugQuadPipelineLayout;
			pipelineCI.renderPass = renderPass;
			depthStencilState.depthWriteEnable = VK_FALSE;
			depthStencilState.depthTestEnable = VK_FALSE;
			VkPipelineVertexInputStateCreateInfo vertexInputStateCI = {};
			vertexInputStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
			/*vertexInputStateCI.vertexBindingDescriptionCount = 0;
			vertexInputStateCI.pVertexBindingDescriptions = nullptr;
			vertexInputStateCI.vertexAttributeDescriptionCount = 0;
			vertexInputStateCI.pVertexAttributeDescriptions = nullptr;*/
			pipelineCI.pVertexInputState = &vertexInputStateCI;
			rasterizationState.cullMode = VK_CULL_MODE_NONE;
			shaderStages[0] = loadShader(getShadersPath() + "pbrtexture/debugquad.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
			shaderStages[1] = loadShader(getShadersPath() + "pbrtexture/debugquad.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
			pipelineCI.stageCount = 2;
			pipelineCI.pStages = shaderStages.data();
			VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &debugQuadPipeline));
		}
		
		{
			// Shading pipeline
			push_constant.size = sizeof(RenderingPushConstants);
			push_constant.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
			pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&descManager->getSetLayout("shading"), 1);
			pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
			pipelineLayoutCreateInfo.pPushConstantRanges = &push_constant;
			VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &shadingPipelineLayout));

			pipelineCI.layout = shadingPipelineLayout;
			pipelineCI.renderPass = renderPass;
			shaderStages[0] = loadShader(getShadersPath() + "pbrtexture/shading.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
			shaderStages[1] = loadShader(getShadersPath() + "pbrtexture/shading.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
			pipelineCI.stageCount = 2;
			pipelineCI.pStages = shaderStages.data();
			VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &shadingPipeline));
			pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
		}
		//ASSERT(false, "debug interrupt");
		{
			// Hi-Z Buffer build pipeline
			VkPipelineShaderStageCreateInfo computeShaderStage = loadShader(getShadersPath() + "pbrtexture/genhiz.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);

			pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&descManager->getSetLayout("hizBuild"), 1);
			VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &hizComputePipelineLayout));

			VkComputePipelineCreateInfo pipelineCreateInfo = {};
			pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
			pipelineCreateInfo.stage = computeShaderStage;
			pipelineCreateInfo.layout = hizComputePipelineLayout;
			VK_CHECK_RESULT(vkCreateComputePipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &hizComputePipeline));
		}

		{
			// Clear image pipeline
			VkPipelineShaderStageCreateInfo computeShaderStage = loadShader(getShadersPath() + "pbrtexture/clearimage.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);

			pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&descManager->getSetLayout("clearImage"), 1);
			VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &clearImagePipelineLayout));

			VkComputePipelineCreateInfo pipelineCreateInfo = {};
			pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
			pipelineCreateInfo.stage = computeShaderStage;
			pipelineCreateInfo.layout = clearImagePipelineLayout;
			VK_CHECK_RESULT(vkCreateComputePipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &clearImagePipeline));
		}

		{
			// Software rasterize pipeline
			VkPipelineShaderStageCreateInfo computeShaderStage = loadShader(getShadersPath() + "pbrtexture/swrasterize.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);

			pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&descManager->getSetLayout("swRast"), 1);
			VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &swrComputePipelineLayout));

			VkComputePipelineCreateInfo pipelineCreateInfo = {};
			pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
			pipelineCreateInfo.stage = computeShaderStage;
			pipelineCreateInfo.layout = swrComputePipelineLayout;
			VK_CHECK_RESULT(vkCreateComputePipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &swrComputePipeline));
		}

		{
			// Depth copy pipeline
			VkPipelineShaderStageCreateInfo computeShaderStage = loadShader(getShadersPath() + "pbrtexture/depthcopy.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);

			pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&descManager->getSetLayout("depthCopy"), 1);
			VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &depthCopyPipelineLayout));

			VkComputePipelineCreateInfo pipelineCreateInfo = {};
			pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
			pipelineCreateInfo.stage = computeShaderStage;
			pipelineCreateInfo.layout = depthCopyPipelineLayout;
			VK_CHECK_RESULT(vkCreateComputePipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &depthCopyPipeline));
		}

		{
			// BVH Traversal pipeline
			VkPipelineShaderStageCreateInfo computeShaderStage = loadShader(getShadersPath() + "pbrtexture/bvhtraversal.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);
			VkPushConstantRange push_constant2{};
			push_constant2.size = sizeof(BVHTraversalPushConstants);
			push_constant2.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
			pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&descManager->getSetLayout("bvhTraversal"), 1);
			pipelineLayoutCreateInfo.pPushConstantRanges = &push_constant2;
			pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
			VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &bvhTraversalPipelineLayout));

			VkComputePipelineCreateInfo pipelineCreateInfo = {};
			pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
			pipelineCreateInfo.stage = computeShaderStage;
			pipelineCreateInfo.layout = bvhTraversalPipelineLayout;
			VK_CHECK_RESULT(vkCreateComputePipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &bvhTraversalPipeline));
		}

		{
			// Culling pipeline
			VkPipelineShaderStageCreateInfo computeShaderStage = loadShader(getShadersPath() + "pbrtexture/culling.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);
			VkPushConstantRange push_constant{};
			push_constant.size = sizeof(CullingPushConstants);
			push_constant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
			pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&descManager->getSetLayout("culling"), 1);
			pipelineLayoutCreateInfo.pPushConstantRanges = &push_constant;
			pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
			VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &cullingPipelineLayout));

			VkComputePipelineCreateInfo pipelineCreateInfo = {};
			pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
			pipelineCreateInfo.stage = computeShaderStage;
			pipelineCreateInfo.layout = cullingPipelineLayout;
			VK_CHECK_RESULT(vkCreateComputePipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &cullingPipeline));
		}

		{
			// Error projection pipeline
			VkPipelineShaderStageCreateInfo computeShaderStage = loadShader(getShadersPath() + "pbrtexture/error.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);
			VkPushConstantRange push_constant{};
			push_constant.size = sizeof(ErrorPushConstants);
			push_constant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
			pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&descManager->getSetLayout("errorProj"), 1);
			pipelineLayoutCreateInfo.pPushConstantRanges = &push_constant;
			pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
			VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &errorProjPipelineLayout));

			VkComputePipelineCreateInfo pipelineCreateInfo = {};
			pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
			pipelineCreateInfo.stage = computeShaderStage;
			pipelineCreateInfo.layout = errorProjPipelineLayout;
			VK_CHECK_RESULT(vkCreateComputePipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &errorProjPipeline));
		}

		{
			// Merge Rasterize result
			VkPipelineShaderStageCreateInfo computeShaderStage = loadShader(getShadersPath() + "pbrtexture/merger.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);
			VkPushConstantRange push_constant{};
			push_constant.size = sizeof(RenderingPushConstants);
			push_constant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
			pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&descManager->getSetLayout("mergeRast"), 1);
			pipelineLayoutCreateInfo.pPushConstantRanges = &push_constant;
			pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
			VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &mergeRastPipelineLayout));

			VkComputePipelineCreateInfo pipelineCreateInfo = {};
			pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
			pipelineCreateInfo.stage = computeShaderStage;
			pipelineCreateInfo.layout = mergeRastPipelineLayout;
			VK_CHECK_RESULT(vkCreateComputePipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &mergeRastPipeline));
		}
		//ASSERT(false, "debug");
	}

	// Generate a BRDF integration map used as a look-up-table (stores roughness / NdotV)
	void generateBRDFLUT()
	{
		auto tStart = std::chrono::high_resolution_clock::now();

		const VkFormat format = VK_FORMAT_R16G16_SFLOAT;	// R16G16 is supported pretty much everywhere
		const int32_t dim = 512;

		// Image
		VkImageCreateInfo imageCI = vks::initializers::imageCreateInfo();
		imageCI.imageType = VK_IMAGE_TYPE_2D;
		imageCI.format = format;
		imageCI.extent.width = dim;
		imageCI.extent.height = dim;
		imageCI.extent.depth = 1;
		imageCI.mipLevels = 1;
		imageCI.arrayLayers = 1;
		imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCI.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		VK_CHECK_RESULT(vkCreateImage(device, &imageCI, nullptr, &textures.lutBrdf.image));
		VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
		VkMemoryRequirements memReqs;
		vkGetImageMemoryRequirements(device, textures.lutBrdf.image, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		memAlloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &textures.lutBrdf.deviceMemory));
		VK_CHECK_RESULT(vkBindImageMemory(device, textures.lutBrdf.image, textures.lutBrdf.deviceMemory, 0));
		// Image view
		VkImageViewCreateInfo viewCI = vks::initializers::imageViewCreateInfo();
		viewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewCI.format = format;
		viewCI.subresourceRange = {};
		viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewCI.subresourceRange.levelCount = 1;
		viewCI.subresourceRange.layerCount = 1;
		viewCI.image = textures.lutBrdf.image;
		VK_CHECK_RESULT(vkCreateImageView(device, &viewCI, nullptr, &textures.lutBrdf.view));
		// Sampler
		VkSamplerCreateInfo samplerCI = vks::initializers::samplerCreateInfo();
		samplerCI.magFilter = VK_FILTER_LINEAR;
		samplerCI.minFilter = VK_FILTER_LINEAR;
		samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.minLod = 0.0f;
		samplerCI.maxLod = 1.0f;
		samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		VK_CHECK_RESULT(vkCreateSampler(device, &samplerCI, nullptr, &textures.lutBrdf.sampler));

		textures.lutBrdf.descriptor.imageView = textures.lutBrdf.view;
		textures.lutBrdf.descriptor.sampler = textures.lutBrdf.sampler;
		textures.lutBrdf.descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		textures.lutBrdf.device = vulkanDevice;

		// FB, Att, RP, Pipe, etc.
		VkAttachmentDescription attDesc = {};
		// Color attachment
		attDesc.format = format;
		attDesc.samples = VK_SAMPLE_COUNT_1_BIT;
		attDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attDesc.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

		VkSubpassDescription subpassDescription = {};
		subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDescription.colorAttachmentCount = 1;
		subpassDescription.pColorAttachments = &colorReference;

		// Use subpass dependencies for layout transitions
		std::array<VkSubpassDependency, 2> dependencies;
		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
		dependencies[1].srcSubpass = 0;
		dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		// Create the actual renderpass
		VkRenderPassCreateInfo renderPassCI = vks::initializers::renderPassCreateInfo();
		renderPassCI.attachmentCount = 1;
		renderPassCI.pAttachments = &attDesc;
		renderPassCI.subpassCount = 1;
		renderPassCI.pSubpasses = &subpassDescription;
		renderPassCI.dependencyCount = 2;
		renderPassCI.pDependencies = dependencies.data();

		VkRenderPass renderpass;
		VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassCI, nullptr, &renderpass));

		VkFramebufferCreateInfo framebufferCI = vks::initializers::framebufferCreateInfo();
		framebufferCI.renderPass = renderpass;
		framebufferCI.attachmentCount = 1;
		framebufferCI.pAttachments = &textures.lutBrdf.view;
		framebufferCI.width = dim;
		framebufferCI.height = dim;
		framebufferCI.layers = 1;

		VkFramebuffer framebuffer;
		VK_CHECK_RESULT(vkCreateFramebuffer(device, &framebufferCI, nullptr, &framebuffer));

		// Descriptors
		VkDescriptorSetLayout descriptorsetlayout;
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {};
		VkDescriptorSetLayoutCreateInfo descriptorsetlayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorsetlayoutCI, nullptr, &descriptorsetlayout));

		// Descriptor Pool
		std::vector<VkDescriptorPoolSize> poolSizes = { vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1) };
		VkDescriptorPoolCreateInfo descriptorPoolCI = vks::initializers::descriptorPoolCreateInfo(poolSizes, 2);
		VkDescriptorPool descriptorpool;
		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCI, nullptr, &descriptorpool));

		// Descriptor sets
		VkDescriptorSet descriptorset;
		VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorpool, &descriptorsetlayout, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorset));

		// Pipeline layout
		VkPipelineLayout pipelinelayout;
		VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(&descriptorsetlayout, 1);
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelinelayout));

		// Pipeline
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
		VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1);
		VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
		std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
		VkPipelineVertexInputStateCreateInfo emptyInputState = vks::initializers::pipelineVertexInputStateCreateInfo();
		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

		VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(pipelinelayout, renderpass);
		pipelineCI.pInputAssemblyState = &inputAssemblyState;
		pipelineCI.pRasterizationState = &rasterizationState;
		pipelineCI.pColorBlendState = &colorBlendState;
		pipelineCI.pMultisampleState = &multisampleState;
		pipelineCI.pViewportState = &viewportState;
		pipelineCI.pDepthStencilState = &depthStencilState;
		pipelineCI.pDynamicState = &dynamicState;
		pipelineCI.stageCount = 2;
		pipelineCI.pStages = shaderStages.data();
		pipelineCI.pVertexInputState = &emptyInputState;

		// Look-up-table (from BRDF) pipeline
		shaderStages[0] = loadShader(getShadersPath() + "pbrtexture/genbrdflut.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "pbrtexture/genbrdflut.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		VkPipeline pipeline;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipeline));

		// Render
		VkClearValue clearValues[1];
		clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };

		VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
		renderPassBeginInfo.renderPass = renderpass;
		renderPassBeginInfo.renderArea.extent.width = dim;
		renderPassBeginInfo.renderArea.extent.height = dim;
		renderPassBeginInfo.clearValueCount = 1;
		renderPassBeginInfo.pClearValues = clearValues;
		renderPassBeginInfo.framebuffer = framebuffer;

		VkCommandBuffer cmdBuf = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		vkCmdBeginRenderPass(cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		VkViewport viewport = vks::initializers::viewport((float)dim, (float)dim, 0.0f, 1.0f);
		VkRect2D scissor = vks::initializers::rect2D(dim, dim, 0, 0);
		vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
		vkCmdSetScissor(cmdBuf, 0, 1, &scissor);
		vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		vkCmdDraw(cmdBuf, 3, 1, 0, 0);
		vkCmdEndRenderPass(cmdBuf);
		vulkanDevice->flushCommandBuffer(cmdBuf, queue);

		vkQueueWaitIdle(queue);

		vkDestroyPipeline(device, pipeline, nullptr);
		vkDestroyPipelineLayout(device, pipelinelayout, nullptr);
		vkDestroyRenderPass(device, renderpass, nullptr);
		vkDestroyFramebuffer(device, framebuffer, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorsetlayout, nullptr);
		vkDestroyDescriptorPool(device, descriptorpool, nullptr);

		auto tEnd = std::chrono::high_resolution_clock::now();
		auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
		std::cout << "Generating BRDF LUT took " << tDiff << " ms" << std::endl;
	}

	// Generate an irradiance cube map from the environment cube map
	void generateIrradianceCube()
	{
		auto tStart = std::chrono::high_resolution_clock::now();

		const VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;
		const int32_t dim = 64;
		const uint32_t numMips = static_cast<uint32_t>(floor(log2(dim))) + 1;

		// Pre-filtered cube map
		// Image
		VkImageCreateInfo imageCI = vks::initializers::imageCreateInfo();
		imageCI.imageType = VK_IMAGE_TYPE_2D;
		imageCI.format = format;
		imageCI.extent.width = dim;
		imageCI.extent.height = dim;
		imageCI.extent.depth = 1;
		imageCI.mipLevels = numMips;
		imageCI.arrayLayers = 6;
		imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCI.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		imageCI.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		VK_CHECK_RESULT(vkCreateImage(device, &imageCI, nullptr, &textures.irradianceCube.image));
		VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
		VkMemoryRequirements memReqs;
		vkGetImageMemoryRequirements(device, textures.irradianceCube.image, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		memAlloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &textures.irradianceCube.deviceMemory));
		VK_CHECK_RESULT(vkBindImageMemory(device, textures.irradianceCube.image, textures.irradianceCube.deviceMemory, 0));
		// Image view
		VkImageViewCreateInfo viewCI = vks::initializers::imageViewCreateInfo();
		viewCI.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
		viewCI.format = format;
		viewCI.subresourceRange = {};
		viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewCI.subresourceRange.levelCount = numMips;
		viewCI.subresourceRange.layerCount = 6;
		viewCI.image = textures.irradianceCube.image;
		VK_CHECK_RESULT(vkCreateImageView(device, &viewCI, nullptr, &textures.irradianceCube.view));
		// Sampler
		VkSamplerCreateInfo samplerCI = vks::initializers::samplerCreateInfo();
		samplerCI.magFilter = VK_FILTER_LINEAR;
		samplerCI.minFilter = VK_FILTER_LINEAR;
		samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.minLod = 0.0f;
		samplerCI.maxLod = static_cast<float>(numMips);
		samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		VK_CHECK_RESULT(vkCreateSampler(device, &samplerCI, nullptr, &textures.irradianceCube.sampler));

		textures.irradianceCube.descriptor.imageView = textures.irradianceCube.view;
		textures.irradianceCube.descriptor.sampler = textures.irradianceCube.sampler;
		textures.irradianceCube.descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		textures.irradianceCube.device = vulkanDevice;

		// FB, Att, RP, Pipe, etc.
		VkAttachmentDescription attDesc = {};
		// Color attachment
		attDesc.format = format;
		attDesc.samples = VK_SAMPLE_COUNT_1_BIT;
		attDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

		VkSubpassDescription subpassDescription = {};
		subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDescription.colorAttachmentCount = 1;
		subpassDescription.pColorAttachments = &colorReference;

		// Use subpass dependencies for layout transitions
		std::array<VkSubpassDependency, 2> dependencies;
		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
		dependencies[1].srcSubpass = 0;
		dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		// Renderpass
		VkRenderPassCreateInfo renderPassCI = vks::initializers::renderPassCreateInfo();
		renderPassCI.attachmentCount = 1;
		renderPassCI.pAttachments = &attDesc;
		renderPassCI.subpassCount = 1;
		renderPassCI.pSubpasses = &subpassDescription;
		renderPassCI.dependencyCount = 2;
		renderPassCI.pDependencies = dependencies.data();
		VkRenderPass renderpass;
		VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassCI, nullptr, &renderpass));

		struct {
			VkImage image;
			VkImageView view;
			VkDeviceMemory memory;
			VkFramebuffer framebuffer;
		} offscreen;

		// Offfscreen framebuffer
		{
			// Color attachment
			VkImageCreateInfo imageCreateInfo = vks::initializers::imageCreateInfo();
			imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
			imageCreateInfo.format = format;
			imageCreateInfo.extent.width = dim;
			imageCreateInfo.extent.height = dim;
			imageCreateInfo.extent.depth = 1;
			imageCreateInfo.mipLevels = 1;
			imageCreateInfo.arrayLayers = 1;
			imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
			imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
			imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			imageCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
			imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			VK_CHECK_RESULT(vkCreateImage(device, &imageCreateInfo, nullptr, &offscreen.image));

			VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
			VkMemoryRequirements memReqs;
			vkGetImageMemoryRequirements(device, offscreen.image, &memReqs);
			memAlloc.allocationSize = memReqs.size;
			memAlloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &offscreen.memory));
			VK_CHECK_RESULT(vkBindImageMemory(device, offscreen.image, offscreen.memory, 0));

			VkImageViewCreateInfo colorImageView = vks::initializers::imageViewCreateInfo();
			colorImageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
			colorImageView.format = format;
			colorImageView.flags = 0;
			colorImageView.subresourceRange = {};
			colorImageView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			colorImageView.subresourceRange.baseMipLevel = 0;
			colorImageView.subresourceRange.levelCount = 1;
			colorImageView.subresourceRange.baseArrayLayer = 0;
			colorImageView.subresourceRange.layerCount = 1;
			colorImageView.image = offscreen.image;
			VK_CHECK_RESULT(vkCreateImageView(device, &colorImageView, nullptr, &offscreen.view));

			VkFramebufferCreateInfo fbufCreateInfo = vks::initializers::framebufferCreateInfo();
			fbufCreateInfo.renderPass = renderpass;
			fbufCreateInfo.attachmentCount = 1;
			fbufCreateInfo.pAttachments = &offscreen.view;
			fbufCreateInfo.width = dim;
			fbufCreateInfo.height = dim;
			fbufCreateInfo.layers = 1;
			VK_CHECK_RESULT(vkCreateFramebuffer(device, &fbufCreateInfo, nullptr, &offscreen.framebuffer));

			VkCommandBuffer layoutCmd = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
			vks::tools::setImageLayout(
				layoutCmd,
				offscreen.image,
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
			vulkanDevice->flushCommandBuffer(layoutCmd, queue, true);
		}

		// Descriptors
		VkDescriptorSetLayout descriptorsetlayout;
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
		};
		VkDescriptorSetLayoutCreateInfo descriptorsetlayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorsetlayoutCI, nullptr, &descriptorsetlayout));

		// Descriptor Pool
		std::vector<VkDescriptorPoolSize> poolSizes = { vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1) };
		VkDescriptorPoolCreateInfo descriptorPoolCI = vks::initializers::descriptorPoolCreateInfo(poolSizes, 2);
		VkDescriptorPool descriptorpool;
		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCI, nullptr, &descriptorpool));

		// Descriptor sets
		VkDescriptorSet descriptorset;
		VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorpool, &descriptorsetlayout, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorset));
		VkWriteDescriptorSet writeDescriptorSet = vks::initializers::writeDescriptorSet(descriptorset, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &textures.environmentCube.descriptor);
		vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);

		// Pipeline layout
		struct PushBlock {
			glm::mat4 mvp;
			// Sampling deltas
			float deltaPhi = (2.0f * float(M_PI)) / 180.0f;
			float deltaTheta = (0.5f * float(M_PI)) / 64.0f;
		} pushBlock;

		VkPipelineLayout pipelinelayout;
		std::vector<VkPushConstantRange> pushConstantRanges = {
			vks::initializers::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(PushBlock), 0),
		};
		VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(&descriptorsetlayout, 1);
		pipelineLayoutCI.pushConstantRangeCount = 1;
		pipelineLayoutCI.pPushConstantRanges = pushConstantRanges.data();
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelinelayout));

		// Pipeline
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
		VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1);
		VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
		std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

		VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(pipelinelayout, renderpass);
		pipelineCI.pInputAssemblyState = &inputAssemblyState;
		pipelineCI.pRasterizationState = &rasterizationState;
		pipelineCI.pColorBlendState = &colorBlendState;
		pipelineCI.pMultisampleState = &multisampleState;
		pipelineCI.pViewportState = &viewportState;
		pipelineCI.pDepthStencilState = &depthStencilState;
		pipelineCI.pDynamicState = &dynamicState;
		pipelineCI.stageCount = 2;
		pipelineCI.pStages = shaderStages.data();
		pipelineCI.renderPass = renderpass;
		pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::UV });

		shaderStages[0] = loadShader(getShadersPath() + "pbrtexture/filtercube.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "pbrtexture/irradiancecube.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		VkPipeline pipeline;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipeline));

		// Render

		VkClearValue clearValues[1];
		clearValues[0].color = { { 0.0f, 0.0f, 0.2f, 0.0f } };

		VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
		// Reuse render pass from example pass
		renderPassBeginInfo.renderPass = renderpass;
		renderPassBeginInfo.framebuffer = offscreen.framebuffer;
		renderPassBeginInfo.renderArea.extent.width = dim;
		renderPassBeginInfo.renderArea.extent.height = dim;
		renderPassBeginInfo.clearValueCount = 1;
		renderPassBeginInfo.pClearValues = clearValues;

		std::vector<glm::mat4> matrices = {
			// POSITIVE_X
			glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// NEGATIVE_X
			glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// POSITIVE_Y
			glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// NEGATIVE_Y
			glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// POSITIVE_Z
			glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// NEGATIVE_Z
			glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
		};

		VkCommandBuffer cmdBuf = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		VkViewport viewport = vks::initializers::viewport((float)dim, (float)dim, 0.0f, 1.0f);
		VkRect2D scissor = vks::initializers::rect2D(dim, dim, 0, 0);

		vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
		vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = numMips;
		subresourceRange.layerCount = 6;

		// Change image layout for all cubemap faces to transfer destination
		vks::tools::setImageLayout(
			cmdBuf,
			textures.irradianceCube.image,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			subresourceRange);

		for (uint32_t m = 0; m < numMips; m++) {
			for (uint32_t f = 0; f < 6; f++) {
				viewport.width = static_cast<float>(dim * std::pow(0.5f, m));
				viewport.height = static_cast<float>(dim * std::pow(0.5f, m));
				vkCmdSetViewport(cmdBuf, 0, 1, &viewport);

				// Render scene from cube face's point of view
				vkCmdBeginRenderPass(cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

				// Update shader push constant block
				pushBlock.mvp = glm::perspective((float)(M_PI / 2.0), 1.0f, 0.1f, 512.0f) * matrices[f];

				vkCmdPushConstants(cmdBuf, pipelinelayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushBlock), &pushBlock);

				vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
				vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelinelayout, 0, 1, &descriptorset, 0, NULL);

				models.skybox.draw(cmdBuf);

				vkCmdEndRenderPass(cmdBuf);

				vks::tools::setImageLayout(
					cmdBuf,
					offscreen.image,
					VK_IMAGE_ASPECT_COLOR_BIT,
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

				// Copy region for transfer from framebuffer to cube face
				VkImageCopy copyRegion = {};

				copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				copyRegion.srcSubresource.baseArrayLayer = 0;
				copyRegion.srcSubresource.mipLevel = 0;
				copyRegion.srcSubresource.layerCount = 1;
				copyRegion.srcOffset = { 0, 0, 0 };

				copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				copyRegion.dstSubresource.baseArrayLayer = f;
				copyRegion.dstSubresource.mipLevel = m;
				copyRegion.dstSubresource.layerCount = 1;
				copyRegion.dstOffset = { 0, 0, 0 };

				copyRegion.extent.width = static_cast<uint32_t>(viewport.width);
				copyRegion.extent.height = static_cast<uint32_t>(viewport.height);
				copyRegion.extent.depth = 1;

				vkCmdCopyImage(
					cmdBuf,
					offscreen.image,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					textures.irradianceCube.image,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					1,
					&copyRegion);

				// Transform framebuffer color attachment back
				vks::tools::setImageLayout(
					cmdBuf,
					offscreen.image,
					VK_IMAGE_ASPECT_COLOR_BIT,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
			}
		}

		vks::tools::setImageLayout(
			cmdBuf,
			textures.irradianceCube.image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			subresourceRange);

		vulkanDevice->flushCommandBuffer(cmdBuf, queue);

		vkDestroyRenderPass(device, renderpass, nullptr);
		vkDestroyFramebuffer(device, offscreen.framebuffer, nullptr);
		vkFreeMemory(device, offscreen.memory, nullptr);
		vkDestroyImageView(device, offscreen.view, nullptr);
		vkDestroyImage(device, offscreen.image, nullptr);
		vkDestroyDescriptorPool(device, descriptorpool, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorsetlayout, nullptr);
		vkDestroyPipeline(device, pipeline, nullptr);
		vkDestroyPipelineLayout(device, pipelinelayout, nullptr);

		auto tEnd = std::chrono::high_resolution_clock::now();
		auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
		std::cout << "Generating irradiance cube with " << numMips << " mip levels took " << tDiff << " ms" << std::endl;
	}

	// Prefilter environment cubemap
	// See https://placeholderart.wordpress.com/2015/07/28/implementation-notes-runtime-environment-map-filtering-for-image-based-lighting/
	void generatePrefilteredCube()
	{
		auto tStart = std::chrono::high_resolution_clock::now();

		const VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT;
		const int32_t dim = 512;
		const uint32_t numMips = static_cast<uint32_t>(floor(log2(dim))) + 1;

		// Pre-filtered cube map
		// Image
		VkImageCreateInfo imageCI = vks::initializers::imageCreateInfo();
		imageCI.imageType = VK_IMAGE_TYPE_2D;
		imageCI.format = format;
		imageCI.extent.width = dim;
		imageCI.extent.height = dim;
		imageCI.extent.depth = 1;
		imageCI.mipLevels = numMips;
		imageCI.arrayLayers = 6;
		imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCI.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		imageCI.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		VK_CHECK_RESULT(vkCreateImage(device, &imageCI, nullptr, &textures.prefilteredCube.image));
		VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
		VkMemoryRequirements memReqs;
		vkGetImageMemoryRequirements(device, textures.prefilteredCube.image, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		memAlloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &textures.prefilteredCube.deviceMemory));
		VK_CHECK_RESULT(vkBindImageMemory(device, textures.prefilteredCube.image, textures.prefilteredCube.deviceMemory, 0));
		// Image view
		VkImageViewCreateInfo viewCI = vks::initializers::imageViewCreateInfo();
		viewCI.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
		viewCI.format = format;
		viewCI.subresourceRange = {};
		viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewCI.subresourceRange.levelCount = numMips;
		viewCI.subresourceRange.layerCount = 6;
		viewCI.image = textures.prefilteredCube.image;
		VK_CHECK_RESULT(vkCreateImageView(device, &viewCI, nullptr, &textures.prefilteredCube.view));
		// Sampler
		VkSamplerCreateInfo samplerCI = vks::initializers::samplerCreateInfo();
		samplerCI.magFilter = VK_FILTER_LINEAR;
		samplerCI.minFilter = VK_FILTER_LINEAR;
		samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.minLod = 0.0f;
		samplerCI.maxLod = static_cast<float>(numMips);
		samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		VK_CHECK_RESULT(vkCreateSampler(device, &samplerCI, nullptr, &textures.prefilteredCube.sampler));

		textures.prefilteredCube.descriptor.imageView = textures.prefilteredCube.view;
		textures.prefilteredCube.descriptor.sampler = textures.prefilteredCube.sampler;
		textures.prefilteredCube.descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		textures.prefilteredCube.device = vulkanDevice;

		// FB, Att, RP, Pipe, etc.
		VkAttachmentDescription attDesc = {};
		// Color attachment
		attDesc.format = format;
		attDesc.samples = VK_SAMPLE_COUNT_1_BIT;
		attDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

		VkSubpassDescription subpassDescription = {};
		subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDescription.colorAttachmentCount = 1;
		subpassDescription.pColorAttachments = &colorReference;

		// Use subpass dependencies for layout transitions
		std::array<VkSubpassDependency, 2> dependencies;
		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
		dependencies[1].srcSubpass = 0;
		dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		// Renderpass
		VkRenderPassCreateInfo renderPassCI = vks::initializers::renderPassCreateInfo();
		renderPassCI.attachmentCount = 1;
		renderPassCI.pAttachments = &attDesc;
		renderPassCI.subpassCount = 1;
		renderPassCI.pSubpasses = &subpassDescription;
		renderPassCI.dependencyCount = 2;
		renderPassCI.pDependencies = dependencies.data();
		VkRenderPass renderpass;
		VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassCI, nullptr, &renderpass));

		struct {
			VkImage image;
			VkImageView view;
			VkDeviceMemory memory;
			VkFramebuffer framebuffer;
		} offscreen;

		// Offfscreen framebuffer
		{
			// Color attachment
			VkImageCreateInfo imageCreateInfo = vks::initializers::imageCreateInfo();
			imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
			imageCreateInfo.format = format;
			imageCreateInfo.extent.width = dim;
			imageCreateInfo.extent.height = dim;
			imageCreateInfo.extent.depth = 1;
			imageCreateInfo.mipLevels = 1;
			imageCreateInfo.arrayLayers = 1;
			imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
			imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
			imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			imageCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
			imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			VK_CHECK_RESULT(vkCreateImage(device, &imageCreateInfo, nullptr, &offscreen.image));

			VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
			VkMemoryRequirements memReqs;
			vkGetImageMemoryRequirements(device, offscreen.image, &memReqs);
			memAlloc.allocationSize = memReqs.size;
			memAlloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &offscreen.memory));
			VK_CHECK_RESULT(vkBindImageMemory(device, offscreen.image, offscreen.memory, 0));

			VkImageViewCreateInfo colorImageView = vks::initializers::imageViewCreateInfo();
			colorImageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
			colorImageView.format = format;
			colorImageView.flags = 0;
			colorImageView.subresourceRange = {};
			colorImageView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			colorImageView.subresourceRange.baseMipLevel = 0;
			colorImageView.subresourceRange.levelCount = 1;
			colorImageView.subresourceRange.baseArrayLayer = 0;
			colorImageView.subresourceRange.layerCount = 1;
			colorImageView.image = offscreen.image;
			VK_CHECK_RESULT(vkCreateImageView(device, &colorImageView, nullptr, &offscreen.view));

			VkFramebufferCreateInfo fbufCreateInfo = vks::initializers::framebufferCreateInfo();
			fbufCreateInfo.renderPass = renderpass;
			fbufCreateInfo.attachmentCount = 1;
			fbufCreateInfo.pAttachments = &offscreen.view;
			fbufCreateInfo.width = dim;
			fbufCreateInfo.height = dim;
			fbufCreateInfo.layers = 1;
			VK_CHECK_RESULT(vkCreateFramebuffer(device, &fbufCreateInfo, nullptr, &offscreen.framebuffer));

			VkCommandBuffer layoutCmd = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
			vks::tools::setImageLayout(
				layoutCmd,
				offscreen.image,
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
			vulkanDevice->flushCommandBuffer(layoutCmd, queue, true);
		}

		// Descriptors
		VkDescriptorSetLayout descriptorsetlayout;
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
		};
		VkDescriptorSetLayoutCreateInfo descriptorsetlayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorsetlayoutCI, nullptr, &descriptorsetlayout));

		// Descriptor Pool
		std::vector<VkDescriptorPoolSize> poolSizes = { vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1) };
		VkDescriptorPoolCreateInfo descriptorPoolCI = vks::initializers::descriptorPoolCreateInfo(poolSizes, 2);
		VkDescriptorPool descriptorpool;
		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCI, nullptr, &descriptorpool));

		// Descriptor sets
		VkDescriptorSet descriptorset;
		VkDescriptorSetAllocateInfo allocInfo =	vks::initializers::descriptorSetAllocateInfo(descriptorpool, &descriptorsetlayout, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorset));
		VkWriteDescriptorSet writeDescriptorSet = vks::initializers::writeDescriptorSet(descriptorset, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &textures.environmentCube.descriptor);
		vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);

		// Pipeline layout
		struct PushBlock {
			glm::mat4 mvp;
			float roughness;
			uint32_t numSamples = 32u;
		} pushBlock;

		VkPipelineLayout pipelinelayout;
		std::vector<VkPushConstantRange> pushConstantRanges = {
			vks::initializers::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(PushBlock), 0),
		};
		VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(&descriptorsetlayout, 1);
		pipelineLayoutCI.pushConstantRangeCount = 1;
		pipelineLayoutCI.pPushConstantRanges = pushConstantRanges.data();
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelinelayout));

		// Pipeline
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
		VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1);
		VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
		std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

		VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(pipelinelayout, renderpass);
		pipelineCI.pInputAssemblyState = &inputAssemblyState;
		pipelineCI.pRasterizationState = &rasterizationState;
		pipelineCI.pColorBlendState = &colorBlendState;
		pipelineCI.pMultisampleState = &multisampleState;
		pipelineCI.pViewportState = &viewportState;
		pipelineCI.pDepthStencilState = &depthStencilState;
		pipelineCI.pDynamicState = &dynamicState;
		pipelineCI.stageCount = 2;
		pipelineCI.pStages = shaderStages.data();
		pipelineCI.renderPass = renderpass;
		pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::UV });

		shaderStages[0] = loadShader(getShadersPath() + "pbrtexture/filtercube.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "pbrtexture/prefilterenvmap.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		VkPipeline pipeline;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipeline));

		// Render

		VkClearValue clearValues[1];
		clearValues[0].color = { { 0.0f, 0.0f, 0.2f, 0.0f } };

		VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
		// Reuse render pass from example pass
		renderPassBeginInfo.renderPass = renderpass;
		renderPassBeginInfo.framebuffer = offscreen.framebuffer;
		renderPassBeginInfo.renderArea.extent.width = dim;
		renderPassBeginInfo.renderArea.extent.height = dim;
		renderPassBeginInfo.clearValueCount = 1;
		renderPassBeginInfo.pClearValues = clearValues;

		std::vector<glm::mat4> matrices = {
			// POSITIVE_X
			glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// NEGATIVE_X
			glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// POSITIVE_Y
			glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// NEGATIVE_Y
			glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// POSITIVE_Z
			glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// NEGATIVE_Z
			glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
		};

		VkCommandBuffer cmdBuf = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		VkViewport viewport = vks::initializers::viewport((float)dim, (float)dim, 0.0f, 1.0f);
		VkRect2D scissor = vks::initializers::rect2D(dim, dim, 0, 0);

		vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
		vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = numMips;
		subresourceRange.layerCount = 6;

		// Change image layout for all cubemap faces to transfer destination
		vks::tools::setImageLayout(
			cmdBuf,
			textures.prefilteredCube.image,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			subresourceRange);

		for (uint32_t m = 0; m < numMips; m++) {
			pushBlock.roughness = (float)m / (float)(numMips - 1);
			for (uint32_t f = 0; f < 6; f++) {
				viewport.width = static_cast<float>(dim * std::pow(0.5f, m));
				viewport.height = static_cast<float>(dim * std::pow(0.5f, m));
				vkCmdSetViewport(cmdBuf, 0, 1, &viewport);

				// Render scene from cube face's point of view
				vkCmdBeginRenderPass(cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

				// Update shader push constant block
				pushBlock.mvp = glm::perspective((float)(M_PI / 2.0), 1.0f, 0.1f, 512.0f) * matrices[f];

				vkCmdPushConstants(cmdBuf, pipelinelayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushBlock), &pushBlock);

				vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
				vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelinelayout, 0, 1, &descriptorset, 0, NULL);

				models.skybox.draw(cmdBuf);

				vkCmdEndRenderPass(cmdBuf);

				vks::tools::setImageLayout(
					cmdBuf,
					offscreen.image,
					VK_IMAGE_ASPECT_COLOR_BIT,
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

				// Copy region for transfer from framebuffer to cube face
				VkImageCopy copyRegion = {};

				copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				copyRegion.srcSubresource.baseArrayLayer = 0;
				copyRegion.srcSubresource.mipLevel = 0;
				copyRegion.srcSubresource.layerCount = 1;
				copyRegion.srcOffset = { 0, 0, 0 };

				copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				copyRegion.dstSubresource.baseArrayLayer = f;
				copyRegion.dstSubresource.mipLevel = m;
				copyRegion.dstSubresource.layerCount = 1;
				copyRegion.dstOffset = { 0, 0, 0 };

				copyRegion.extent.width = static_cast<uint32_t>(viewport.width);
				copyRegion.extent.height = static_cast<uint32_t>(viewport.height);
				copyRegion.extent.depth = 1;

				vkCmdCopyImage(
					cmdBuf,
					offscreen.image,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					textures.prefilteredCube.image,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					1,
					&copyRegion);

				// Transform framebuffer color attachment back
				vks::tools::setImageLayout(
					cmdBuf,
					offscreen.image,
					VK_IMAGE_ASPECT_COLOR_BIT,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
			}
		}

		vks::tools::setImageLayout(
			cmdBuf,
			textures.prefilteredCube.image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			subresourceRange);

		vulkanDevice->flushCommandBuffer(cmdBuf, queue);

		vkDestroyRenderPass(device, renderpass, nullptr);
		vkDestroyFramebuffer(device, offscreen.framebuffer, nullptr);
		vkFreeMemory(device, offscreen.memory, nullptr);
		vkDestroyImageView(device, offscreen.view, nullptr);
		vkDestroyImage(device, offscreen.image, nullptr);
		vkDestroyDescriptorPool(device, descriptorpool, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorsetlayout, nullptr);
		vkDestroyPipeline(device, pipeline, nullptr);
		vkDestroyPipelineLayout(device, pipelinelayout, nullptr);

		auto tEnd = std::chrono::high_resolution_clock::now();
		auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
		std::cout << "Generating pre-filtered enivornment cube with " << numMips << " mip levels took " << tDiff << " ms" << std::endl;
	}

	void createBVHTraversalBuffers() {
		//vks::Buffer bvhNodeInfosBuffer;
		//vks::Buffer currNodeInfosBuffer;
		//vks::Buffer nextNodeInfosBuffer;
		//vks::Buffer culledClusterIndicesBuffer; // Cluster indices after BVH culling

		{
			vks::Buffer bvhNodeInfosStaging;
			//std::cout << "size of clusterInfo:" << sizeof(ClusterInfo) << std::endl;

			VK_CHECK_RESULT(vulkanDevice->createBuffer(
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				scene.bvhNodeInfos.size() * sizeof(BVHNodeInfo),
				&bvhNodeInfosStaging.buffer,
				&bvhNodeInfosStaging.memory,
				scene.bvhNodeInfos.data()));

			VK_CHECK_RESULT(vulkanDevice->createBuffer(
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				scene.bvhNodeInfos.size() * sizeof(BVHNodeInfo),
				&bvhNodeInfosBuffer.buffer,
				&bvhNodeInfosBuffer.memory,
				nullptr));
			VkCommandBuffer copyCmd = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

			VkBufferCopy copyRegion = {};

			copyRegion.size = scene.bvhNodeInfos.size() * sizeof(BVHNodeInfo);
			vkCmdCopyBuffer(copyCmd, bvhNodeInfosStaging.buffer, bvhNodeInfosBuffer.buffer, 1, &copyRegion);

			vulkanDevice->flushCommandBuffer(copyCmd, queue, true);//TODO: get transfer queue here

			vkDestroyBuffer(vulkanDevice->logicalDevice, bvhNodeInfosStaging.buffer, nullptr);
			vkFreeMemory(vulkanDevice->logicalDevice, bvhNodeInfosStaging.memory, nullptr);

			//std::cout << bvhNodeInfosBuffer.alignment << std::endl;
			//std::cout << bvhNodeInfosBuffer.size << std::endl;
			//std::cout << sizeof(BVHNodeInfo) << std::endl;
			//std::cout << alignof(BVHNodeInfo) << std::endl;
			//ASSERT(0, "Stop");
		}

		{

			vks::Buffer initNodeInfosStaging;
			//std::cout << "size of clusterInfo:" << sizeof(ClusterInfo) << std::endl;

			VK_CHECK_RESULT(vulkanDevice->createBuffer(
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				scene.initNodeInfoIndices.size() * sizeof(uint32_t),
				&initNodeInfosStaging.buffer,
				&initNodeInfosStaging.memory,
				scene.initNodeInfoIndices.data()));

			VK_CHECK_RESULT(vulkanDevice->createBuffer(
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				scene.initNodeInfoIndices.size() * sizeof(uint32_t),
				&initNodeInfosBuffer.buffer,
				&initNodeInfosBuffer.memory,
				nullptr));
			VkCommandBuffer copyCmd = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

			VkBufferCopy copyRegion = {};

			copyRegion.size = scene.initNodeInfoIndices.size() * sizeof(uint32_t);
			vkCmdCopyBuffer(copyCmd, initNodeInfosStaging.buffer, initNodeInfosBuffer.buffer, 1, &copyRegion);

			vulkanDevice->flushCommandBuffer(copyCmd, queue, true);//TODO: get transfer queue here

			vkDestroyBuffer(vulkanDevice->logicalDevice, initNodeInfosStaging.buffer, nullptr);
			vkFreeMemory(vulkanDevice->logicalDevice, initNodeInfosStaging.memory, nullptr);
		}

		std::cout << "scene.maxDepthCounts: " << scene.maxDepthCounts << std::endl;
		std::cout << scene.maxDepthCounts * sizeof(uint32_t) << std::endl;
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			scene.maxDepthCounts * 2 * sizeof(uint32_t),
			//10000 * sizeof(uint32_t),
			&currNodeInfosBuffer.buffer,
			&currNodeInfosBuffer.memory,
			nullptr));

		{

			vks::Buffer sortedClusterIndicesStaging;
			//std::cout << "size of clusterInfo:" << sizeof(ClusterInfo) << std::endl;

			VK_CHECK_RESULT(vulkanDevice->createBuffer(
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				scene.sortedClusterIndices.size() * sizeof(uint32_t),
				&sortedClusterIndicesStaging.buffer,
				&sortedClusterIndicesStaging.memory,
				scene.sortedClusterIndices.data()));

			VK_CHECK_RESULT(vulkanDevice->createBuffer(
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				scene.sortedClusterIndices.size() * sizeof(uint32_t),
				&sortedClusterIndicesBuffer.buffer,
				&sortedClusterIndicesBuffer.memory,
				nullptr));
			VkCommandBuffer copyCmd = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

			VkBufferCopy copyRegion = {};

			copyRegion.size = scene.sortedClusterIndices.size() * sizeof(uint32_t);
			vkCmdCopyBuffer(copyCmd, sortedClusterIndicesStaging.buffer, sortedClusterIndicesBuffer.buffer, 1, &copyRegion);

			vulkanDevice->flushCommandBuffer(copyCmd, queue, true);//TODO: get transfer queue here

			vkDestroyBuffer(vulkanDevice->logicalDevice, sortedClusterIndicesStaging.buffer, nullptr);
			vkFreeMemory(vulkanDevice->logicalDevice, sortedClusterIndicesStaging.memory, nullptr);
		}

		//VkMemoryRequirements memoryRequirements;
		//vkGetBufferMemoryRequirements(device, currNodeInfosBuffer.buffer, &memoryRequirements);
		//std::cout << memoryRequirements.alignment << std::endl;
		//std::cout << memoryRequirements.size << std::endl;

		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			scene.maxDepthCounts * 2 * sizeof(uint32_t),
			//10000 * sizeof(uint32_t),
			&nextNodeInfosBuffer.buffer,
			&nextNodeInfosBuffer.memory,
			nullptr));

		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			(scene.maxClusterNum + 5) * sizeof(uint32_t), // reserve 5 for atomic counter
			&culledClusterIndicesBuffer.buffer,
			&culledClusterIndicesBuffer.memory,
			nullptr));
		//VK_CHECK_RESULT(culledClusterIndicesBuffer.map(sizeof(uint32_t)));

		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			scene.maxClusterNum * sizeof(uint32_t),
			//scene.sceneIndicesCount * sizeof(uint16_t),
			//width * height * CLUSTER_MAX_SIZE * sizeof(uint32_t), // TODO: Should consider using a buffer relative with screen size
			&culledClusterObjectIndicesBuffer.buffer,
			&culledClusterObjectIndicesBuffer.memory,
			nullptr));
	}

	void createCullingBuffers()
	{
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			scene.sceneIndicesCount / 8 * sizeof(uint32_t),
			&HWRIndicesBuffer.buffer,
			&HWRIndicesBuffer.memory,
			nullptr));

		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			scene.sceneIndicesCount / 8 / 3 * sizeof(glm::uvec3),
			&HWRIDBuffer.buffer,
			&HWRIDBuffer.memory,
			nullptr));
		//ASSERT(false, "buffer size:"+std::to_string(scene.sceneIndicesCount / 8));

		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			scene.sceneIndicesCount / 8 * sizeof(uint32_t),
			&SWRIndicesBuffer.buffer,
			&SWRIndicesBuffer.memory,
			nullptr));

		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			scene.sceneIndicesCount / 8 / 3 * sizeof(glm::uvec3),
			&SWRIDBuffer.buffer,
			&SWRIDBuffer.memory,
			nullptr));
		
		for (auto& ci:scene.clusterInfo)
		{
			assert(ci.triangleIndicesEnd >= 0 && ci.triangleIndicesEnd <= scene.indices.count / 3);
			assert(ci.triangleIndicesStart >= 0 && ci.triangleIndicesStart < scene.indices.count / 3);
			assert(ci.triangleIndicesStart < ci.triangleIndicesEnd);
			clusterinfos.emplace_back(ci);
		}

		vks::Buffer clusterStaging;
		//std::cout << "size of clusterInfo:" << sizeof(ClusterInfo) << std::endl;

		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			clusterinfos.size() * sizeof(ClusterInfo),
			&clusterStaging.buffer,
			&clusterStaging.memory,
			clusterinfos.data()));

		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			clusterinfos.size() * sizeof(ClusterInfo),
			&clustersInfoBuffer.buffer,
			&clustersInfoBuffer.memory,
			nullptr));
		VkCommandBuffer copyCmd = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		VkBufferCopy copyRegion = {};

		copyRegion.size = clusterinfos.size() * sizeof(ClusterInfo);
		vkCmdCopyBuffer(copyCmd, clusterStaging.buffer, clustersInfoBuffer.buffer, 1, &copyRegion);

		vulkanDevice->flushCommandBuffer(copyCmd, queue, true);//TODO: get transfer queue here

		vkDestroyBuffer(vulkanDevice->logicalDevice, clusterStaging.buffer, nullptr);
		vkFreeMemory(vulkanDevice->logicalDevice, clusterStaging.memory, nullptr);


		uboCullingMatrices.model = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		uboCullingMatrices.lastView = camera.matrices.view;
		uboCullingMatrices.lastProj = camera.matrices.perspective;
		uboCullingMatrices.currView = camera.matrices.view;
		uboCullingMatrices.currProj = camera.matrices.perspective;

		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			sizeof(UBOCullingMatrices),
			&cullingUniformBuffer.buffer,
			&cullingUniformBuffer.memory,
			&uboCullingMatrices));
		cullingUniformBuffer.device = device;
		VK_CHECK_RESULT(cullingUniformBuffer.map());

		hwrDrawIndexedIndirect.firstIndex = 0;
		hwrDrawIndexedIndirect.firstInstance = 0;
		hwrDrawIndexedIndirect.indexCount = models.object.indexBuffer.size();
		//drawIndexedIndirect.indexCount = naniteMesh.meshes[naniteMesh.meshes.size()-1].mesh.n_faces();
		hwrDrawIndexedIndirect.instanceCount = 1;
		hwrDrawIndexedIndirect.vertexOffset = 0;

		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			sizeof(DrawIndexedIndirect),
			&hwrDrawIndexedIndirectBuffer.buffer,
			&hwrDrawIndexedIndirectBuffer.memory,
			&hwrDrawIndexedIndirect));

		hwrDrawIndexedIndirectBuffer.device = device;
		VK_CHECK_RESULT(hwrDrawIndexedIndirectBuffer.map());


		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			sizeof(SWRIndirectBuffer),
			&swrIndirectDispatchBuffer.buffer,
			&swrIndirectDispatchBuffer.memory,
			nullptr));


		uint32_t num_verts = 0;
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			sizeof(uint32_t),
			&swrNumVerticesBuffer.buffer,
			&swrNumVerticesBuffer.memory,
			&num_verts));

		swrNumVerticesBuffer.device = device;
		VK_CHECK_RESULT(swrNumVerticesBuffer.map());

		//ASSERT(false, "debug");
	}

	void createErrorProjectionBuffer()
	{
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			//scene.errorInfo.size() * sizeof(glm::vec2),
			scene.maxClusterNum * sizeof(glm::vec2),
			&projectedErrorBuffer.buffer,
			&projectedErrorBuffer.memory,
			nullptr));

		for (auto& ei : scene.errorInfo)
		{
			errorinfos.emplace_back(ei);
		}
		vks::Buffer errorStaging;
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			errorinfos.size() * sizeof(ErrorInfo),
			&errorStaging.buffer,
			&errorStaging.memory,
			errorinfos.data()));

		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			errorinfos.size() * sizeof(ErrorInfo),
			&errorInfoBuffer.buffer,
			&errorInfoBuffer.memory,
			nullptr));
		VkCommandBuffer copyCmd = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		VkBufferCopy copyRegion = {};

		copyRegion.size = errorinfos.size() * sizeof(ErrorInfo);
		vkCmdCopyBuffer(copyCmd, errorStaging.buffer, errorInfoBuffer.buffer, 1, &copyRegion);

		vulkanDevice->flushCommandBuffer(copyCmd, queue, true);//TODO: get transfer queue here

		vkDestroyBuffer(vulkanDevice->logicalDevice, errorStaging.buffer, nullptr);
		vkFreeMemory(vulkanDevice->logicalDevice, errorStaging.memory, nullptr);

		uboErrorMatrices.view = camera.matrices.view;
		uboErrorMatrices.proj = camera.matrices.perspective;
		uboErrorMatrices.camRight = camera.getRight();
		uboErrorMatrices.camUp = camera.getUp();
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			sizeof(UBOErrorMatrices),
			&errorUniformBuffer.buffer,
			&errorUniformBuffer.memory,
			&uboErrorMatrices));
		errorUniformBuffer.device = device;
		VK_CHECK_RESULT(errorUniformBuffer.map());
	}

	void createRasterizeBuffer()
	{
		VkImageCreateInfo imageCI{};
		imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCI.imageType = VK_IMAGE_TYPE_2D;
		imageCI.format = VK_FORMAT_D32_SFLOAT;
		imageCI.extent = { width, height, 1 };
		imageCI.mipLevels = 1;
		imageCI.arrayLayers = 1;
		imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCI.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

		VK_CHECK_RESULT(vkCreateImage(device, &imageCI, nullptr, &HWRZBuffer.image));
		VkMemoryRequirements memReqs{};
		vkGetImageMemoryRequirements(device, HWRZBuffer.image, &memReqs);

		VkMemoryAllocateInfo memAllloc{};
		memAllloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		memAllloc.allocationSize = memReqs.size;
		memAllloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device, &memAllloc, nullptr, &HWRZBuffer.mem));
		VK_CHECK_RESULT(vkBindImageMemory(device, HWRZBuffer.image, HWRZBuffer.mem, 0));

		VkImageViewCreateInfo imageViewCI{};
		imageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCI.image = HWRZBuffer.image;
		imageViewCI.format = VK_FORMAT_D32_SFLOAT;
		imageViewCI.subresourceRange.baseMipLevel = 0;
		imageViewCI.subresourceRange.levelCount = 1;
		imageViewCI.subresourceRange.baseArrayLayer = 0;
		imageViewCI.subresourceRange.layerCount = 1;
		imageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		VK_CHECK_RESULT(vkCreateImageView(device, &imageViewCI, nullptr, &HWRZBuffer.view));

		VkSamplerCreateInfo samplerCI = vks::initializers::samplerCreateInfo();
		samplerCI.magFilter = VK_FILTER_NEAREST;
		samplerCI.minFilter = VK_FILTER_NEAREST;
		samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		VK_CHECK_RESULT(vkCreateSampler(device, &samplerCI, nullptr, &HWRZBuffer.sampler));

		VkCommandBuffer cmdBuf = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		subresourceRange.levelCount = 1;
		subresourceRange.layerCount = 1;

		vks::tools::setImageLayout(
			cmdBuf,
			HWRZBuffer.image,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			subresourceRange);

		vulkanDevice->flushCommandBuffer(cmdBuf, queue);





		imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCI.imageType = VK_IMAGE_TYPE_2D;
		imageCI.format = VK_FORMAT_R32_UINT;
		imageCI.extent = { width, height, 1 };
		imageCI.mipLevels = 1;
		imageCI.arrayLayers = 1;
		imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCI.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		VK_CHECK_RESULT(vkCreateImage(device, &imageCI, nullptr, &HWRVisBuffer.image));
		vkGetImageMemoryRequirements(device, HWRVisBuffer.image, &memReqs);

		memAllloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		memAllloc.allocationSize = memReqs.size;
		memAllloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device, &memAllloc, nullptr, &HWRVisBuffer.mem));
		VK_CHECK_RESULT(vkBindImageMemory(device, HWRVisBuffer.image, HWRVisBuffer.mem, 0));

		imageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCI.image = HWRVisBuffer.image;
		imageViewCI.format = VK_FORMAT_R32_UINT;
		imageViewCI.subresourceRange.baseMipLevel = 0;
		imageViewCI.subresourceRange.levelCount = 1;
		imageViewCI.subresourceRange.baseArrayLayer = 0;
		imageViewCI.subresourceRange.layerCount = 1;
		imageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		VK_CHECK_RESULT(vkCreateImageView(device, &imageViewCI, nullptr, &HWRVisBuffer.view));

		cmdBuf = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.levelCount = 1;
		subresourceRange.layerCount = 1;

		vks::tools::setImageLayout(
			cmdBuf,
			HWRVisBuffer.image,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			subresourceRange);

		vulkanDevice->flushCommandBuffer(cmdBuf, queue);


		imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCI.imageType = VK_IMAGE_TYPE_2D;
		imageCI.format = VK_FORMAT_R32_UINT;
		imageCI.extent = { width, height, 1 };
		imageCI.mipLevels = 1;
		imageCI.arrayLayers = 1;
		imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCI.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		VK_CHECK_RESULT(vkCreateImage(device, &imageCI, nullptr, &FinalVisBuffer.image));
		vkGetImageMemoryRequirements(device, FinalVisBuffer.image, &memReqs);

		memAllloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		memAllloc.allocationSize = memReqs.size;
		memAllloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device, &memAllloc, nullptr, &FinalVisBuffer.mem));
		VK_CHECK_RESULT(vkBindImageMemory(device, FinalVisBuffer.image, FinalVisBuffer.mem, 0));

		imageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCI.image = FinalVisBuffer.image;
		imageViewCI.format = VK_FORMAT_R32_UINT;
		imageViewCI.subresourceRange.baseMipLevel = 0;
		imageViewCI.subresourceRange.levelCount = 1;
		imageViewCI.subresourceRange.baseArrayLayer = 0;
		imageViewCI.subresourceRange.layerCount = 1;
		imageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		VK_CHECK_RESULT(vkCreateImageView(device, &imageViewCI, nullptr, &FinalVisBuffer.view));

		cmdBuf = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.levelCount = 1;
		subresourceRange.layerCount = 1;

		vks::tools::setImageLayout(
			cmdBuf,
			FinalVisBuffer.image,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_GENERAL,
			subresourceRange);

		vulkanDevice->flushCommandBuffer(cmdBuf, queue);


		imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCI.imageType = VK_IMAGE_TYPE_2D;
		imageCI.format = VK_FORMAT_R32_SFLOAT;
		imageCI.extent = { width, height, 1 };
		imageCI.mipLevels = 1;
		imageCI.arrayLayers = 1;
		imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCI.usage = VK_IMAGE_USAGE_STORAGE_BIT;

		VK_CHECK_RESULT(vkCreateImage(device, &imageCI, nullptr, &FinalZBuffer.image));
		vkGetImageMemoryRequirements(device, FinalZBuffer.image, &memReqs);

		memAllloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		memAllloc.allocationSize = memReqs.size;
		memAllloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device, &memAllloc, nullptr, &FinalZBuffer.mem));
		VK_CHECK_RESULT(vkBindImageMemory(device, FinalZBuffer.image, FinalZBuffer.mem, 0));

		imageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCI.image = FinalZBuffer.image;
		imageViewCI.format = VK_FORMAT_R32_SFLOAT;
		imageViewCI.subresourceRange.baseMipLevel = 0;
		imageViewCI.subresourceRange.levelCount = 1;
		imageViewCI.subresourceRange.baseArrayLayer = 0;
		imageViewCI.subresourceRange.layerCount = 1;
		imageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		VK_CHECK_RESULT(vkCreateImageView(device, &imageViewCI, nullptr, &FinalZBuffer.view));

		cmdBuf = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.levelCount = 1;
		subresourceRange.layerCount = 1;

		vks::tools::setImageLayout(
			cmdBuf,
			FinalZBuffer.image,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_GENERAL,
			subresourceRange);

		vulkanDevice->flushCommandBuffer(cmdBuf, queue);



		imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCI.imageType = VK_IMAGE_TYPE_2D;
		imageCI.format = VK_FORMAT_R64_UINT;
		imageCI.extent = { width, height, 1 };
		imageCI.mipLevels = 1;
		imageCI.arrayLayers = 1;
		imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCI.usage = VK_IMAGE_USAGE_STORAGE_BIT;

		VK_CHECK_RESULT(vkCreateImage(device, &imageCI, nullptr, &SWRBuffer.image));
		vkGetImageMemoryRequirements(device, SWRBuffer.image, &memReqs);

		memAllloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		memAllloc.allocationSize = memReqs.size;
		memAllloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device, &memAllloc, nullptr, &SWRBuffer.mem));
		VK_CHECK_RESULT(vkBindImageMemory(device, SWRBuffer.image, SWRBuffer.mem, 0));

		imageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCI.image = SWRBuffer.image;
		imageViewCI.format = VK_FORMAT_R64_UINT;
		imageViewCI.subresourceRange.baseMipLevel = 0;
		imageViewCI.subresourceRange.levelCount = 1;
		imageViewCI.subresourceRange.baseArrayLayer = 0;
		imageViewCI.subresourceRange.layerCount = 1;
		imageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		VK_CHECK_RESULT(vkCreateImageView(device, &imageViewCI, nullptr, &SWRBuffer.view));

		cmdBuf = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.levelCount = 1;
		subresourceRange.layerCount = 1;

		vks::tools::setImageLayout(
			cmdBuf,
			SWRBuffer.image,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_GENERAL,
			subresourceRange);

		vulkanDevice->flushCommandBuffer(cmdBuf, queue);





		uboshading.invView = glm::inverse(camera.matrices.view);
		uboshading.invProj = glm::inverse(camera.matrices.perspective);
		uboshading.camPos = camera.position * -1.0f;
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			sizeof(UBOShading),
			&uniformBuffers.shadingMats.buffer,
			&uniformBuffers.shadingMats.memory,
			&uboshading));
		uniformBuffers.shadingMats.device = device;
		VK_CHECK_RESULT(uniformBuffers.shadingMats.map());
	}

	void createHiZBuffer()
	{
		uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
		textures.hizbuffer.mipLevels = mipLevels;
		const VkFormat format = VK_FORMAT_R32_SFLOAT;
		VkImageCreateInfo imageCI = vks::initializers::imageCreateInfo();
		imageCI.imageType = VK_IMAGE_TYPE_2D;
		imageCI.format = format;
		imageCI.extent.width = width;
		imageCI.extent.height = height;
		imageCI.extent.depth = 1;
		imageCI.mipLevels = mipLevels;
		imageCI.arrayLayers = 1;
		imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCI.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		VK_CHECK_RESULT(vkCreateImage(device, &imageCI, nullptr, &textures.hizbuffer.image));
		
		VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
		VkMemoryRequirements memReqs;
		vkGetImageMemoryRequirements(device, textures.hizbuffer.image, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		memAlloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &textures.hizbuffer.deviceMemory));
		VK_CHECK_RESULT(vkBindImageMemory(device, textures.hizbuffer.image, textures.hizbuffer.deviceMemory, 0));

		// Image view
		VkImageViewCreateInfo viewCI = vks::initializers::imageViewCreateInfo();
		viewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewCI.format = format;
		viewCI.subresourceRange = {};
		viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewCI.subresourceRange.levelCount = mipLevels;
		viewCI.subresourceRange.layerCount = 1;
		viewCI.image = textures.hizbuffer.image;
		VK_CHECK_RESULT(vkCreateImageView(device, &viewCI, nullptr, &textures.hizbuffer.view));
		// Sampler
		VkSamplerCreateInfo samplerCI = vks::initializers::samplerCreateInfo();
		samplerCI.magFilter = VK_FILTER_NEAREST;
		samplerCI.minFilter = VK_FILTER_NEAREST;
		samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.minLod = 0.0f;
		samplerCI.maxLod = static_cast<float>(mipLevels);
		samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		VK_CHECK_RESULT(vkCreateSampler(device, &samplerCI, nullptr, &textures.hizbuffer.sampler));

		textures.hizbuffer.descriptor.imageView = textures.hizbuffer.view;
		textures.hizbuffer.descriptor.sampler = textures.hizbuffer.sampler;
		textures.hizbuffer.descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		textures.hizbuffer.device = vulkanDevice;


		for (int i = 0; i < mipLevels; i++)
		{
			VkImageView iView;
			VkImageViewCreateInfo viewCI = vks::initializers::imageViewCreateInfo();
			viewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewCI.format = format;
			viewCI.subresourceRange = {};
			viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			viewCI.subresourceRange.baseMipLevel = i;
			viewCI.subresourceRange.levelCount = 1;
			viewCI.subresourceRange.layerCount = 1;
			viewCI.image = textures.hizbuffer.image;
			VK_CHECK_RESULT(vkCreateImageView(device, &viewCI, nullptr, &iView));
			hizImageViews.emplace_back(iView);
		}

		VkCommandBuffer cmdBuf = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = textures.hizbuffer.mipLevels;
		subresourceRange.layerCount = 1;

		vks::tools::setImageLayout(
			cmdBuf,
			textures.hizbuffer.image,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_GENERAL,
			subresourceRange);

		vulkanDevice->flushCommandBuffer(cmdBuf, queue);
		vkDeviceWaitIdle(device);
	}

	void createModelMatsBuffer()
	{
		vks::Buffer modelMatsStaging;
		//std::cout << "size of clusterInfo:" << sizeof(ClusterInfo) << std::endl;

		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			modelMats.size() * sizeof(glm::mat4),
			&modelMatsStaging.buffer,
			&modelMatsStaging.memory,
			modelMats.data()));

		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			modelMats.size() * sizeof(glm::mat4),
			&modelMatsBuffer.buffer,
			&modelMatsBuffer.memory,
			nullptr));
		VkCommandBuffer copyCmd = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		VkBufferCopy copyRegion = {};

		copyRegion.size = modelMats.size() * sizeof(glm::mat4);
		vkCmdCopyBuffer(copyCmd, modelMatsStaging.buffer, modelMatsBuffer.buffer, 1, &copyRegion);

		vulkanDevice->flushCommandBuffer(copyCmd, queue, true);//TODO: get transfer queue here

		vkDestroyBuffer(vulkanDevice->logicalDevice, modelMatsStaging.buffer, nullptr);
		vkFreeMemory(vulkanDevice->logicalDevice, modelMatsStaging.memory, nullptr);
	}

	// Prepare and initialize uniform buffer containing shader uniforms
	void prepareUniformBuffers()
	{
		// Object vertex shader uniform buffer
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&uniformBuffers.object,
			sizeof(uboMatrices1)));

		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&uniformBuffers.topObject,
			sizeof(uboMatrices1)));

		// Skybox vertex shader uniform buffer
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&uniformBuffers.skybox,
			sizeof(uboMatrices1)));

		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&uniformBuffers.topSkybox,
			sizeof(uboMatrices1)));

		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&uniformBuffers.cube,
			sizeof(uboMatrices1)));

		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&uniformBuffers.topCube,
			sizeof(uboMatrices1)));


		// Shared parameter uniform buffer
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&uniformBuffers.params,
			sizeof(uboParams)));

		// Map persistent
		VK_CHECK_RESULT(uniformBuffers.object.map());
		VK_CHECK_RESULT(uniformBuffers.skybox.map());
		VK_CHECK_RESULT(uniformBuffers.cube.map());
		VK_CHECK_RESULT(uniformBuffers.params.map());
		VK_CHECK_RESULT(uniformBuffers.topObject.map());
		VK_CHECK_RESULT(uniformBuffers.topSkybox.map());
		VK_CHECK_RESULT(uniformBuffers.topCube.map());

		updateUniformBuffers();
		updateParams();
	}

	void updateUniformBuffers()
	{
		// 3D object
		uboMatrices1.projection = camera.matrices.perspective;
		uboMatrices1.view = camera.matrices.view;
		uboMatrices1.model = model0;
		uboMatrices1.camPos = camera.position * -1.0f;
		memcpy(uniformBuffers.object.mapped, &uboMatrices1, sizeof(uboMatrices1));

		uboMatrices2.projection = glm::perspective(90.0f, 1.0f, 0.1f, 100.0f);
		uboMatrices2.view = glm::lookAt(glm::vec3(0, -3, 1), glm::vec3(0, 0, 1), glm::vec3(0, 0, -1));
		uboMatrices2.model = model0;
		uboMatrices2.camPos = glm::vec3(0, -5, 0);
		memcpy(uniformBuffers.topObject.mapped, &uboMatrices2, sizeof(uboMatrices2));

		uboMatrices3.projection = camera.matrices.perspective;
		uboMatrices3.view = camera.matrices.view;
		uboMatrices3.model = model1;
		uboMatrices3.camPos = camera.position * -1.0f;
		memcpy(uniformBuffers.cube.mapped, &uboMatrices3, sizeof(uboMatrices3));

		uboMatrices4.projection = glm::perspective(90.0f, 1.0f, 0.1f, 100.0f);
		uboMatrices4.view = glm::lookAt(glm::vec3(0, -3, 1), glm::vec3(0, 0, 1), glm::vec3(0, 0, -1));
		uboMatrices4.model = model1;
		uboMatrices4.camPos = glm::vec3(0, -5, 0);
		memcpy(uniformBuffers.topCube.mapped, &uboMatrices4, sizeof(uboMatrices4));

		// Skybox
		uboMatrices1.model = glm::mat4(glm::mat3(camera.matrices.view));
		memcpy(uniformBuffers.skybox.mapped, &uboMatrices1, sizeof(uboMatrices1));

		uboMatrices2.model = glm::mat4(glm::mat3(uboMatrices2.view));
		memcpy(uniformBuffers.topSkybox.mapped, &uboMatrices2, sizeof(uboMatrices2));

		// Error
		uboErrorMatrices.view = camera.matrices.view;
		uboErrorMatrices.proj = camera.matrices.perspective;
		uboErrorMatrices.camRight = camera.getRight();
		uboErrorMatrices.camUp = camera.getUp();
		memcpy(errorUniformBuffer.mapped, &uboErrorMatrices, sizeof(UBOErrorMatrices));
		errorUniformBuffer.flush();

		uboshading.invView = glm::inverse(camera.matrices.view);
		uboshading.invProj = glm::inverse(camera.matrices.perspective);
		uboshading.camPos = camera.position * -1.0f;
		memcpy(uniformBuffers.shadingMats.mapped, &uboshading, sizeof(UBOShading));
		uniformBuffers.shadingMats.flush();
	}

	void updateParams()
	{
		const float p = 15.0f;
		uboParams.lights[0] = glm::vec4(-p, -p*0.5f, -p, 1.0f);
		uboParams.lights[1] = glm::vec4(-p, -p*0.5f,  p, 1.0f);
		uboParams.lights[2] = glm::vec4( p, -p*0.5f,  p, 1.0f);
		uboParams.lights[3] = glm::vec4( p, -p*0.5f, -p, 1.0f);

		memcpy(uniformBuffers.params.mapped, &uboParams, sizeof(uboParams));
	}

	void draw()
	{
		VulkanExampleBase::prepareFrame();

		VK_CHECK_RESULT(vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX));
		VK_CHECK_RESULT(vkResetFences(device, 1, &inFlightFence));

		if (hwrDrawIndexedIndirectBuffer.mapped)
		{
			//
			hwrDrawIndexedIndirect.indexCount = 0;
			memcpy(hwrDrawIndexedIndirectBuffer.mapped, &hwrDrawIndexedIndirect, sizeof(DrawIndexedIndirect));
			hwrDrawIndexedIndirectBuffer.flush();

			uint32_t zero = 0;
			memcpy(swrNumVerticesBuffer.mapped, &zero, sizeof(uint32_t));
			swrNumVerticesBuffer.flush();
			//vkDeviceWaitIdle(device);
		}

		uboCullingMatrices.currView = camera.matrices.view;
		uboCullingMatrices.currProj = camera.matrices.perspective;
		memcpy(cullingUniformBuffer.mapped, &uboCullingMatrices, sizeof(uboCullingMatrices));
		cullingUniformBuffer.flush();

		VkSubmitInfo submitInfo0 = vks::initializers::submitInfo();
		submitInfo0.commandBufferCount = 1;
		submitInfo0.pCommandBuffers = &drawCmdBuffers[currentBuffer];
		submitInfo0.waitSemaphoreCount = 1;
		submitInfo0.pWaitSemaphores = &semaphores.presentComplete;
		VkPipelineStageFlags submitPipelineStages0 = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
		submitInfo0.pWaitDstStageMask = &submitPipelineStages0;
		static bool useFenceBetweenCullingAndRaster = false;
		if (useFenceBetweenCullingAndRaster)
		{
			VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo0, cullingFinishedFence));
			VK_CHECK_RESULT(vkWaitForFences(device, 1, &cullingFinishedFence, VK_TRUE, UINT64_MAX));
			VK_CHECK_RESULT(vkResetFences(device, 1, &cullingFinishedFence))
		}
		else
		{
			VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo0, VK_NULL_HANDLE));
		}

		VkSubmitInfo submitInfo1 = vks::initializers::submitInfo();
		submitInfo1.commandBufferCount = 1;
		submitInfo1.pCommandBuffers = &rasterizeCommandBuffers[currentBuffer];
		submitInfo1.signalSemaphoreCount = 1;
		submitInfo1.pSignalSemaphores = &semaphores.renderComplete;
		VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo1, inFlightFence));

		VulkanExampleBase::submitFrame();

		uboCullingMatrices.lastView = camera.matrices.view;
		uboCullingMatrices.lastProj = camera.matrices.perspective;
		memcpy(cullingUniformBuffer.mapped, &uboCullingMatrices, sizeof(uboCullingMatrices));
		cullingUniformBuffer.flush();
	}

	void setupRenderPass()
	{
		VulkanExampleBase::setupRenderPass();

		std::array<VkAttachmentDescription, 2> attachments = {};
		// Color attachment
		attachments[0].format = swapChain.colorFormat;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		// Depth attachment
		attachments[1].format = depthFormat;
		attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference colorReference = {};
		colorReference.attachment = 0;
		colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depthReference = {};
		depthReference.attachment = 1;
		depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpassDescription = {};
		subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDescription.colorAttachmentCount = 1;
		subpassDescription.pColorAttachments = &colorReference;
		subpassDescription.pDepthStencilAttachment = &depthReference;
		subpassDescription.inputAttachmentCount = 0;
		subpassDescription.pInputAttachments = nullptr;
		subpassDescription.preserveAttachmentCount = 0;
		subpassDescription.pPreserveAttachments = nullptr;
		subpassDescription.pResolveAttachments = nullptr;

		// Subpass dependencies for layout transitions
		std::array<VkSubpassDependency, 2> dependencies;

		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		dependencies[0].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
		dependencies[0].dependencyFlags = 0;

		dependencies[1].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[1].dstSubpass = 0;
		dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[1].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[1].srcAccessMask = 0;
		dependencies[1].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
		dependencies[1].dependencyFlags = 0;

		VkRenderPassCreateInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		renderPassInfo.pAttachments = attachments.data();
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpassDescription;
		renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
		renderPassInfo.pDependencies = dependencies.data();

		VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassInfo, nullptr, &topViewRenderPass));


		// Color attachment
		attachments[0].format = VK_FORMAT_R32_UINT;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		// Depth attachment
		attachments[1].format = VK_FORMAT_D32_SFLOAT;
		attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		colorReference.attachment = 0;
		colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		depthReference.attachment = 1;
		depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDescription.colorAttachmentCount = 1;
		subpassDescription.pColorAttachments = &colorReference;
		subpassDescription.pDepthStencilAttachment = &depthReference;
		subpassDescription.inputAttachmentCount = 0;
		subpassDescription.pInputAttachments = nullptr;
		subpassDescription.preserveAttachmentCount = 0;
		subpassDescription.pPreserveAttachments = nullptr;
		subpassDescription.pResolveAttachments = nullptr;

		// Subpass dependencies for layout transitions

		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		dependencies[0].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
		dependencies[0].dependencyFlags = 0;

		dependencies[1].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[1].dstSubpass = 0;
		dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[1].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[1].srcAccessMask = 0;
		dependencies[1].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
		dependencies[1].dependencyFlags = 0;

		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		renderPassInfo.pAttachments = attachments.data();
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpassDescription;
		renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
		renderPassInfo.pDependencies = dependencies.data();

		VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassInfo, nullptr, &hwRastRenderPass));

	}

	void setupDepthStencil()
	{
		VkImageCreateInfo imageCI{};
		imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCI.imageType = VK_IMAGE_TYPE_2D;
		imageCI.format = depthFormat;
		imageCI.extent = { width, height, 1 };
		imageCI.mipLevels = 1;
		imageCI.arrayLayers = 1;
		imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCI.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

		VK_CHECK_RESULT(vkCreateImage(device, &imageCI, nullptr, &depthStencil.image));
		VkMemoryRequirements memReqs{};
		vkGetImageMemoryRequirements(device, depthStencil.image, &memReqs);

		VkMemoryAllocateInfo memAllloc{};
		memAllloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		memAllloc.allocationSize = memReqs.size;
		memAllloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device, &memAllloc, nullptr, &depthStencil.mem));
		VK_CHECK_RESULT(vkBindImageMemory(device, depthStencil.image, depthStencil.mem, 0));

		VkImageViewCreateInfo imageViewCI{};
		imageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCI.image = depthStencil.image;
		imageViewCI.format = depthFormat;
		imageViewCI.subresourceRange.baseMipLevel = 0;
		imageViewCI.subresourceRange.levelCount = 1;
		imageViewCI.subresourceRange.baseArrayLayer = 0;
		imageViewCI.subresourceRange.layerCount = 1;
		imageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		// Stencil aspect should only be set on depth + stencil formats (VK_FORMAT_D16_UNORM_S8_UINT..VK_FORMAT_D32_SFLOAT_S8_UINT
		/*if (depthFormat >= VK_FORMAT_D16_UNORM_S8_UINT) {
			imageViewCI.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}*/
		VK_CHECK_RESULT(vkCreateImageView(device, &imageViewCI, nullptr, &depthStencil.view));

		VkSamplerCreateInfo samplerCI = vks::initializers::samplerCreateInfo();
		samplerCI.magFilter = VK_FILTER_NEAREST;
		samplerCI.minFilter = VK_FILTER_NEAREST;
		samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		VK_CHECK_RESULT(vkCreateSampler(device, &samplerCI, nullptr, &depthStencilSampler));

		VkCommandBuffer cmdBuf = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		subresourceRange.levelCount = 1;
		subresourceRange.layerCount = 1;

		vks::tools::setImageLayout(
			cmdBuf,
			depthStencil.image,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			subresourceRange);

		vulkanDevice->flushCommandBuffer(cmdBuf, queue);
	}

	void createHWRasterizeFramebuffer()
	{
		VkImageView attachments[2];

		attachments[0] = HWRVisBuffer.view;
		attachments[1] = HWRZBuffer.view;

		VkFramebufferCreateInfo frameBufferCreateInfo = {};
		frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		frameBufferCreateInfo.pNext = NULL;
		frameBufferCreateInfo.renderPass = hwRastRenderPass;
		frameBufferCreateInfo.attachmentCount = 2;
		frameBufferCreateInfo.pAttachments = attachments;
		frameBufferCreateInfo.width = width;
		frameBufferCreateInfo.height = height;
		frameBufferCreateInfo.layers = 1;

		
		VK_CHECK_RESULT(vkCreateFramebuffer(vulkanDevice->logicalDevice, &frameBufferCreateInfo, nullptr, &HWRFramebuffer));
	}

	void createFence()
	{
		VkFenceCreateInfo fenceInfo = {};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Optional flags

		VK_CHECK_RESULT(vkCreateFence(device, &fenceInfo, NULL, &inFlightFence));

		fenceInfo.flags = 0;
		VK_CHECK_RESULT(vkCreateFence(device, &fenceInfo, NULL, &cullingFinishedFence));
	}

	virtual void createCommandBuffers()
	{
		VulkanExampleBase::createCommandBuffers();

		rasterizeCommandBuffers.resize(swapChain.imageCount);

		VkCommandBufferAllocateInfo cmdBufAllocateInfo =
			vks::initializers::commandBufferAllocateInfo(
				cmdPool,
				VK_COMMAND_BUFFER_LEVEL_PRIMARY,
				static_cast<uint32_t>(rasterizeCommandBuffers.size()));

		VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, rasterizeCommandBuffers.data()));
	}

	void prepare()
	{
		enabledDeviceExtensions.push_back(VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME);
		enabledDeviceExtensions.push_back(VK_EXT_SHADER_IMAGE_ATOMIC_INT64_EXTENSION_NAME);
		enabledInstanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
		VulkanExampleBase::prepare();
		createFence();
		createRasterizeBuffer();
		loadAssets();
		generateBRDFLUT();
		generateIrradianceCube();
		generatePrefilteredCube();
		createBVHTraversalBuffers();
		createCullingBuffers();
		createErrorProjectionBuffer();
		
		createHiZBuffer();
		createModelMatsBuffer();
		createHWRasterizeFramebuffer();
		prepareUniformBuffers();
		setupDescriptors();
		preparePipelines();
		buildCommandBuffers();
		prepared = true;
	}

	virtual void render()
	{
		if (!prepared)
			return;
		draw();
		if (camera.updated)
		{
			updateUniformBuffers();
		}
	}

	virtual void viewChanged()
	{
		updateUniformBuffers();
	}

	virtual void OnUpdateUIOverlay(vks::UIOverlay *overlay)
	{
		bool rebuildCB = false;
		std::string s1 = "Num triangles without vulkanite:" + std::to_string(scene.sceneIndicesCount / 3);
		overlay->text(s1.c_str());


		memcpy(&hwrDrawIndexedIndirect, hwrDrawIndexedIndirectBuffer.mapped, sizeof(DrawIndexedIndirect));
		uint32_t swrVertSize;
		memcpy(&swrVertSize, swrNumVerticesBuffer.mapped, sizeof(uint32_t));

		std::string s2 = "Num triangles hw raserized:" + std::to_string(hwrDrawIndexedIndirect.indexCount / 3);
		overlay->text(s2.c_str());
		std::string s3 = "Num triangles sw raserized:" + std::to_string(swrVertSize / 3);
		overlay->text(s3.c_str());
		std::string s4 = "Num triangles raserized in total:" + std::to_string((swrVertSize + hwrDrawIndexedIndirect.indexCount) / 3);
		overlay->text(s4.c_str());
		/*std::string s5 = "Num culled clusters:" + std::to_string(numCulledClusters);
		overlay->text(s5.c_str());*/

		if (overlay->header("Settings")) {
			if (overlay->inputFloat("Exposure", &uboParams.exposure, 0.1f, 2)) {
				updateParams();
			}
			if (overlay->inputFloat("Gamma", &uboParams.gamma, 0.1f, 2)) {
				updateParams();
			}
			if (overlay->checkBox("Skybox", &displaySkybox)) {
				rebuildCB = true;
			}
			if (overlay->checkBox("Top View", &vis_topView)) {
				rebuildCB = true;
			}
			if (overlay->checkBox("Software Rasterization", &cullingPushConstants.useSoftwareRasterization)) {
				rebuildCB = true;
			}
			if (overlay->checkBox("Frustrum&Occlusion Culling", &cullingPushConstants.useFrustrumOcclusionCulling)) {
				rebuildCB = true;
			}
			if (overlay->sliderInt("Threshold", &thresholdInt, 0, 1000))
			{
				rebuildCB = true;
			}
			if (overlay->sliderInt("Visualize Clusters", &renderingPushConstants.vis_clusters,0,3)) {
				rebuildCB = true;
			}
			if (overlay->sliderInt("LOD level", &vis_clusters_level, 0, naniteMesh.meshes.size() - 1))
			{
				rebuildCB = true;
			}
		}
		if (rebuildCB)
		{
			buildCommandBuffers();
		}
	}
};

VULKAN_EXAMPLE_MAIN()
