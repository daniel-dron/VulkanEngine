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

#include <vk_types.h>

struct Node;

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

    Node *node;

    void DrawDebug( );
};

struct DirectionalLight {
    Node *node;
    HSV hsv;
    float power;

    // Shadow map
    ImageId shadowMap;
    float distance = 20.0f;
    float right = 20.0f;
    float up = 20.0f;
    float nearPlane = 0.1f;
    float farPlane = 30.0f;
};
