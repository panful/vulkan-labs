#pragma once

#include <array>
#include <cstdint>

enum class EventType : uint8_t
{
    None = 0,

    ResizeWindow,

    MouseMove,

    LeftButtonDown,
    LeftButtonUp,
    RightButtonDown,
    RightButtonUp,
    MiddleButtonDown,
    MiddleButtonUp,

    MouseWheelForward,
    MouseWheelBackward,

    Char,

    KeyDown,
    KeyUp,
    KeyRepeat,
};

enum class EventModifiers : uint8_t
{
    None = 0,
    LeftShift,
    RightShift,
    LeftCtrl,
    RightCtrl,
    LeftAlt,
    RightAlt,

    Caps,
    Num,
};

enum class EventKeyCode : uint8_t
{
    None = 0,

    Num_1,
    Num_2,
    // ...
    Num_9,

    Alpha_A,
    Alpha_B,
    // ...
    Alpha_Z,
};

struct Event
{
    EventType type {EventType::None};
    std::array<int, 2> position {0, 0};
    EventModifiers modifiers {EventModifiers::None};
    EventKeyCode keyCode {EventKeyCode::None};
    int repeatCount {0};
    int keySym {0};
};
