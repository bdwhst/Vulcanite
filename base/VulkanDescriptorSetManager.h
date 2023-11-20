#pragma once
#include <vector>
#include <unordered_map>
#include <string>
#include "vulkan/vulkan.h"
class VulkanDescriptorSetManager
{
private:
    static VulkanDescriptorSetManager* instance;
    VkDevice device = nullptr;
    VkDescriptorPool descriptorPool = nullptr;
    std::unordered_map<std::string, std::vector<VkDescriptorSet>> descriptorSets;
    std::unordered_map<std::string, VkDescriptorSetLayout> descriptorSetLayouts;
    std::unordered_map<std::string, std::pair<std::vector<VkDescriptorSetLayoutBinding>, uint32_t>> descriptorSetLayoutBindings;
    VulkanDescriptorSetManager() {}
public:
    static VulkanDescriptorSetManager* getManager() {
        if (instance == nullptr) {
            instance = new VulkanDescriptorSetManager();
        }
        return instance;
    }
    static void destory()
    {
        if (instance)
        {
            for (auto [_, layout] : instance->descriptorSetLayouts)
            {
                vkDestroyDescriptorSetLayout(instance->device, layout, nullptr);
            }
            vkDestroyDescriptorPool(instance->device, instance->descriptorPool, nullptr);
        }
    }
    VulkanDescriptorSetManager(VulkanDescriptorSetManager const&) = delete;
    VulkanDescriptorSetManager& operator=(VulkanDescriptorSetManager const&) = delete;
    void addSetLayout(const std::string& layoutName, const std::vector<VkDescriptorSetLayoutBinding>& setBindings, uint32_t numSets = 1);
    void createLayoutsAndSets(VkDevice device);
    void writeToSet(const std::string& layoutName, uint32_t set, uint32_t binding, VkDescriptorBufferInfo* buffer);
    void writeToSet(const std::string& layoutName, uint32_t set, uint32_t binding, VkDescriptorImageInfo* image);
    const VkDescriptorSet& getSet(const std::string& layoutName, uint32_t set);
    const VkDescriptorSetLayout& getSetLayout(const std::string& layoutName);
};