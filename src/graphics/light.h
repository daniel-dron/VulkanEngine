#pragma once

#include <vk_types.h>
#include "../math/transform.h"

struct PointLight {
    Transform3D transform;
    vec4 color; // w is power in W's
    float diffuse = 1.0f;
    float specular = 1.0f;
    float radius = 10.0f;
};