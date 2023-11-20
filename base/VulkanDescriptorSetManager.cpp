#include "VulkanDescriptorSetManager.h"
#include "VulkanInitializers.hpp"
#include "VulkanTools.h"
#include <exception>


void VulkanDescriptorSetManager::addSetLayout(const std::string& setName, const std::vector<VkDescriptorSetLayoutBinding>& setBindings, uint32_t numSets)
{
    auto newLayout = std::make_pair(setBindings, numSets);
    if(numSets<1)
    {
        throw std::runtime_error("ERROR: Invalid number of sets");
    }
    if(descriptorSetLayoutBindings.count(setName))
    {
        throw std::runtime_error("ERROR: Two descriptor sets have the same name");
    }
    descriptorSetLayoutBindings[setName] = newLayout;
}

void VulkanDescriptorSetManager::createLayoutsAndSets(VkDevice device)
{
    this->device = device;
    if(descriptorSetLayouts.size()==descriptorSetLayoutBindings.size()) return;
    std::vector<VkDescriptorPoolSize> poolSizes;
    std::unordered_map<VkDescriptorType,int> typeCount;
    uint32_t maxSets=0;
    for(const auto& [name, bindings]:descriptorSetLayoutBindings)
    {
        int numSets = bindings.second;
        maxSets += numSets;
        for(const auto& binding:bindings.first)
        {
            typeCount[binding.descriptorType] += numSets;
        }
    }
    for(const auto& [type, count]:typeCount)
    {
        poolSizes.emplace_back(vks::initializers::descriptorPoolSize(type, count));
    }
    VkDescriptorPoolCreateInfo descriptorPoolInfo =	vks::initializers::descriptorPoolCreateInfo(poolSizes, maxSets);
    VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));

    for(const auto& [name, bindings]:descriptorSetLayoutBindings)
    {
        int numSets = bindings.second;
        VkDescriptorSetLayoutCreateInfo layoutCI = vks::initializers::descriptorSetLayoutCreateInfo(bindings.first);
        VkDescriptorSetLayout setLayout;
        VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &layoutCI, nullptr, &setLayout));
        descriptorSetLayouts[name]=setLayout;
        for(int i=0;i<numSets;i++)
        {
            VkDescriptorSet set;
            VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &setLayout, 1);
            VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &set));
            descriptorSets[name].emplace_back(set);
        }
    }
}

void VulkanDescriptorSetManager::writeToSet(const std::string& layoutName, uint32_t set, uint32_t binding, VkDescriptorBufferInfo* buffer)
{
    auto type = descriptorSetLayoutBindings[layoutName].first[binding].descriptorType;
    auto writeSet = vks::initializers::writeDescriptorSet(descriptorSets[layoutName][set], type, binding, buffer);
    vkUpdateDescriptorSets(device, 1, &writeSet, 0, NULL);
}

void VulkanDescriptorSetManager::writeToSet(const std::string& layoutName, uint32_t set, uint32_t binding, VkDescriptorImageInfo* image)
{
    auto type = descriptorSetLayoutBindings[layoutName].first[binding].descriptorType;
    auto writeSet = vks::initializers::writeDescriptorSet(descriptorSets[layoutName][set], type, binding, image);
    vkUpdateDescriptorSets(device, 1, &writeSet, 0, NULL);
}

const VkDescriptorSet& VulkanDescriptorSetManager::getSet(const std::string& layoutName, uint32_t set)
{
    if(!descriptorSets.count(layoutName))
    {
        throw std::runtime_error("ERROR: Invalid set name");
    }
    return descriptorSets[layoutName][set];
}

const VkDescriptorSetLayout& VulkanDescriptorSetManager::getSetLayout(const std::string& layoutName)
{
    if (!descriptorSetLayouts.count(layoutName))
    {
        throw std::runtime_error("ERROR: Invalid layout name");
    }
    return descriptorSetLayouts[layoutName];
}

VulkanDescriptorSetManager* VulkanDescriptorSetManager::instance = nullptr;