#pragma once

#include <vk_types.h>

#include "../math/transform.h"

class Camera {
public:
	Camera( const vec3& position, float yaw, float pitch, float width, float height );

	void setAspectRatio( float width, float height );
	void rotate( float delta_yaw, float delta_pitch, float delta_roll = 0.0f );

	const vec3& getFront( ) const;
	const vec3& getRight( ) const;
	const vec3& getPosition( ) const;
	void setPosition( const vec3& new_position );

	const mat4& getViewMatrix( );
	const mat4& getProjectionMatrix( );

	void drawDebug( );
private:
	void updateVectors( );
	void updateMatrices( );

	vec3 position = { 0.0f, 0.0f, 0.0f };
	vec3 front = GlobalFront;
	vec3 right = GlobalRight;
	vec3 up = GlobalUp;
	vec3 world_up = GlobalUp;

	mat4 view_matrix = glm::mat4( 1.0f );
	mat4 projection_matrix = glm::mat4( 1.0f );

	float yaw = 0.0f;
	float roll = 0.0f;
	float pitch = 0.0f;

	float min_pitch = -89.0f;
	float max_pitch = 89.0f;

	float fov = 90.0f;
	float max_fov = 130.0f;
	float min_fov = 20.0f;

	float aspect_ratio = 0.0f;

	float near_plane = 0.001f;
	float far_plane = 1000.0f;

	bool dirty_matrices = true;
};

class CameraController {
public:
	CameraController( Camera* cam ) : camera( cam ) {}

	virtual void update( float delta ) = 0;
	virtual void draw_debug( ) = 0;

protected:
	Camera* camera;
};

class FirstPersonFlyingController : public CameraController {
public:
	FirstPersonFlyingController( Camera* cam, float sens = 0.1f,
		float speed = 5.0f )
		: CameraController( cam ), sensitivity( sens ), move_speed( speed ) {}

	void update( float deltaTime ) override;
	void draw_debug( ) override;

private:
	float sensitivity;
	float move_speed;
};
