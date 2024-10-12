#pragma once

#include <vk_types.h>

class Transform {
public:
    [[nodiscard]] Mat4 AsMatrix( ) const;

	void DrawDebug( const std::string& );

	Vec3 position = { 0.0f, 0.0f, 0.0f };
	Vec3 euler = { 0.0f, 0.0f, 0.0f };
    Vec3 scale = { 1.0f, 1.0f, 1.0f };

	Mat4 model = glm::identity<Mat4>( );
};