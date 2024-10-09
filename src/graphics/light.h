#pragma once

#include <vk_types.h>
#include "../math/transform.h"
#include <engine/scene.h>

struct HSV {
	float hue;
	float saturation;
	float value;
};

struct PointLight {
	HSV hsv;
	float power;
	float constant;
	float linear;
	float quadratic;

	std::shared_ptr<Scene::Node> node;

	void DrawDebug();
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