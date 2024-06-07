#pragma once

class InteractorStyle
{
public:
    virtual ~InteractorStyle() noexcept;

    virtual void MouseMoveEvent();
    virtual void RightButtonPressEvent();
    virtual void RightButtonReleaseEvent();
    virtual void LeftButtonPressEvent();
    virtual void LeftButtonReleaseEvent();
    virtual void MiddleButtonPressEvent();
    virtual void MiddleButtonReleaseEvent();
    virtual void MouseWheelForwardEvent();
    virtual void MouseWheelBackwardEvent();

    virtual void EnterEvent();
    virtual void LeaveEvent();
    virtual void KeyPressEvent();
    virtual void KeyReleaseEvent();
    virtual void CharEvent();
};
