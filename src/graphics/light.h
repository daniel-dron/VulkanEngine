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
};