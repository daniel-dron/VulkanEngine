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

#include <SDL.h>

// swap here the input implementation
#define EG_INPUT EngineInput::instance( )

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

class TL_Engine;

struct Key {
    bool isDown = false;
    bool justPressed = false;
    bool justReleased = false;
    uint64_t halfCount = 0;
};

class Input {
public:
    static Input &Get( ) {
        static Input input;
        return input;
    }

    void Init( );

    void PollEvents( TL_Engine *engine );

    virtual void ProcessSdlEvent( SDL_Event &event, TL_Engine *engine );

    bool IsKeyDown( EG_KEY key );
    bool IsKeyUp( EG_KEY key );
    bool WasKeyPressed( EG_KEY key );
    bool WasKeyReleased( EG_KEY key );
    std::pair<int32_t, int32_t> GetMouseRel( );
    std::pair<int32_t, int32_t> GetMousePos( );
    float GetMouseWheel( );
    bool ShouldQuit( );

private:
    constexpr static size_t KeyCount = 255;
    std::array<Key, KeyCount> m_keys;
    bool m_shouldQuit = false;

    // mouse
    int32_t m_xrel = 0;
    int32_t m_yrel = 0;
    int32_t m_x = 0;
    int32_t m_y = 0;
    float m_mwheel = 0;

    std::unordered_map<uint16_t, EG_KEY> m_translationLut;
};

class EngineInput : public Input {
public:
    static EngineInput &instance( ) {
        static EngineInput input;
        return input;
    }

    virtual void ProcessSdlEvent( SDL_Event &event, TL_Engine *engine ) override;
};
