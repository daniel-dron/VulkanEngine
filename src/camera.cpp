#include "camera.h"
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include "engine/input.h"

void Camera::update() {
  if (EG_INPUT.is_key_down(EG_KEY::W)) {
    velocity.z = -1;
  }
  if (EG_INPUT.is_key_down(EG_KEY::S)) {
    velocity.z = 1;
  }
  if (EG_INPUT.is_key_down(EG_KEY::A)) {
    velocity.x = -1;
  }
  if (EG_INPUT.is_key_down(EG_KEY::D)) {
    velocity.x = 1;
  }

  if (EG_INPUT.is_key_up(EG_KEY::W)) {
    velocity.z = 0;
  }
  if (EG_INPUT.is_key_up(EG_KEY::S)) {
    velocity.z = 0;
  }
  if (EG_INPUT.is_key_up(EG_KEY::A)) {
    velocity.x = 0;
  }
  if (EG_INPUT.is_key_up(EG_KEY::D)) {
    velocity.x = 0;
  }

  auto mrel = EG_INPUT.get_mouse_rel();
  yaw += static_cast<float>(mrel.first) / 200.0f;
  pitch -= static_cast<float>(mrel.second) / 200.0f;

  glm::mat4 rotation = getRotationMatrix();
  position += glm::vec3(rotation * glm::vec4(velocity * 0.1f, 0.0f));
}

void Camera::processSDLEvent(SDL_Event &e) {
  if (e.type == SDL_KEYDOWN) {
    if (e.key.keysym.sym == SDLK_w) {
      velocity.z = -1;
    }
    if (e.key.keysym.sym == SDLK_s) {
      velocity.z = 1;
    }
    if (e.key.keysym.sym == SDLK_a) {
      velocity.x = -1;
    }
    if (e.key.keysym.sym == SDLK_d) {
      velocity.x = 1;
    }
  }

  if (e.type == SDL_KEYUP) {
    if (e.key.keysym.sym == SDLK_w) {
      velocity.z = 0;
    }
    if (e.key.keysym.sym == SDLK_s) {
      velocity.z = 0;
    }
    if (e.key.keysym.sym == SDLK_a) {
      velocity.x = 0;
    }
    if (e.key.keysym.sym == SDLK_d) {
      velocity.x = 0;
    }
  }

  if (e.type == SDL_MOUSEMOTION) {
    yaw += (float)e.motion.xrel / 200.f;
    pitch -= (float)e.motion.yrel / 200.f;
  }
}

glm::mat4 Camera::getViewMatrix() {
  glm::mat4 cameraTranslation = glm::translate(glm::mat4(1.f), position);
  glm::mat4 cameraRotation = getRotationMatrix();
  return glm::inverse(cameraTranslation * cameraRotation);
}

glm::mat4 Camera::getRotationMatrix() {
  glm::quat pitchRotation = glm::angleAxis(pitch, glm::vec3{1.f, 0.f, 0.f});
  glm::quat yawRotation = glm::angleAxis(yaw, glm::vec3{0.f, -1.f, 0.f});

  return glm::toMat4(yawRotation) * glm::toMat4(pitchRotation);
}


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