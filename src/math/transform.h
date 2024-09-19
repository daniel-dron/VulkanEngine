#pragma once

#include <vk_types.h>

class Transform3D {
public:
	Transform3D( ) = default;
	Transform3D( const mat4& matrix );

	const vec3& getPosition( ) const;
	const quat& getHeading( ) const;
	const vec3& getScale( ) const;

	void setPosition( const vec3& pos );
	void setHeading( const quat& h );
	void setScale( const vec3& s );

	vec3 getLocalUp( ) const;
	vec3 getLocalRight( ) const;
	vec3 getLocalFront( ) const;

	const glm::mat4& asMatrix( ) const;

	Transform3D operator*( const Transform3D& rhs ) const;
	void drawDebug( const std::string& label );

private:
	vec3 position{};
	quat heading = glm::identity<quat>( );
	vec3 scale{ 1.0f };

	mutable mat4 matrix = glm::identity<mat4>( );
	mutable bool is_dirty = false;
};

class Transform {
public:
	mat4 asMatrix( ) const;

	void drawDebug( const std::string& );

	vec3 position = { 0.0f, 0.0f, 0.0f };
	vec3 euler = { 0.0f, 0.0f, 0.0f };
	vec3 scale = { 1.0f, 1.0f, 1.0f };

	mat4 model = glm::identity<mat4>( );
};