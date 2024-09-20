#pragma once

#include <vk_types.h>

class GfxDevice;

enum class VulkanObjectType {
	Buffer,
	Image,
	ImageView,
	Sampler,
	DescriptorSet,
	DescriptorSetLayout,
	DescriptorPool,
	Pipeline,
	PipelineLayout,
	CommandPool,
	Fence,
	Semaphore,
	ShaderModule
};

struct VulkanObject {
	VulkanObjectType type;
	uint64_t handle;
};

class LifetimeTracker {
public:
	void init( GfxDevice* gfx );
	void clean( );
	~LifetimeTracker( );

	template<typename T>
	void track( VulkanObjectType type, T object ) {
		objects.push_back( { type, (uint64_t)object } );
	}

private:
	void deleteObject( const VulkanObject& obj );

	GfxDevice* gfx;
	std::vector<VulkanObject> objects;
};