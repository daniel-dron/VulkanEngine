#pragma once

#include <vk_types.h>

struct GBuffer {
	ImageID albedo;
	ImageID normal;
	ImageID position;
	ImageID pbr;
};