#pragma once

#include "input.h"

#include <fmt/core.h>
#include <imgui_impl_sdl2.h>

#include <algorithm>
#include <tracy/tracy/Tracy.hpp>

#include "../vk_engine.h"

void Input::init() {
  translation_lut[SDL_BUTTON_LEFT] = EG_KEY::MOUSE_LEFT;
  translation_lut[SDL_BUTTON_RIGHT] = EG_KEY::MOUSE_RIGHT;
  translation_lut[SDL_BUTTON_MIDDLE] = EG_KEY::MOUSE_MIDDLE;

  // Numbers
  translation_lut[SDL_SCANCODE_0] = EG_KEY::N0;
  translation_lut[SDL_SCANCODE_1] = EG_KEY::N1;
  translation_lut[SDL_SCANCODE_2] = EG_KEY::N2;
  translation_lut[SDL_SCANCODE_3] = EG_KEY::N3;
  translation_lut[SDL_SCANCODE_4] = EG_KEY::N4;
  translation_lut[SDL_SCANCODE_5] = EG_KEY::N5;
  translation_lut[SDL_SCANCODE_6] = EG_KEY::N6;
  translation_lut[SDL_SCANCODE_7] = EG_KEY::N7;
  translation_lut[SDL_SCANCODE_8] = EG_KEY::N8;
  translation_lut[SDL_SCANCODE_9] = EG_KEY::N9;

  // Letters
  translation_lut[SDL_SCANCODE_A] = EG_KEY::A;
  translation_lut[SDL_SCANCODE_B] = EG_KEY::B;
  translation_lut[SDL_SCANCODE_C] = EG_KEY::C;
  translation_lut[SDL_SCANCODE_D] = EG_KEY::D;
  translation_lut[SDL_SCANCODE_E] = EG_KEY::E;
  translation_lut[SDL_SCANCODE_F] = EG_KEY::F;
  translation_lut[SDL_SCANCODE_G] = EG_KEY::G;
  translation_lut[SDL_SCANCODE_H] = EG_KEY::H;
  translation_lut[SDL_SCANCODE_J] = EG_KEY::J;
  translation_lut[SDL_SCANCODE_K] = EG_KEY::K;
  translation_lut[SDL_SCANCODE_L] = EG_KEY::L;
  translation_lut[SDL_SCANCODE_M] = EG_KEY::M;
  translation_lut[SDL_SCANCODE_N] = EG_KEY::N;
  translation_lut[SDL_SCANCODE_O] = EG_KEY::O;
  translation_lut[SDL_SCANCODE_P] = EG_KEY::P;
  translation_lut[SDL_SCANCODE_Q] = EG_KEY::Q;
  translation_lut[SDL_SCANCODE_R] = EG_KEY::R;
  translation_lut[SDL_SCANCODE_S] = EG_KEY::S;
  translation_lut[SDL_SCANCODE_T] = EG_KEY::T;
  translation_lut[SDL_SCANCODE_U] = EG_KEY::U;
  translation_lut[SDL_SCANCODE_V] = EG_KEY::V;
  translation_lut[SDL_SCANCODE_W] = EG_KEY::W;
  translation_lut[SDL_SCANCODE_X] = EG_KEY::X;
  translation_lut[SDL_SCANCODE_Y] = EG_KEY::Y;
  translation_lut[SDL_SCANCODE_Z] = EG_KEY::Z;

  // Special keys
  translation_lut[SDL_SCANCODE_SPACE] = EG_KEY::SPACE;
  translation_lut[SDL_SCANCODE_BACKSPACE] = EG_KEY::BACKSPACE;
  translation_lut[SDL_SCANCODE_RETURN] = EG_KEY::ENTER;
  translation_lut[SDL_SCANCODE_TAB] = EG_KEY::TAB;
  translation_lut[SDL_SCANCODE_LSHIFT] = EG_KEY::L_SHIFT;
  translation_lut[SDL_SCANCODE_LCTRL] = EG_KEY::L_CTRL;
  translation_lut[SDL_SCANCODE_ESCAPE] = EG_KEY::ESCAPE;
}

void Input::poll_events(VulkanEngine* engine) {
  ZoneScopedN("poll events");

  // reset key states
  for (auto& key : keys) {
    key.just_pressed = false;
    key.just_released = false;
  }

  // reset mouse rel
  xrel = 0;
  yrel = 0;
  mwheel = 0;

  SDL_Event e;
  while (SDL_PollEvent(&e) != 0) {
    process_sdl_event(e, engine);
  }
}

void Input::process_sdl_event(SDL_Event& e, VulkanEngine* engine) {
  if (e.type == SDL_QUIT) {
    _should_quit = true;
  }

  switch (e.type) {
    case SDL_MOUSEMOTION: {
      xrel += e.motion.xrel;
      yrel += e.motion.yrel;
      x = e.motion.x;
      y = e.motion.y;
    } break;
    case SDL_WINDOWEVENT: {
      if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED &&
          e.window.data1 > 0 && e.window.data2 > 0) {
        engine->resizeSwapchain(e.window.data1, e.window.data2);
    }
    } break;
    case SDL_KEYDOWN: {
      auto code = translation_lut[e.key.keysym.scancode];
      auto& key = keys.at(static_cast<uint32_t>(code));
      key.half_count += 1;
      key.just_pressed = !key.just_pressed && !key.is_down;
      key.is_down = true;
    } break;
    case SDL_KEYUP: {
      auto code = translation_lut[e.key.keysym.scancode];
      auto& key = keys.at(static_cast<uint32_t>(code));
      key.half_count += 1;
      key.just_released = !key.just_released && key.is_down;
      key.is_down = false;
    } break;
    case SDL_MOUSEBUTTONDOWN: {
      auto code = translation_lut[e.button.button];
      auto& key = keys.at(static_cast<uint32_t>(code));
      key.half_count += 1;
      key.just_pressed = !key.just_pressed && !key.is_down;
      key.is_down = true;
    } break;
    case SDL_MOUSEBUTTONUP: {
      auto code = translation_lut[e.button.button];
      auto& key = keys.at(static_cast<uint32_t>(code));
      key.half_count += 1;
      key.just_released = !key.just_released && key.is_down;
      key.is_down = false;
    } break;
    case SDL_MOUSEWHEEL: {
      mwheel += e.wheel.preciseY;
    } break;
    default:
      break;
  }
}

bool Input::is_key_down(EG_KEY key) {
  return keys.at(static_cast<uint32_t>(key)).is_down;
}

bool Input::is_key_up(EG_KEY key) {
  return !keys.at(static_cast<uint32_t>(key)).is_down;
}

bool Input::was_key_pressed(EG_KEY key) {
  return keys.at(static_cast<uint32_t>(key)).just_pressed;
}

bool Input::was_key_released(EG_KEY key) {
  return keys.at(static_cast<uint32_t>(key)).just_released;
}

std::pair<int32_t, int32_t> Input::get_mouse_rel() { return {xrel, yrel}; }

std::pair<int32_t, int32_t> Input::get_mouse_pos() { return {x, y}; }

bool Input::should_quit() { return _should_quit; }

float Input::get_mouse_wheel() { return mwheel; }

void EngineInput::process_sdl_event(SDL_Event& event, VulkanEngine* engine) {
  ImGui_ImplSDL2_ProcessEvent(&event);

  Input::process_sdl_event(event, engine);
}
