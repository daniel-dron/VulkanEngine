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

#include <graphics/swapchain.h>

namespace TL {
    // This will setup the current frame.
    // Will wait for the frame rendering fence, free its arena/resources, acquire the next image
    // in the swapchain and prepare the command buffer to record commands. In a multi in-flight frame
    // setup, this won't wait for the last frame to finish rendering, but instead, it will wait for
    // the desired next frame to finish its previous work. In most cases, this wait time is close to 0,
    // since the CPU was doing a lot of work in parallel with the gpu, which is busy rendering the last frame.
    // Returns the index of the swapchain image.
    u32 StartFrame( const TL_FrameData &frame ) noexcept;

    // Will queue up the work agreggated in the frame command buffer. This function is non blocking,
    // as the only thing it does is queue up the work to the right graphics queue.
    // It will also transition the swapchain image to its expected layout to be presented in the future.
    void EndFrame( const TL_FrameData &frame, u32 swapchain_image_index ) noexcept;

    // Will present the swapchain image to the screen.
    void Present( const TL_FrameData &frame, u32 swapchain_image_index ) noexcept;
} // namespace TL