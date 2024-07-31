#include "camera.h"

#include <imgui.h>

#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>

#include "engine/input.h"

glm::mat4 Camera3D::get_view_matrix( ) {
	auto up = transform.get_local_up( );
	auto front = transform.get_position( ) + transform.get_local_front( );
	return glm::lookAtRH( transform.get_position( ), front, up );
}

void Camera3D::look_at( const glm::vec3& target ) {
	glm::vec3 direction = glm::normalize( target - transform.get_position( ) );

	// Calculate pitch and yaw from the direction vector
	float pitch = glm::asin( -direction.y );
	float yaw = atan2( direction.x, direction.z );

	// Convert radians to degrees
	euler.x = glm::degrees( pitch );
	euler.y = -1.0f * glm::degrees( yaw );

	// Create quaternion from euler angles
	glm::quat q_pitch = glm::angleAxis( pitch, glm::vec3( 1.0f, 0.0f, 0.0f ) );
	glm::quat q_yaw = glm::angleAxis( yaw, glm::vec3( 0.0f, 1.0f, 0.0f ) );

	// Set the new heading
	transform.set_heading( q_yaw * q_pitch );
}

void Camera3D::rotate_by( const glm::vec3& rot ) {
	euler += rot;

	auto q_yaw = glm::angleAxis( glm::radians( euler.y ), GlobalUp );
	auto q_pitch = glm::angleAxis( glm::radians( euler.x ), GlobalRight );
	auto q_roll = glm::angleAxis( glm::radians( euler.z ), GlobalFront );
	transform.set_heading( q_yaw * q_pitch * q_roll );
}

void Camera3D::draw_debug( ) {
	auto pos = transform.get_position( );
	if ( ImGui::DragFloat3( "Position", &pos.x, 0.01f, -1000.0f, 1000.0f ) ) {
		transform.set_position( pos );
	}

	if ( ImGui::DragFloat3( "Rotation", &euler.x, 0.01f ) ) {
		auto q_yaw = glm::angleAxis( glm::radians( euler.y ), GlobalUp );
		auto q_pitch = glm::angleAxis( glm::radians( euler.x ), GlobalRight );
		auto q_roll = glm::angleAxis( glm::radians( euler.z ), GlobalFront );
		transform.set_heading( q_yaw * q_pitch * q_roll );
	}

	auto q = transform.get_heading( );
	if ( ImGui::DragFloat4( "Quat", &q.x, 0.1f ) ) {
		transform.set_heading( q );
		euler = glm::degrees( glm::eulerAngles( q ) );
	}
}

void FirstPersonFlyingController::update( float deltaTime ) {
	if ( EG_INPUT.is_key_up( EG_KEY::MOUSE_RIGHT ) ) {
		return;
	}

	move_speed += EG_INPUT.get_mouse_wheel( ) * 10.0f;
	if ( move_speed <= 0.1f ) {
		move_speed = 0.1f;
	}

	glm::vec3 movement( 0.0f );

	if ( EG_INPUT.is_key_down( EG_KEY::W ) ) {
		movement += camera->transform.get_local_front( );
	}
	if ( EG_INPUT.is_key_down( EG_KEY::S ) ) {
		movement -= camera->transform.get_local_front( );
	}
	if ( EG_INPUT.is_key_down( EG_KEY::A ) ) {
		movement -= camera->transform.get_local_right( );
	}
	if ( EG_INPUT.is_key_down( EG_KEY::D ) ) {
		movement += camera->transform.get_local_right( );
	}

	// Normalize movement vector if it's not zero
	if ( glm::length2( movement ) > 0.0f ) {
		movement = glm::normalize( movement );
	}

	// Apply movement
	glm::vec3 newPosition =
		camera->transform.get_position( ) + movement * move_speed * deltaTime;
	camera->transform.set_position( newPosition );

	auto rel = EG_INPUT.get_mouse_rel( );
	float delta_yaw = -static_cast<float>(rel.first) * sensitivity;
	float delta_pitch = static_cast<float>(rel.second) * sensitivity;
	camera->rotate_by( { delta_pitch, delta_yaw, 0.0f } );
}

void FirstPersonFlyingController::draw_debug( ) {
	ImGui::DragFloat( "Sensitivity", &sensitivity, 0.01f );
	ImGui::DragFloat( "Move Speed", &move_speed, 0.01f );
}
