
file(GLOB SRC_SRC
  main.cpp
  vk_types.h
  vk_initializers.cpp
  vk_initializers.h
  vk_pipelines.h
  vk_pipelines.cpp
  vk_engine.h
  vk_engine.cpp
)

file(GLOB SRC_GRAPHICS
  graphics/light.h
  graphics/light.cpp
  graphics/gfx_device.h
  graphics/gfx_device.cpp
  graphics/gpu_image.h
  graphics/gpu_image.cpp
  graphics/descriptors.h
  graphics/descriptors.cpp
  graphics/image_codex.h
  graphics/image_codex.cpp
  graphics/mesh_codex.h
  graphics/mesh_codex.cpp
  graphics/material_codex.h
  graphics/material_codex.cpp
  graphics/swapchain.h
  graphics/swapchain.cpp
  graphics/draw_command.h
  graphics/draw_command.cpp
  graphics/bindless.h
  graphics/bindless.cpp
  graphics/gbuffer.h
  graphics/ibl.h
  graphics/ibl.cpp
)

file (GLOB SRC_PIPELINES
  graphics/pipelines/pipeline.h
  graphics/pipelines/pbr_pipeline.h
  graphics/pipelines/pbr_pipeline.cpp
  graphics/pipelines/wireframe_pipeline.h
  graphics/pipelines/wireframe_pipeline.cpp
  graphics/pipelines/gbuffer_pipeline.h
  graphics/pipelines/gbuffer_pipeline.cpp
  graphics/pipelines/imgui_pipeline.h
  graphics/pipelines/imgui_pipeline.cpp
  graphics/pipelines/skybox_pipeline.h
  graphics/pipelines/skybox_pipeline.cpp
  graphics/pipelines/ibl_pipeline.h
  graphics/pipelines/ibl_pipeline.cpp
  graphics/pipelines/compute_pipeline.h
  graphics/pipelines/compute_pipeline.cpp
)

file(GLOB SRC_CAMERA
  camera/camera.cpp
  camera/camera.h
)

file(GLOB SRC_MATH
  math/transform.h
  math/transform.cpp
)

file(GLOB SRC_ENGINE
  engine/input.h
  engine/input.cpp
  engine/loader.h
  engine/loader.cpp
  engine/scene.h
  engine/scene.cpp
)

file(GLOB SRC_IMGUIZMO
  imguizmo/GraphEditor.cpp
  imguizmo/GraphEditor.h
  imguizmo/ImCurveEdit.cpp
  imguizmo/ImCurveEdit.h
  imguizmo/ImGradient.cpp
  imguizmo/ImGradient.h
  imguizmo/ImGuizmo.cpp
  imguizmo/ImGuizmo.h
  imguizmo/ImSequencer.cpp
  imguizmo/ImSequencer.h
  imguizmo/GraphEditor.h
)

file(GLOB SRC_UTILS
  utils/workers.cpp
  utils/workers.h
)

# Add source to this project's executable.
add_executable (engine 
	${SRC_SRC}
	${SRC_PIPELINES}
	${SRC_GRAPHICS}
	${SRC_CAMERA}
	${SRC_MATH}
	${SRC_ENGINE}
	${SRC_IMGUIZMO}
  ${SRC_UTILS}
)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(TARGET engine PROPERTY CXX_STANDARD 23)

source_group("graphics" FILES ${SRC_GRAPHICS})
source_group("graphics/pipelines" FILES ${SRC_PIPELINES})
source_group("src" FILES ${SRC_SRC})
source_group("camera" FILES ${SRC_CAMERA})
source_group("math" FILES ${SRC_MATH})
source_group("engine" FILES ${SRC_ENGINE})
source_group("imguizmo" FILES ${SRC_IMGUIZMO})
source_group("utils" FILES ${SRC_UTILS})

target_compile_definitions(engine PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE)
target_compile_definitions(engine PUBLIC GLM_FORCE_RIGHT_HANDED)
target_include_directories(engine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(engine PUBLIC vma glm Vulkan::Vulkan fmt::fmt stb_image SDL2::SDL2 vkbootstrap imgui assimp)

target_precompile_headers(engine PUBLIC <optional> <vector> <memory> <string> <vector> <unordered_map> <glm/mat4x4.hpp>  <glm/vec4.hpp> <vulkan/vulkan.h>)