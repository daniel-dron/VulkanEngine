#include <pch.h>

#include <graphics/gfx_device.h>
#include "bindless.h"

void BindlessRegistry::Init( GfxDevice &gfx ) {
    // Create descriptor pool
    {
        VkDescriptorPoolSize pool_sizes[] = {
                { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, MaxBindlessImages },
                { VK_DESCRIPTOR_TYPE_SAMPLER, MaxSamplers } };

        const VkDescriptorPoolCreateInfo info = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                .pNext = nullptr,
                .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
                .maxSets = MaxBindlessImages * 2,
                .poolSizeCount = 2u,
                .pPoolSizes = pool_sizes };

        VK_CHECK( vkCreateDescriptorPool( gfx.device, &info, nullptr, &pool ) );
    }

    // Descriptor Set Layout
    {
        std::array bindings = {
                VkDescriptorSetLayoutBinding{
                        .binding = TextureBinding,
                        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                        .descriptorCount = MaxBindlessImages,
                        .stageFlags = VK_SHADER_STAGE_ALL },
                VkDescriptorSetLayoutBinding{
                        .binding = SamplersBinding,
                        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
                        .descriptorCount = MaxSamplers,
                        .stageFlags = VK_SHADER_STAGE_ALL },
        };

        std::array<VkDescriptorBindingFlags, 2> binding_flags = {
                VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
                VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT };

        VkDescriptorSetLayoutBindingFlagsCreateInfo flag_info = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
                .pNext = nullptr,
                .bindingCount = binding_flags.size( ),
                .pBindingFlags = binding_flags.data( ),
        };

        const VkDescriptorSetLayoutCreateInfo info = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .pNext = &flag_info,
                .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
                .bindingCount = bindings.size( ),
                .pBindings = bindings.data( ) };

        VK_CHECK( vkCreateDescriptorSetLayout( gfx.device, &info, nullptr, &layout ) );
    }

    // Allocate descriptor set
    {
        VkDescriptorSetAllocateInfo alloc_info = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .descriptorPool = pool,
                .descriptorSetCount = 1,
                .pSetLayouts = &layout,
        };

        VK_CHECK( vkAllocateDescriptorSets( gfx.device, &alloc_info, &set ) );


        const VkDebugUtilsObjectNameInfoEXT obj = {
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
                .pNext = nullptr,
                .objectType = VK_OBJECT_TYPE_DESCRIPTOR_SET,
                .objectHandle = reinterpret_cast<uint64_t>( set ),
                .pObjectName = "Bindless Descriptor Set" };
        vkSetDebugUtilsObjectNameEXT( gfx.device, &obj );
    }

    InitSamplers( gfx );
}

void BindlessRegistry::Cleanup( const GfxDevice &gfx ) {
    vkDestroySampler( gfx.device, nearestSampler, nullptr );
    vkDestroySampler( gfx.device, linearSampler, nullptr );
    vkDestroySampler( gfx.device, shadowMapSampler, nullptr );

    vkDestroyDescriptorSetLayout( gfx.device, layout, nullptr );
    vkDestroyDescriptorPool( gfx.device, pool, nullptr );
}

void BindlessRegistry::AddImage( const GfxDevice &gfx, ImageId id, const VkImageView view ) {
    VkDescriptorImageInfo image_info = {
            .imageView = view,
            .imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
    };

    const VkWriteDescriptorSet write_set = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = set,
            .dstBinding = TextureBinding,
            .dstArrayElement = id,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .pImageInfo = &image_info,
    };
    vkUpdateDescriptorSets( gfx.device, 1, &write_set, 0, nullptr );
}

void BindlessRegistry::AddSampler( GfxDevice &gfx, std::uint32_t id, const VkSampler sampler ) {
    VkDescriptorImageInfo info = { .sampler = sampler, .imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL };
    const VkWriteDescriptorSet write_set = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = set,
            .dstBinding = SamplersBinding,
            .dstArrayElement = id,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
            .pImageInfo = &info,
    };
    vkUpdateDescriptorSets( gfx.device, 1, &write_set, 0, nullptr );
}

void BindlessRegistry::InitSamplers( GfxDevice &gfx ) {
    static constexpr std::uint32_t NearestSamplerId = 0;
    static constexpr std::uint32_t LinearSamplerId = 1;
    static constexpr std::uint32_t ShadowSamplerId = 2;

    {
        constexpr VkSamplerCreateInfo create_info = {
                .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                .magFilter = VK_FILTER_NEAREST,
                .minFilter = VK_FILTER_NEAREST,
        };
        VK_CHECK( vkCreateSampler( gfx.device, &create_info, nullptr, &nearestSampler ) );
        AddSampler( gfx, NearestSamplerId, nearestSampler );
    }

    {
        constexpr VkSamplerCreateInfo create_info = {
                .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                .magFilter = VK_FILTER_LINEAR,
                .minFilter = VK_FILTER_LINEAR,
                .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                .mipLodBias = 0.0f,
                .anisotropyEnable = VK_TRUE,
                .maxAnisotropy = 16.0f,
                .minLod = 0.0f,
                .maxLod = 10.0f };
        VK_CHECK( vkCreateSampler( gfx.device, &create_info, nullptr, &linearSampler ) );
        AddSampler( gfx, LinearSamplerId, linearSampler );
    }

    {
        constexpr VkSamplerCreateInfo create_info = {
                .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                .magFilter = VK_FILTER_LINEAR,
                .minFilter = VK_FILTER_LINEAR,
                .compareEnable = VK_TRUE,
                .compareOp = VK_COMPARE_OP_GREATER_OR_EQUAL,
        };
        VK_CHECK( vkCreateSampler( gfx.device, &create_info, nullptr, &shadowMapSampler ) );
        AddSampler( gfx, ShadowSamplerId, shadowMapSampler );
    }
}
