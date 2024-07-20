#include <vk_types.h>

#include "../math/transform.h"

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
