#pragma once

#include <SDL.h>

#include <array>
#include <cstdint>
#include <unordered_map>


// swap here the input implementation
#define EG_INPUT EngineInput::instance()

enum class EG_KEY : uint8_t {
  MOUSE_LEFT,
  MOUSE_RIGHT,
  MOUSE_MIDDLE,

  // NUMBERS
  N0,
  N1,
  N2,
  N3,
  N4,
  N5,
  N6,
  N7,
  N8,
  N9,

  // ASCI
  A,
  B,
  C,
  D,
  E,
  F,
  G,
  H,
  J,
  K,
  L,
  M,
  N,
  O,
  P,
  Q,
  R,
  S,
  T,
  U,
  V,
  W,
  X,
  Y,
  Z,

  SPACE,
  BACKSPACE,
  ENTER,
  TAB,
  L_SHIFT,
  L_CTRL,
  ESCAPE,
};

class VulkanEngine;

struct Key {
  bool is_down = false;
  bool just_pressed = false;
  bool just_released = false;
  uint64_t half_count = 0;
};

class Input {
 public:
  static Input &instance() {
    static Input input;
    return input;
  }

  void init();

  void poll_events(VulkanEngine* engine);

  virtual void process_sdl_event(SDL_Event &event, VulkanEngine* engine);

  bool is_key_down(EG_KEY key);
  bool is_key_up(EG_KEY key);
  bool was_key_pressed(EG_KEY key);
  bool was_key_released(EG_KEY key);
  std::pair<int32_t, int32_t> get_mouse_rel();
  std::pair<int32_t, int32_t> get_mouse_pos();
  float get_mouse_wheel();
  bool should_quit();

 private:
  constexpr static size_t KEY_COUNT = 255;
  std::array<Key, KEY_COUNT> keys;
  bool _should_quit = false;

  // mouse
  int32_t xrel;
  int32_t yrel;
  int32_t x;
  int32_t y;
  float mwheel;

  std::unordered_map<uint16_t, EG_KEY> translation_lut;
};

class EngineInput : public Input {
 public:
  static EngineInput &instance() {
    static EngineInput input;
    return input;
  }

  virtual void process_sdl_event(SDL_Event &event, VulkanEngine* engine) override;
};