#include "Interactor.h"
#include "Event.h"
#include "InteractorStyle.h"
#include "Viewer.h"
#include "Window.h"
#include <GLFW/glfw3.h>

void Interactor::Start()
{
    glfwSetWindowUserPointer(m_window->GetWindowHelper()->window, this);
    glfwSetCursorPosCallback(m_window->GetWindowHelper()->window, CursorPosCallback);
    glfwSetKeyCallback(m_window->GetWindowHelper()->window, KeyCallback);
    glfwSetScrollCallback(m_window->GetWindowHelper()->window, ScrollCallback);
    glfwSetMouseButtonCallback(m_window->GetWindowHelper()->window, MouseButtonCallback);
    glfwSetFramebufferSizeCallback(m_window->GetWindowHelper()->window, FramebufferSizeCallback);

    while (!m_window->GetWindowHelper()->ShouldExit())
    {
        m_window->GetWindowHelper()->PollEvents();
    }
}

void Interactor::SetWindow(const std::shared_ptr<Window>& window)
{
    m_window = window;
}

void Interactor::FramebufferSizeCallback(GLFWwindow* window, int width, int height) noexcept
{
    if (width <= 0 || height <= 0)
    {
        return;
    }

    if (auto pInstance = static_cast<Interactor*>(glfwGetWindowUserPointer(window)))
    {
        pInstance->m_window->WaitIdle();
        pInstance->m_window->GetViewer()->ResizeFramebuffer(vk::Extent2D {static_cast<uint32_t>(width), static_cast<uint32_t>(height)});
        pInstance->m_window->ResizeWindow(vk::Extent2D {static_cast<uint32_t>(width), static_cast<uint32_t>(height)});

        Event event {.type = EventType::ResizeWindow};
        pInstance->m_window->GetViewer()->ProcessEvent(event);
    }
}

void Interactor::CursorPosCallback(GLFWwindow* window, double xpos, double ypos) noexcept
{
    if (auto pInstance = static_cast<Interactor*>(glfwGetWindowUserPointer(window)))
    {
        pInstance->m_eventPos = {static_cast<int>(xpos), static_cast<int>(ypos)};

        Event event {.type = EventType::MouseMove, .position = pInstance->m_eventPos};
        pInstance->m_window->GetViewer()->ProcessEvent(event);
    }
}

void Interactor::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) noexcept
{
    if (auto pInstance = static_cast<Interactor*>(glfwGetWindowUserPointer(window)))
    {
        Event event {.position = pInstance->m_eventPos};

        switch (action)
        {
            case GLFW_PRESS:
            {
                switch (button)
                {
                    case GLFW_MOUSE_BUTTON_LEFT:
                    {
                        event.type = EventType::LeftButtonDown;
                        break;
                    }
                    case GLFW_MOUSE_BUTTON_MIDDLE:
                    {
                        event.type = EventType::MiddleButtonDown;
                        break;
                    }
                    case GLFW_MOUSE_BUTTON_RIGHT:
                    {
                        event.type = EventType::RightButtonDown;
                        break;
                    }
                }
                break;
            }
            case GLFW_RELEASE:
            {
                switch (button)
                {
                    case GLFW_MOUSE_BUTTON_LEFT:
                    {
                        event.type = EventType::LeftButtonUp;
                        break;
                    }
                    case GLFW_MOUSE_BUTTON_MIDDLE:
                    {
                        event.type = EventType::MiddleButtonUp;
                        break;
                    }
                    case GLFW_MOUSE_BUTTON_RIGHT:
                    {
                        event.type = EventType::RightButtonUp;
                        break;
                    }
                }
                break;
            }
        }

        pInstance->m_window->GetViewer()->ProcessEvent(event);
    }
}

void Interactor::ScrollCallback(GLFWwindow* window, double xoffset, double yoffset) noexcept
{
    if (auto pInstance = static_cast<Interactor*>(glfwGetWindowUserPointer(window)))
    {
        Event event {.position = pInstance->m_eventPos};

        if (yoffset > 0.0)
        {
            event.type = EventType::MouseWheelForward;
        }
        else if (yoffset < 0.0)
        {
            event.type = EventType::MouseWheelBackward;
        }

        pInstance->m_window->GetViewer()->ProcessEvent(event);
    }
}

void Interactor::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) noexcept
{
    if (auto pInstance = static_cast<Interactor*>(glfwGetWindowUserPointer(window)))
    {
        Event event {.position = pInstance->m_eventPos};

        switch (action)
        {
            case GLFW_PRESS:
                event.type = EventType::KeyDown;
                break;
            case GLFW_RELEASE:
                event.type = EventType::KeyUp;
                break;
            case GLFW_REPEAT:
                event.type = EventType::KeyRepeat;
                break;
        }

        switch (key)
        {
            case GLFW_KEY_0:
            case GLFW_KEY_1:
            case GLFW_KEY_2:
            case GLFW_KEY_3:
            case GLFW_KEY_4:
            case GLFW_KEY_5:
            case GLFW_KEY_6:
            case GLFW_KEY_7:
            case GLFW_KEY_8:
            case GLFW_KEY_9:
                event.keyCode = EventKeyCode::Num_1; // XXX
                break;
            case GLFW_KEY_A:
            case GLFW_KEY_B:
            case GLFW_KEY_C:
            case GLFW_KEY_D:
            case GLFW_KEY_E:
            case GLFW_KEY_F:
            case GLFW_KEY_G:
            case GLFW_KEY_H:
            case GLFW_KEY_I:
            case GLFW_KEY_J:
            case GLFW_KEY_K:
            case GLFW_KEY_L:
            case GLFW_KEY_M:
            case GLFW_KEY_N:
            case GLFW_KEY_O:
            case GLFW_KEY_P:
            case GLFW_KEY_Q:
            case GLFW_KEY_R:
            case GLFW_KEY_S:
            case GLFW_KEY_T:
            case GLFW_KEY_U:
            case GLFW_KEY_V:
            case GLFW_KEY_W:
            case GLFW_KEY_X:
            case GLFW_KEY_Y:
            case GLFW_KEY_Z:
                event.keyCode = EventKeyCode::Alpha_A; // XXX mods == GLFW_MOD_CAPS_LOCK ? key : key + 32;
                break;
            case GLFW_KEY_RIGHT:
            case GLFW_KEY_LEFT:
            case GLFW_KEY_DOWN:
            case GLFW_KEY_UP:
                break;
        }

        switch (mods)
        {
            case GLFW_MOD_SHIFT:
                event.modifiers = EventModifiers::LeftShift; // XXX Right & Left
                break;
            case GLFW_MOD_CONTROL:
                event.modifiers = EventModifiers::LeftCtrl;
                break;
            case GLFW_MOD_ALT:
                event.modifiers = EventModifiers::LeftAlt;
                break;
            case GLFW_MOD_CAPS_LOCK:
                event.modifiers = EventModifiers::Caps;
                break;
            case GLFW_MOD_NUM_LOCK:
                event.modifiers = EventModifiers::Num;
                break;
        }

        pInstance->m_window->GetViewer()->ProcessEvent(event);
    }
}
