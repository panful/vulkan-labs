#pragma once

#include <array>
#include <memory>

struct GLFWwindow;
struct Window;
class InteractorStyle;

class Interactor
{
public:
    void Start();

    void SetWindow(const std::shared_ptr<Window>& window);

private:
    static void CursorPosCallback(GLFWwindow* window, double xpos, double ypos) noexcept;
    static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) noexcept;
    static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset) noexcept;
    static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) noexcept;
    static void WindowSizeCallback(GLFWwindow* window, int width, int height) noexcept;

private:
    inline static bool s_windowResized {false};
    inline static uint32_t s_windowWidth {800};
    inline static uint32_t s_windowHeight {600};

    std::shared_ptr<Window> m_window {};

    std::array<int, 2> m_eventPos {};
    uint16_t m_key {0};
};
