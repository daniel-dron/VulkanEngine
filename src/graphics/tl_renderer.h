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

#include <vk_types.h>
#include <graphics/tl_vkcontext.h>
#include <graphics/resources/tl_pipeline.h>

namespace TL {
    struct SceneBuffer {
        Mat4 view;
        Mat4 projection;
        Mat4 viewProjection;
        // Inverse
        Mat4 viewInv;
        Mat4 projectionInv;
        Mat4 viewProjectionInv;

        Vec3 cameraPosition;
        f32 time;

    };

    class Renderer {
    public:
        void Init( struct SDL_Window *window );
        void Cleanup( );

        // This will setup the current frame.
        // Will wait for the frame rendering fence, free its arena/resources, acquire the next image
        // in the swapchain and prepare the command buffer to record commands. In a multi in-flight frame
        // setup, this won't wait for the last frame to finish rendering, but instead, it will wait for
        // the desired next frame to finish its previous work. In most cases, this wait time is close to 0,
        // since the CPU was doing a lot of work in parallel with the gpu, which is busy rendering the last frame.
        // Returns the index of the swapchain image.
        void StartFrame( ) noexcept;

        // Will queue up the work agreggated in the frame command buffer. This function is non blocking,
        // as the only thing it does is queue up the work to the right graphics queue.
        // It will also transition the swapchain image to its expected layout to be presented in the future.
        void EndFrame( ) noexcept;

        // Will present the swapchain image to the screen.
        void Present( ) noexcept;

        u32 swapchainImageIndex = -1;

        static constexpr VkFormat DepthFormat = VK_FORMAT_D32_SFLOAT;
        static constexpr u8 MaxColorRenderTargets = 8;

    private:
        void GBufferPass( );
    };
} // namespace TL