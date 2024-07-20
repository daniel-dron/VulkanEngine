#include <vk_types.h>
#include "../math/transform.h"

struct Light {
    glm::vec4 color; // w is power in W's
    float diffuse = 1.0f;
    float specular = 1.0f;
};