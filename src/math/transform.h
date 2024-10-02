#pragma once

#include <vk_types.h>

class Transform {
public:
	mat4 asMatrix( ) const;

	void drawDebug( const std::string& );

	void DrawGizmo( );

	vec3 position = { 0.0f, 0.0f, 0.0f };
	vec3 euler = { 0.0f, 0.0f, 0.0f };
	vec3 scale = { 1.0f, 1.0f, 1.0f };

	mat4 model = glm::identity<mat4>( );
};