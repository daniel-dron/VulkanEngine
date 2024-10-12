#pragma once

#include <vk_types.h>

struct GBuffer {
	ImageId albedo;
	ImageId normal;
	ImageId position;
	ImageId pbr;
};