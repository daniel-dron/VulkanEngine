#include "camera.h"
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include "engine/input.h"

void Camera3D::rotate(float deltaYaw, float deltaPitch, float deltaRoll)
{
  // Update Euler angles
  yaw += deltaYaw;
  pitch += deltaPitch;
  roll += deltaRoll;

  // Clamp pitch to avoid gimbal lock
  pitch = glm::clamp(pitch, -89.0f, 89.0f);

  // Convert Euler angles to quaternion
  glm::quat qYaw = glm::angleAxis(glm::radians(yaw), glm::vec3(0, 1, 0));
  glm::quat qPitch = glm::angleAxis(glm::radians(pitch), glm::vec3(1, 0, 0));
  glm::quat qRoll = glm::angleAxis(glm::radians(roll), glm::vec3(0, 0, 1));

  // Combine rotations
  orientation = qYaw * qPitch * qRoll;
}

glm::mat4 Camera3D::getViewMatrix()
{
  glm::mat4 view = glm::mat4_cast(glm::inverse(orientation));
  return glm::translate(view, -position);
}

glm::vec3 Camera3D::getEulerAngles()
{
  return glm::vec3(pitch, yaw, roll);
}

glm::vec3 Camera3D::getForward() const
{
  return glm::rotate(orientation, glm::vec3(0, 0, -1));
}

glm::vec3 Camera3D::getRight() const
{
  return glm::rotate(orientation, glm::vec3(1, 0, 0));
}

void Camera3D::setPosition(const glm::vec3 &newPosition)
{
  position = newPosition;
}

glm::vec3 Camera3D::getPosition() const
{
  return position;
}

void Camera3D::translate(const glm::vec3 &translation)
{
  position += translation;
}

void Camera3D::setOrientation(const glm::quat &newOrientation)
{
  orientation = newOrientation;

  // Extract Euler angles from the quaternion
  glm::vec3 eulerAngles = glm::eulerAngles(orientation);
  pitch = glm::degrees(eulerAngles.x);
  yaw = glm::degrees(eulerAngles.y);
  roll = glm::degrees(eulerAngles.z);
}

void FirstPersonFlyingController::update(float deltaTime)
{
  glm::vec3 movement(0.0f);

  if (EG_INPUT.is_key_down(EG_KEY::W))
  {
    movement += camera->getForward();
  }
  if (EG_INPUT.is_key_down(EG_KEY::S))
  {
    movement -= camera->getForward();
  }
  if (EG_INPUT.is_key_down(EG_KEY::A))
  {
    movement -= camera->getRight();
  }
  if (EG_INPUT.is_key_down(EG_KEY::D))
  {
    movement += camera->getRight();
  }

  // Normalize movement vector if it's not zero
  if (glm::length2(movement) > 0.0f)
  {
    movement = glm::normalize(movement);
  }

  // Apply movement
  glm::vec3 newPosition = camera->getPosition() + movement * move_speed * deltaTime;
  camera->setPosition(newPosition);

  auto rel = EG_INPUT.get_mouse_rel();
  float delta_yaw = -static_cast<float>(rel.first) * sensitivity;
  float delta_pitch = -static_cast<float>(rel.second) * sensitivity;
  camera->rotate(delta_yaw, delta_pitch, 0.0f);
}