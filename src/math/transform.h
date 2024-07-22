#pragma once

#include <vk_types.h>

class Transform3D {
 public:
  const glm::vec3& get_position() const;
  const glm::quat& get_heading() const;
  const glm::vec3& get_scale() const;

  void set_position(const glm::vec3& pos);
  void set_heading(const glm::quat& h);
  void set_scale(const glm::vec3& s);

  const glm::vec3& get_position();
  const glm::quat& get_heading();
  const glm::vec3& get_scale();

  glm::vec3 get_local_up();
  glm::vec3 get_local_right();
  glm::vec3 get_local_front();

 private:
  glm::vec3 position{};
  glm::quat heading = glm::identity<glm::quat>();
  glm::vec3 scale{1.0f};

  mutable glm::mat4 matrix = glm::identity<glm::mat4>();
  mutable bool is_dirty = false;
};