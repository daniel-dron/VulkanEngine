#include "camera.h"
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include "engine/input.h"
#include <imgui.h>

void Camera3D::rotate(float deltaYaw, float deltaPitch, float deltaRoll)
{
  // Update Euler angles
  euler.y += deltaYaw;
  euler.x += deltaPitch;
  euler.z += deltaRoll;

  updateQuaternions();
}

void Camera3D::rotateAroundPoint(const glm::vec3 &center, float deltaYaw, float deltaPitch)
{
  // Translate to origin
  auto orig_position = position;
  position = center;

  // Apply rotation
  glm::quat qYaw = glm::angleAxis(glm::radians(deltaYaw), glm::vec3(0, 1, 0));
  glm::quat qPitch = glm::angleAxis(glm::radians(deltaPitch), glm::vec3(1, 0, 0));
  glm::quat rotation = qYaw * qPitch;

  position = rotation * position;
  orientation = rotation * orientation;

  // Translate back to center
  position = orig_position;

  // Update Euler angles
  glm::vec3 eulerAngles = glm::eulerAngles(orientation);
  euler.x = glm::degrees(eulerAngles.y);
  euler.y = glm::degrees(eulerAngles.x);
  euler.z = glm::degrees(eulerAngles.z);
}

void Camera3D::updateQuaternions()
{
  // Clamp pitch to avoid gimbal lock
  euler.x = glm::clamp(euler.x, -89.0f, 89.0f);

  // Convert Euler angles to quaternion
  glm::quat qYaw = glm::angleAxis(glm::radians(euler.y), glm::vec3(0, 1, 0));
  glm::quat qPitch = glm::angleAxis(glm::radians(euler.x), glm::vec3(1, 0, 0));
  glm::quat qRoll = glm::angleAxis(glm::radians(euler.z), glm::vec3(0, 0, 1));

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
  return euler;
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
  euler.x = glm::degrees(eulerAngles.x);
  euler.y = glm::degrees(eulerAngles.y);
  euler.z = glm::degrees(eulerAngles.z);
}

void Camera3D::draw_debug()
{
  ImGui::DragFloat3("Position", &position.x, 0.01f, -1000.0f, 1000.0f);
  if (ImGui::DragFloat3("Rotation", &euler.x, 0.01f))
  {
    updateQuaternions();
  }
}

void FirstPersonFlyingController::update(float deltaTime)
{
  if (EG_INPUT.is_key_up(EG_KEY::MOUSE_RIGHT))
  {
    return;
  }

  move_speed += EG_INPUT.get_mouse_wheel() * 10.0f;
  if (move_speed <= 0.1f)
  {
    move_speed = 0.1f;
  }

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

void FirstPersonFlyingController::draw_debug()
{
  ImGui::DragFloat("Sensitivity", &sensitivity, 0.01f);
  ImGui::DragFloat("Move Speed", &move_speed, 0.01f);
}