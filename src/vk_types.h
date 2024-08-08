// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.
#pragma once

#include <fmt/core.h>
#include <vk_mem_alloc.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <glm/gtx/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>
#include <deque>
#include <functional>

#include "glm/ext/vector_float4.hpp"

#ifdef _DEBUG
#define ENABLE_DEBUG_UTILS
#define VK_CHECK(x)                                                       \
  do {                                                                    \
    VkResult err = x;                                                     \
    if (err) {                                                            \
      fmt::println("{} {} Detected Vulkan error: {}", __FILE__, __LINE__, \
                   string_VkResult(err));                                 \
      abort();                                                            \
    }                                                                     \
  } while (0)
#else
#undef ENABLE_DEBUG_UTILS
#define VK_CHECK(x) x
#endif

// Helper macro for early return
#define RETURN_IF_ERROR(expression) \
        if (auto result = (expression); !result) \
            return std::unexpected(result.error())


const glm::vec3 GlobalUp{ 0.0f, -1.0f, 0.0f };
const glm::vec3 GlobalRight{ 1.0f, 0.0f, 0.0f };
const glm::vec3 GlobalFront{ 0.0f, 0.0f, 1.0f };

using ImageID = uint32_t;
using MaterialID = uint32_t;

struct DeletionQueue {
	std::deque<std::function<void( )>> deletors;

	DeletionQueue( ) {
		deletors.clear( );
	}

	void pushFunction( std::function<void( )>&& deletor ) {
		deletors.push_back( std::move( deletor ) );
	}

	void flush( ) {
		// reverse iterate the deletion queue to execute all the functions
		for ( auto it = deletors.rbegin( ); it != deletors.rend( ); ++it ) {
			(*it)();  // call functors
		}

		deletors.clear( );
	}
};

struct AllocatedImage {
	VkImage image;
	VkImageView view;
	VmaAllocation allocation;
	VkExtent3D extent;
	VkFormat format;
};

struct GpuBuffer {
	VkBuffer buffer;
	VmaAllocation allocation;
	VmaAllocationInfo info;
	std::string name;
};

struct Vertex {
	glm::vec3 position;
	float uv_x;
	glm::vec3 normal;
	float uv_y;
	glm::vec4 color;
	glm::vec4 tangent;
};

// resources needed for a single mesh
struct GPUMeshBuffers {
	GpuBuffer indexBuffer;
	GpuBuffer vertexBuffer;
	VkDeviceAddress vertexBufferAddress;
};

// push constants for our mesh object to be drawn
struct GPUDrawPushConstants {
	glm::mat4 worldMatrix;
	VkDeviceAddress vertexBuffer;
};

//
// Rendering system types
//

enum class MaterialPass : uint8_t { MainColor, Transparent, Other };

struct MaterialPipeline {
	VkPipeline pipeline;
	VkPipelineLayout layout;
};

struct MaterialInstance {
	MaterialPipeline* pipeline;
	MaterialPass passType;
	VkDescriptorSet materialSet;
};

struct EngineStats {
	float frametime;
	int triangle_count;
	int drawcall_count;
	float scene_update_time;
	float mesh_draw_time;
};

struct Bounds {
	glm::vec3 origin;
	float sphereRadius;
	glm::vec3 extents;
};

struct DrawContext;

class IRenderable {
public:
	virtual ~IRenderable( ) = default;

private:
	virtual void Draw( const glm::mat4& topMatrix, DrawContext& ctx ) = 0;
};

struct Node : public IRenderable {
	// parent pointer must be a weak pointer to avoid circular dependencies
	std::weak_ptr<Node> parent;
	std::vector<std::shared_ptr<Node>> children;

	std::string name;

	glm::mat4 localTransform;
	glm::mat4 worldTransform;

	void refreshTransform( const glm::mat4& parentMatrix ) {
		worldTransform = parentMatrix * localTransform;
		for ( auto c : children ) {
			c->refreshTransform( worldTransform );
		}
	}

	void Draw( const glm::mat4& topMatrix, DrawContext& ctx ) override {
		for ( auto& c : children ) {
			c->Draw( topMatrix, ctx );
		}
	}
};

using vec3 = glm::vec3;
using vec4 = glm::vec4;
using mat4 = glm::mat4;
using quat = glm::quat;

struct GpuPointLightData {
	vec3 position;
	float radius;
	vec4 color;
	float diffuse;
	float specular;
	int pad;
	int pad2;
};

struct GpuSceneData {
	mat4 view;
	mat4 proj;
	mat4 viewproj;
	vec4 fog_color;
	vec3 camera_position;
	float ambient_light_factor;
	vec3 ambient_light_color;
	float fog_end;
	float fog_start;
	int number_of_lights;
	int pad;
	GpuPointLightData point_lights[10];
};

struct DrawStats {
	int triangle_count;
	int drawcall_count;
};
