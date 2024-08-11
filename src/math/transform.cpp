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
	matrix *= glm::mat4_cast( heading );
	matrix = glm::scale( matrix, scale );

	is_dirty = false;

	return matrix;
}

Transform3D Transform3D::operator*( const Transform3D& rhs ) const {
	return Transform3D( this->asMatrix( ) * rhs.asMatrix( ) );
}
