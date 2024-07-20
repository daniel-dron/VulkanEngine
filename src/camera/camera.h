#include <vk_types.h>

#include "SDL_events.h"


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

class Camera3D {
 private:
  glm::vec3 euler{};

 public:
  Transform3D transform;

 public:
  Camera3D() {}

  glm::mat4 get_view_matrix();
  void look_at(const glm::vec3& target);
  void rotate_by(const glm::vec3& rot);

  void draw_debug();
};

class CameraController {
 public:
  CameraController(Camera3D* cam) : camera(cam) {}

  virtual void update(float delta) = 0;
  virtual void draw_debug() = 0;

 protected:
  Camera3D* camera;
};

class FirstPersonFlyingController : public CameraController {
 public:
  FirstPersonFlyingController(Camera3D* cam, float sens = 0.1f,
                              float speed = 5.0f)
      : CameraController(cam), sensitivity(sens), move_speed(speed) {}

  void update(float deltaTime) override;
  void draw_debug() override;

 private:
  float sensitivity;
  float move_speed;
};
