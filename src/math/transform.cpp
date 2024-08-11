#include "transform.h"

#include <glm/gtx/matrix_decompose.hpp>

Transform3D::Transform3D( const mat4& matrix ) {
	vec3 scale;
	quat rotation;
	vec3 translation;
	vec3 skew;
	vec4 perspective;
	glm::decompose( matrix, scale, rotation, translation, skew, perspective );

	this->position = translation;
	this->heading = rotation;
	this->scale = scale;
	this->is_dirty = true;
}

void Transform3D::set_position( const vec3& pos ) {
	position = pos;
	is_dirty = true;
}
void Transform3D::set_heading( const quat& h ) {
	heading = h;
	is_dirty = true;
}
void Transform3D::set_scale( const vec3& s ) {
	scale = s;
	is_dirty = true;
}

const vec3& Transform3D::get_position( ) { return position; }
const quat& Transform3D::get_heading( ) { return heading; }
const vec3& Transform3D::get_scale( ) { return scale; }

vec3 Transform3D::get_local_up( ) { return heading * GlobalUp; }
vec3 Transform3D::get_local_right( ) { return heading * GlobalRight; }
vec3 Transform3D::get_local_front( ) { return heading * GlobalFront; }

const glm::mat4& Transform3D::as_matrix( ) const {
	if ( !is_dirty ) {
		return matrix;
	}

	matrix = glm::translate( mat4( 1.0f ), position );
	matrix *= glm::mat4_cast( heading );
	matrix = glm::scale( matrix, scale );

	is_dirty = false;

	return matrix;
}

Transform3D Transform3D::operator*( const Transform3D& rhs ) const {
	return Transform3D( this->as_matrix( ) * rhs.as_matrix( ) );
}
