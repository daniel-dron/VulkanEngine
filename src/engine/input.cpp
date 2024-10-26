/******************************************************************************
******************************************************************************
**                                                                           **
**                             Twilight Engine                               **
**                                                                           **
**                  Copyright (c) 2024-present Daniel Dron                   **
**                                                                           **
**            This software is released under the MIT License.               **
**                 https://opensource.org/licenses/MIT                       **
**                                                                           **
******************************************************************************
******************************************************************************/

#pragma once

#include <pch.h>

#include "input.h"

#include <imgui_impl_sdl2.h>
#include <tracy/Tracy.hpp>

#include "../vk_engine.h"

void Input::Init( ) {
    m_translationLut[SDL_BUTTON_LEFT] = EG_KEY::MOUSE_LEFT;
    m_translationLut[SDL_BUTTON_RIGHT] = EG_KEY::MOUSE_RIGHT;
    m_translationLut[SDL_BUTTON_MIDDLE] = EG_KEY::MOUSE_MIDDLE;

    // Numbers
    m_translationLut[SDL_SCANCODE_0] = EG_KEY::N0;
    m_translationLut[SDL_SCANCODE_1] = EG_KEY::N1;
    m_translationLut[SDL_SCANCODE_2] = EG_KEY::N2;
    m_translationLut[SDL_SCANCODE_3] = EG_KEY::N3;
    m_translationLut[SDL_SCANCODE_4] = EG_KEY::N4;
    m_translationLut[SDL_SCANCODE_5] = EG_KEY::N5;
    m_translationLut[SDL_SCANCODE_6] = EG_KEY::N6;
    m_translationLut[SDL_SCANCODE_7] = EG_KEY::N7;
    m_translationLut[SDL_SCANCODE_8] = EG_KEY::N8;
    m_translationLut[SDL_SCANCODE_9] = EG_KEY::N9;

    // Letters
    m_translationLut[SDL_SCANCODE_A] = EG_KEY::A;
    m_translationLut[SDL_SCANCODE_B] = EG_KEY::B;
    m_translationLut[SDL_SCANCODE_C] = EG_KEY::C;
    m_translationLut[SDL_SCANCODE_D] = EG_KEY::D;
    m_translationLut[SDL_SCANCODE_E] = EG_KEY::E;
    m_translationLut[SDL_SCANCODE_F] = EG_KEY::F;
    m_translationLut[SDL_SCANCODE_G] = EG_KEY::G;
    m_translationLut[SDL_SCANCODE_H] = EG_KEY::H;
    m_translationLut[SDL_SCANCODE_J] = EG_KEY::J;
    m_translationLut[SDL_SCANCODE_K] = EG_KEY::K;
    m_translationLut[SDL_SCANCODE_L] = EG_KEY::L;
    m_translationLut[SDL_SCANCODE_M] = EG_KEY::M;
    m_translationLut[SDL_SCANCODE_N] = EG_KEY::N;
    m_translationLut[SDL_SCANCODE_O] = EG_KEY::O;
    m_translationLut[SDL_SCANCODE_P] = EG_KEY::P;
    m_translationLut[SDL_SCANCODE_Q] = EG_KEY::Q;
    m_translationLut[SDL_SCANCODE_R] = EG_KEY::R;
    m_translationLut[SDL_SCANCODE_S] = EG_KEY::S;
    m_translationLut[SDL_SCANCODE_T] = EG_KEY::T;
    m_translationLut[SDL_SCANCODE_U] = EG_KEY::U;
    m_translationLut[SDL_SCANCODE_V] = EG_KEY::V;
    m_translationLut[SDL_SCANCODE_W] = EG_KEY::W;
    m_translationLut[SDL_SCANCODE_X] = EG_KEY::X;
    m_translationLut[SDL_SCANCODE_Y] = EG_KEY::Y;
    m_translationLut[SDL_SCANCODE_Z] = EG_KEY::Z;

    // Special keys
    m_translationLut[SDL_SCANCODE_SPACE] = EG_KEY::SPACE;
    m_translationLut[SDL_SCANCODE_BACKSPACE] = EG_KEY::BACKSPACE;
    m_translationLut[SDL_SCANCODE_RETURN] = EG_KEY::ENTER;
    m_translationLut[SDL_SCANCODE_TAB] = EG_KEY::TAB;
    m_translationLut[SDL_SCANCODE_LSHIFT] = EG_KEY::L_SHIFT;
    m_translationLut[SDL_SCANCODE_LCTRL] = EG_KEY::L_CTRL;
    m_translationLut[SDL_SCANCODE_ESCAPE] = EG_KEY::ESCAPE;
}

void Input::PollEvents( VulkanEngine *engine ) {
    ZoneScopedN( "poll events" );

    // reset key states
    for ( auto &key : m_keys ) {
        key.justPressed = false;
        key.justReleased = false;
    }

    // reset mouse rel
    m_xrel = 0;
    m_yrel = 0;
    m_mwheel = 0;

    SDL_Event e;
    while ( SDL_PollEvent( &e ) != 0 ) {
        ProcessSdlEvent( e, engine );
    }
}

void Input::ProcessSdlEvent( SDL_Event &event, VulkanEngine *engine ) {
    if ( event.type == SDL_QUIT ) {
        m_shouldQuit = true;
    }

    switch ( event.type ) {
    case SDL_MOUSEMOTION: {
        m_xrel += event.motion.xrel;
        m_yrel += event.motion.yrel;
        m_x = event.motion.x;
        m_y = event.motion.y;
    } break;
    case SDL_WINDOWEVENT: {
        if ( event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED &&
             event.window.data1 > 0 && event.window.data2 > 0 ) {
            engine->ResizeSwapchain( event.window.data1, event.window.data2 );
        }
    } break;
    case SDL_KEYDOWN: {
        auto code = m_translationLut[event.key.keysym.scancode];
        auto &key = m_keys.at( static_cast<uint32_t>( code ) );
        key.halfCount += 1;
        key.justPressed = !key.justPressed && !key.isDown;
        key.isDown = true;
    } break;
    case SDL_KEYUP: {
        auto code = m_translationLut[event.key.keysym.scancode];
        auto &key = m_keys.at( static_cast<uint32_t>( code ) );
        key.halfCount += 1;
        key.justReleased = !key.justReleased && key.isDown;
        key.isDown = false;
    } break;
    case SDL_MOUSEBUTTONDOWN: {
        auto code = m_translationLut[event.button.button];
        auto &key = m_keys.at( static_cast<uint32_t>( code ) );
        key.halfCount += 1;
        key.justPressed = !key.justPressed && !key.isDown;
        key.isDown = true;
    } break;
    case SDL_MOUSEBUTTONUP: {
        auto code = m_translationLut[event.button.button];
        auto &key = m_keys.at( static_cast<uint32_t>( code ) );
        key.halfCount += 1;
        key.justReleased = !key.justReleased && key.isDown;
        key.isDown = false;
    } break;
    case SDL_MOUSEWHEEL: {
        m_mwheel += event.wheel.preciseY;
    } break;
    default:
        break;
    }
}

bool Input::IsKeyDown( EG_KEY key ) {
    return m_keys.at( static_cast<uint32_t>( key ) ).isDown;
}

bool Input::IsKeyUp( EG_KEY key ) {
    return !m_keys.at( static_cast<uint32_t>( key ) ).isDown;
}

bool Input::WasKeyPressed( EG_KEY key ) {
    return m_keys.at( static_cast<uint32_t>( key ) ).justPressed;
}

bool Input::WasKeyReleased( EG_KEY key ) {
    return m_keys.at( static_cast<uint32_t>( key ) ).justReleased;
}

std::pair<int32_t, int32_t> Input::GetMouseRel( ) { return { m_xrel, m_yrel }; }

std::pair<int32_t, int32_t> Input::GetMousePos( ) { return { m_x, m_y }; }

bool Input::ShouldQuit( ) { return m_shouldQuit; }

float Input::GetMouseWheel( ) { return m_mwheel; }

void EngineInput::ProcessSdlEvent( SDL_Event &event, VulkanEngine *engine ) {
    ImGui_ImplSDL2_ProcessEvent( &event );

    Input::ProcessSdlEvent( event, engine );
}
