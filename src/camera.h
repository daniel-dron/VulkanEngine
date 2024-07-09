
#include "SDL_events.h"
#include <vk_types.h>

class Camera {
public:
  glm::vec3 velocity;
  glm::vec3 position;

  // vertical rotation
  float pitch{0.0f};
  // horizontal rotation
  float yaw{0.0f};

  glm::mat4 getViewMatrix();
  glm::mat4 getRotationMatrix();

  void processSDLEvent(SDL_Event &e);

  void update();
};
