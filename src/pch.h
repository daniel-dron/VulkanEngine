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

#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <deque>
#include <queue>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <expected>
#include <random>
#include <format>
#include <Windows.h>

//Third Party headers

// fmt
#include <fmt/core.h>

// Assimp
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/texture.h>

// STB
#include <stb_image.h>

// Meshopt
#include <meshoptimizer.h>

// Vulkan
#include <VkBootstrap.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include <vk_mem_alloc.h>

// SDL
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <SDL.h>
#include <SDL_events.h>
#include <SDL_stdinc.h>
#include <SDL_video.h>

// Imgui
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>

// GLM
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/integer.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/packing.hpp>

// Tracy
#include <tracy/Tracy.hpp>

// ImGuizmo
#include <imguizmo/GraphEditor.h>
#include <imguizmo/ImCurveEdit.h>
#include <imguizmo/ImGradient.h>
#include <imguizmo/ImGuizmo.h>
#include <imguizmo/ImSequencer.h>
#include <imguizmo/ImZoomSlider.h>

#include <vk_engine.h>
