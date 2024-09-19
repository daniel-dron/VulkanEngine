#include "transform.h"

#include <glm/gtx/matrix_decompose.hpp>
#include <imgui.h>

Transform3D::Transform3D( const mat4& matrix ) {
	vec3 scale;
	quat rotation;
	vec3 translation;
	vec3 skew;
	vec4 perspective;
	glm::decompose( matrix, scale, rotation, translation, skew, perspective );

	this->position = translation;
	this->heading = glm::conjugate( rotation );
	this->scale = scale;
	this->is_dirty = true;
}

void Transform3D::setPosition( const vec3& pos ) {
	position = pos;
	is_dirty = true;
}
void Transform3D::setHeading( const quat& h ) {
	heading = h;
	is_dirty = true;
}
void Transform3D::setScale( const vec3& s ) {
	scale = s;
	is_dirty = true;
}

const vec3& Transform3D::getPosition( ) const { return position; }
const quat& Transform3D::getHeading( ) const { return heading; }
const vec3& Transform3D::getScale( ) const { return scale; }

vec3 Transform3D::getLocalUp( ) const { return heading * GlobalUp; }
vec3 Transform3D::getLocalRight( ) const { return heading * GlobalRight; }
vec3 Transform3D::getLocalFront( ) const { return heading * GlobalFront; }

const glm::mat4& Transform3D::asMatrix( ) const {
	if ( !is_dirty ) {
		return matrix;
	}

	matrix = glm::translate( mat4( 1.0f ), position );
	matrix *= glm::mat4_cast( glm::normalize( heading ) );
	matrix = glm::scale( matrix, scale );

	is_dirty = false;

	return matrix;
}

Transform3D Transform3D::operator*( const Transform3D& rhs ) const {
	return Transform3D( this->asMatrix( ) * rhs.asMatrix( ) );
}

void Transform3D::drawDebug( const std::string& label ) {
	ImGui::PushID( label.c_str( ) );
	ImGui::Text( "%s", label.c_str( ) );

	ImGui::Text( "Position" );
	if ( ImGui::DragFloat3( "##Position", &position[0], 0.1f ) ) {
		is_dirty = true;
	}

	ImGui::Text( "Rotation" );
	float headingEuler[3] = {
		glm::degrees( glm::eulerAngles( heading ).x ),
		glm::degrees( glm::eulerAngles( heading ).y ),
		glm::degrees( glm::eulerAngles( heading ).z )
	};
	if ( ImGui::DragFloat3( "##Rotation", headingEuler, 0.1f ) ) {
		heading = glm::quat( glm::radians( vec3( headingEuler[0], headingEuler[1], headingEuler[2] ) ) );
		is_dirty = true;
	}

	ImGui::Text( "Scale" );
	if ( ImGui::DragFloat3( "##Scale", &scale[0], 0.1f ) ) {
		is_dirty = true;
	}

	ImGui::PopID( );
}

mat4 Transform::asMatrix( ) const {
	const glm::mat4 rot_x = glm::rotate( glm::mat4( 1.0f ), glm::radians( euler.x ), glm::vec3( 1.0f, 0.0f, 0.0f ) );
	const glm::mat4 rot_y = glm::rotate( glm::mat4( 1.0f ), glm::radians( euler.y ), glm::vec3( 0.0f, 1.0f, 0.0f ) );
	const glm::mat4 rot_z = glm::rotate( glm::mat4( 1.0f ), glm::radians( euler.z ), glm::vec3( 0.0f, 0.0f, 1.0f ) );

	const glm::mat4 roationMatrix = rot_y * rot_x * rot_z;

	return glm::translate( glm::mat4( 1.0f ), position ) * roationMatrix * glm::scale( glm::mat4( 1.0f ), scale );
}

void Transform::drawDebug( const std::string& label ) {
	ImGui::PushID( label.c_str( ) );
	ImGui::Text( "%s", label.c_str( ) );

	ImGui::Text( "Position" );
	ImGui::DragFloat3( "##Position", &position[0], 0.1f );

	ImGui::Text( "Rotation" );
	ImGui::DragFloat3( "##Rotation", &euler[0], 0.1f );

	ImGui::Text( "Scale" );
	ImGui::DragFloat3( "##Scale", &scale[0], 0.1f );

	ImGui::PopID( );
}
