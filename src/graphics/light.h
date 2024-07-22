#pragma once

#include <vk_types.h>
#include "../math/transform.h"

struct PointLight {
    glm::vec4 color; // w is power in W's
    float diffuse = 1.0f;
    float specular = 1.0f;
    float radius = 10.0f;
    float _padding = 0.0f;
};