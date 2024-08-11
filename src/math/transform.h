#pragma once

#include <vk_types.h>

class Transform3D {
public:
	Transform3D( ) = default;
	Transform3D( const mat4& matrix );

	const vec3& get_position( ) const;
	const quat& get_heading( ) const;
	const vec3& get_scale( ) const;

	void set_position( const vec3& pos );
	void set_heading( const quat& h );
	void set_scale( const vec3& s );

	const vec3& get_position( );
	const quat& get_heading( );
	const vec3& get_scale( );

	vec3 get_local_up( );
	vec3 get_local_right( );
	vec3 get_local_front( );

	const glm::mat4& as_matrix( ) const;

	Transform3D operator*( const Transform3D& rhs ) const;

private:
	vec3 position{};
	quat heading = glm::identity<quat>( );
	vec3 scale{ 1.0f };

	mutable mat4 matrix = glm::identity<mat4>( );
	mutable bool is_dirty = false;
};