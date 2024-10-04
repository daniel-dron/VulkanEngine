#pragma once

#include <vk_types.h>
#include "../math/transform.h"

struct PointLight {
	Transform transform;
	vec4 color;
	float diffuse = 1.0f;
	float specular = 1.0f;
	float radius = 10.0f;
};

struct DirectionalLight {
	Transform transform;
	vec4 color;

	// shadowmap
	ImageID shadow_map;
	float distance = 20.0f;
	float right = 20.0f;
	float up = 20.0f;
	float near_plane = 0.1f;
	float far_plane = 30.0f;
};