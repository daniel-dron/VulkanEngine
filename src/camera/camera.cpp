#include "camera.h"

#include <imgui.h>

#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>
#include <algorithm>

#include "engine/input.h"
#include <glm/gtc/type_ptr.hpp>

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
		movement += camera->getFront( );
	}
	if ( EG_INPUT.is_key_down( EG_KEY::S ) ) {
		movement -= camera->getFront( );
	}
	if ( EG_INPUT.is_key_down( EG_KEY::A ) ) {
		movement -= camera->getRight( );
	}
	if ( EG_INPUT.is_key_down( EG_KEY::D ) ) {
		movement += camera->getRight( );
	}

	// Normalize movement vector if it's not zero
	if ( glm::length2( movement ) > 0.0f ) {
		movement = glm::normalize( movement );
	}

	// Apply movement
	glm::vec3 newPosition = camera->getPosition( ) + movement * move_speed * deltaTime;
	camera->setPosition( newPosition );

	auto rel = EG_INPUT.get_mouse_rel( );
	float delta_yaw = static_cast<float>(rel.first) * sensitivity;
	float delta_pitch = static_cast<float>(rel.second) * sensitivity;
	camera->rotate( delta_yaw, delta_pitch, 0.0f );
}

void FirstPersonFlyingController::draw_debug( ) {
	ImGui::DragFloat( "Sensitivity", &sensitivity, 0.01f );
	ImGui::DragFloat( "Move Speed", &move_speed, 0.01f );
}

Camera::Camera( const vec3& position, float yaw, float pitch, float width, float height ) {
	setAspectRatio( width, height );
	updateVectors( );
	updateMatrices( );
}

void Camera::setAspectRatio( float width, float height ) {
	aspect_ratio = width / height;

	dirty_matrices = true;
}

void Camera::rotate( float delta_yaw, float delta_pitch, float delta_roll ) {
	yaw += delta_yaw;
	yaw = std::fmod( yaw, 360.0f );

	roll += delta_roll;

	pitch += delta_pitch;
	pitch = std::clamp( pitch, min_pitch, max_pitch );

	dirty_matrices = true;

	updateVectors( );
}

const vec3& Camera::getFront( ) const {
	return front;
}

const vec3& Camera::getRight( ) const {
	return right;
}

const vec3& Camera::getPosition( ) const {
	return position;
}

void Camera::setPosition( const vec3& new_position ) {
	position = new_position;
}

const mat4& Camera::getViewMatrix( ) {
	if ( dirty_matrices ) {
		updateMatrices( );
	}

	return view_matrix;
}

const mat4& Camera::getProjectionMatrix( ) {
	if ( dirty_matrices ) {
		updateMatrices( );
	}

	return projection_matrix;
}

void Camera::updateVectors( ) {
	front.x = cos( glm::radians( yaw ) ) * cos( glm::radians( pitch ) );
	front.y = sin( glm::radians( pitch ) );
	front.z = sin( glm::radians( yaw ) ) * cos( glm::radians( pitch ) );
	front = glm::normalize( front );

	right = glm::normalize( glm::cross( front, world_up ) );
	up = glm::normalize( glm::cross( right, front ) );

	if ( roll != 0.0f ) {
		glm::mat4 roll_matrix = glm::rotate( glm::mat4( 1.0f ), glm::radians( roll ), front );
		right = glm::vec3( roll_matrix * glm::vec4( right, 0.0f ) );
		up = glm::vec3( roll_matrix * glm::vec4( up, 0.0f ) );
	}

	dirty_matrices = true;
}

void Camera::updateMatrices( ) {
	view_matrix = glm::lookAt( position, position + front, up );
	projection_matrix = glm::perspective( glm::radians( fov ), aspect_ratio, near_plane, far_plane );

	dirty_matrices = false;
}

void Camera::drawDebug( ) {
	bool value_changed = false;

	ImGui::Indent( );
	ImGuiTreeNodeFlags child_flags = ImGuiTreeNodeFlags_DefaultOpen;
	float original_indent = ImGui::GetStyle( ).IndentSpacing;
	ImGui::GetStyle( ).IndentSpacing = 10.0f;

	// Position
	if ( ImGui::CollapsingHeader( "Position", child_flags) ) {
		value_changed |= ImGui::InputFloat3( "Position", glm::value_ptr( position ) );
	}

	// Orientation vectors
	if ( ImGui::CollapsingHeader( "Orientation Vectors", child_flags ) ) {
		value_changed |= ImGui::InputFloat3( "Front", glm::value_ptr( front ) );
		value_changed |= ImGui::InputFloat3( "Right", glm::value_ptr( right ) );
		value_changed |= ImGui::InputFloat3( "Up", glm::value_ptr( up ) );
		value_changed |= ImGui::InputFloat3( "World Up", glm::value_ptr( world_up ) );
	}

	// Rotation angles
	if ( ImGui::CollapsingHeader( "Rotation Angles", child_flags ) ) {
		value_changed |= ImGui::SliderFloat( "Yaw", &yaw, 0.0f, 360.0f );
		value_changed |= ImGui::SliderFloat( "Pitch", &pitch, min_pitch, max_pitch );
		value_changed |= ImGui::SliderFloat( "Roll", &roll, -180.0f, 180.0f );
	}

	// FOV
	if ( ImGui::CollapsingHeader( "Field of View", child_flags ) ) {
		value_changed |= ImGui::SliderFloat( "FOV", &fov, min_fov, max_fov );
		if ( ImGui::InputFloat( "Min FOV", &min_fov ) || ImGui::InputFloat( "Max FOV", &max_fov ) ) {
			fov = glm::clamp( fov, min_fov, max_fov );
			value_changed = true;
		}
	}

	// Other parameters
	if ( ImGui::CollapsingHeader( "Other Parameters", child_flags ) ) {
		value_changed |= ImGui::InputFloat( "Aspect Ratio", &aspect_ratio );
		value_changed |= ImGui::InputFloat( "Near Plane", &near_plane, 0.001f, 0.1f );
		value_changed |= ImGui::InputFloat( "Far Plane", &far_plane, 1.0f, 100.0f );
	}

	// Matrices
	if ( ImGui::CollapsingHeader( "Matrices", child_flags ) ) {
		ImGui::Text( "View Matrix" );
		for ( int i = 0; i < 4; i++ ) {
			ImGui::InputFloat4( ("##View" + std::to_string( i )).c_str( ), glm::value_ptr( view_matrix[i] ) );
		}

		ImGui::Text( "Projection Matrix" );
		for ( int i = 0; i < 4; i++ ) {
			ImGui::InputFloat4( ("##Proj" + std::to_string( i )).c_str( ), glm::value_ptr( projection_matrix[i] ) );
		}
	}

	// Set dirty flag if any relevant value changed
	if ( value_changed ) {
		updateVectors( );
	}

	// Dirty flag display
	ImGui::Checkbox( "Dirty Matrices", &dirty_matrices );

	ImGui::Unindent( );
}
