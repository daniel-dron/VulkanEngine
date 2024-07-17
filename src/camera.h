
#include "SDL_events.h"
#include <vk_types.h>

class Camera3D
{
public:
private:
  glm::vec3 position;
  glm::quat orientation;
  float yaw, pitch, roll;

public:
  Camera3D() : position(0.0f, 0.0f, 0.0f), orientation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f)),
               yaw(0.0f), pitch(0.0f), roll(0.0f) {}

  void rotate(float deltaYaw, float deltaPitch, float deltaRoll);
  glm::mat4 getViewMatrix();
  glm::vec3 getEulerAngles();
  glm::vec3 getForward() const;
  glm::vec3 getRight() const;
  void setPosition(const glm::vec3 &newPosition);
  glm::vec3 getPosition() const;
  void translate(const glm::vec3 &translation);
  void setOrientation(const glm::quat &newOrientation);
};

class FirstPersonFlyingController
{
public:
  FirstPersonFlyingController(Camera3D* cam, float sens = 0.1f, float speed = 5.0f) : camera(cam), sensitivity(sens), move_speed(speed) {}

  void update(float deltaTime);

private:
  Camera3D* camera;
  float sensitivity;
  float move_speed;
};