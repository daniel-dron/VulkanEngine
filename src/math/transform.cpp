#include "transform.h"

void Transform3D::set_position(const glm::vec3& pos) { position = pos; }
void Transform3D::set_heading(const glm::quat& h) { heading = h; }
void Transform3D::set_scale(const glm::vec3& s) { scale = s; }

const glm::vec3& Transform3D::get_position() { return position; }
const glm::quat& Transform3D::get_heading() { return heading; }
const glm::vec3& Transform3D::get_scale() { return scale; }

glm::vec3 Transform3D::get_local_up() { return heading * GlobalUp; }
glm::vec3 Transform3D::get_local_right() { return heading * GlobalRight; }
glm::vec3 Transform3D::get_local_front() { return heading * GlobalFront; }