#include "transform.h"

void Transform3D::set_position(const vec3& pos) { position = pos; }
void Transform3D::set_heading(const quat& h) { heading = h; }
void Transform3D::set_scale(const vec3& s) { scale = s; }

const vec3& Transform3D::get_position() { return position; }
const quat& Transform3D::get_heading() { return heading; }
const vec3& Transform3D::get_scale() { return scale; }

vec3 Transform3D::get_local_up() { return heading * GlobalUp; }
vec3 Transform3D::get_local_right() { return heading * GlobalRight; }
vec3 Transform3D::get_local_front() { return heading * GlobalFront; }