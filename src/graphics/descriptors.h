﻿#pragma once

#include <vk_types.h>

class GfxDevice;

class DescriptorLayoutBuilder {
public:
	void AddBinding( uint32_t binding, VkDescriptorType type );
	void Clear( );

	VkDescriptorSetLayout Build( VkDevice device, VkShaderStageFlags shaderStages, void* pNext, VkDescriptorSetLayoutCreateFlags flags = 0 );

private:
	std::vector<VkDescriptorSetLayoutBinding> bindings;
};

class DescriptorAllocator {
public:
	struct PoolSizeRatio {
		VkDescriptorType type;
		float ratio;
	};

	VkDescriptorPool pool;

	void InitPool( VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios );
	void ClearDescriptors( VkDevice device );
	void DestroyPool( VkDevice device );

	VkDescriptorSet Allocate( VkDevice device, VkDescriptorSetLayout layout );
};

class DescriptorAllocatorGrowable {
public:
	struct PoolSizeRatio {
		VkDescriptorType type;
		float ratio;
	};

	void Init( VkDevice device, uint32_t initialSets, std::span<PoolSizeRatio> poolRatios );
	void ClearPools( VkDevice device );
	void DestroyPools( VkDevice device );

	VkDescriptorSet Allocate( VkDevice device, VkDescriptorSetLayout layout, void* pNext = nullptr );

private:
	VkDescriptorPool GetPool( VkDevice device );
	VkDescriptorPool CreatePool( VkDevice device, uint32_t setCount, std::span<PoolSizeRatio> poolRatios );

	std::vector<PoolSizeRatio> ratios;
	std::vector<VkDescriptorPool> fullPools;
	std::vector<VkDescriptorPool> readyPools;
	uint32_t setsPerPool;
};

class DescriptorWriter {
public:
	std::deque<VkDescriptorImageInfo> imageInfos;
	std::deque<VkDescriptorBufferInfo> bufferInfos;
	std::vector<VkWriteDescriptorSet> writes;

	void WriteImage( int binding, VkImageView image, VkSampler sampler, VkImageLayout layout, VkDescriptorType type );
	void WriteBuffer( int binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type );

	void Clear( );
	void UpdateSet( VkDevice device, VkDescriptorSet set );
};

namespace Descriptor {

	VkDescriptorSetLayoutBinding CreateLayoutBinding( uint32_t binding, VkDescriptorType type );
	VkDescriptorSetLayout CreateDescriptorSetLayout(VkDevice device, VkDescriptorSetLayoutBinding *bindings, uint32_t count, VkDescriptorSetLayoutCreateFlags flags);

}