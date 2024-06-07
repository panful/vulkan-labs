#pragma once

#include <cstdint>

enum class EventType : uint8_t
{
    None = 0,
    MouseWheelForward,
    MouseWheelBackward,
    Key,
};

struct Event
{
    EventType type {EventType::None};
};

struct MouseEvent : Event
{
};

struct KeyEvent : Event
{
};
