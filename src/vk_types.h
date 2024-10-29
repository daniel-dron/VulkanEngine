/******************************************************************************
******************************************************************************
**                                                                           **
**                             Twilight Engine                               **
**                                                                           **
**                  Copyright (c) 2024-present Daniel Dron                   **
**                                                                           **
**            This software is released under the MIT License.               **
**                 https://opensource.org/licenses/MIT                       **
**                                                                           **
******************************************************************************
******************************************************************************/

#pragma once

#include <deque>
#include <functional>

#include <glm/gtx/quaternion.hpp>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

#ifdef _DEBUG
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan.h>
#define ENABLE_DEBUG_UTILS
#define VKCALL( x )                                                                                                    \
    do {                                                                                                               \
        VkResult err = x;                                                                                              \
        if ( err ) {                                                                                                   \
            TL_Engine::Get( ).console.AddLog( "{} {} Detected Vulkan error: {}", __FILE__, __LINE__,                   \
                                              string_VkResult( err ) );                                                \
            abort( );                                                                                                  \
        }                                                                                                              \
    }                                                                                                                  \
    while ( 0 )
#else
#undef ENABLE_DEBUG_UTILS
#define VKCALL( x ) x
#endif

#define WIDTH 2560
#define HEIGHT 1440

// Helper macro for early return
#define RETURN_IF_ERROR( expression )                                                                                  \
    if ( auto result = ( expression ); !result )                                                                       \
    return std::unexpected( result.error( ) )


constexpr glm::vec3 GLOBAL_UP{ 0.0f, 1.0f, 0.0f };
constexpr glm::vec3 GLOBAL_RIGHT{ 1.0f, 0.0f, 0.0f };
constexpr glm::vec3 GLOBAL_FRONT{ 0.0f, 0.0f, 1.0f };

using ImageId = uint32_t;
using MultiFrameImageId = uint32_t;
using MeshId = uint32_t;
using MaterialId = uint32_t;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef float f32;
typedef double f64;

template<typename T>
T *PtrTo( T &&v ) {
    return &v;
}

struct DeletionQueue {
    std::deque<std::function<void( )>> deletors;

    DeletionQueue( ) { deletors.clear( ); }

    void PushFunction( std::function<void( )> &&deletor ) { deletors.push_back( std::move( deletor ) ); }

    void Flush( ) {
        // reverse iterate the deletion queue to execute all the functions
        for ( auto it = deletors.rbegin( ); it != deletors.rend( ); ++it ) {
            ( *it )( ); // call functors
        }

        deletors.clear( );
    }
};

class TL_VkContext;

struct GpuBuffer {
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = { };
    VmaAllocationInfo info = { };
    std::string name;
    VkDeviceAddress deviceAddress = 0;

    void Upload( const TL_VkContext &gfx, const void *data, size_t size ) const;
    VkDeviceAddress GetDeviceAddress( const TL_VkContext &gfx );
};

struct EngineStats {
    float frametime;
    uint64_t triangleCount;
    int drawcallCount;
    float sceneUpdateTime;
    float meshDrawTime;
};


using Vec2 = glm::vec2;
using Vec3 = glm::vec3;
using Vec4 = glm::vec4;
using Mat4 = glm::mat4;
using Quat = glm::quat;

struct GpuPointLightData {
    Vec3 position;
    float constant;
    Vec3 color;
    float linear;
    float quadratic;
    float pad1;
    float pad2;
    float pad3;
};

struct GpuDirectionalLight {
    Vec3 direction;
    int pad1;
    Vec4 color;
    Mat4 proj;
    Mat4 view;
    ImageId shadowMap;
    int pad2;
    int pad3;
    int pad4;
};

struct GpuSceneData {
    Mat4 view;
    Mat4 proj;
    Mat4 viewproj;
    Mat4 lightProj;
    Mat4 lightView;
    Vec4 fogColor;
    Vec3 cameraPosition;
    float ambientLightFactor;
    Vec3 ambientLightColor;
    float fogEnd;
    float fogStart;
    VkDeviceAddress materials;
    int numberOfDirectionalLights;
    int numberOfPointLights;
};

struct DrawStats {
    int triangleCount;
    int drawcallCount;
};
