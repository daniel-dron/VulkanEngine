
#include "SDL_events.h"
#include <vk_types.h>

class Camera3D
{
public:
private:
  glm::vec3 position;
  glm::quat orientation;
  glm::vec3 euler; // pitch, yaw, roll

public:
  Camera3D() : position(0.0f, 0.0f, 0.0f), orientation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f)),
               euler(0.0f, 0.0f, 0.0f) {}

  void rotate(float deltaYaw, float deltaPitch, float deltaRoll);
  void rotateAroundPoint(const glm::vec3 &center, float deltaYaw, float deltaPitch);
  glm::mat4 getViewMatrix();
  glm::vec3 getEulerAngles();
  glm::vec3 getForward() const;
  glm::vec3 getRight() const;
  void setPosition(const glm::vec3 &newPosition);
  glm::vec3 getPosition() const;
  void translate(const glm::vec3 &translation);
  void setOrientation(const glm::quat &newOrientation);

  void draw_debug();

private:
  void updateQuaternions();
};

class CameraController
{
public:
  CameraController(Camera3D *cam) : camera(cam) {}

  virtual void update(float delta) = 0;
  virtual void draw_debug() = 0;

protected:
  Camera3D *camera;
};

class FirstPersonFlyingController : public CameraController
{
public:
  FirstPersonFlyingController(Camera3D *cam, float sens = 0.1f, float speed = 5.0f) : CameraController(cam), sensitivity(sens), move_speed(speed) {}

  virtual void update(float deltaTime) override;
  virtual void draw_debug() override;

private:
  float sensitivity;
  float move_speed;
};