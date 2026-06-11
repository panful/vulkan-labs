#pragma once

#include <glm/glm.hpp>

class Camera
{
public:
    glm::mat4 GetViewMatrix() const noexcept;
    glm::mat4 GetProjectMatrix(const float aspect) const noexcept;

    void SetEyePosition(glm::vec3&& eyePosition);
    void SetLookAt(glm::vec3&& lookAt);
    void SetViewUp(glm::vec3&& viewUp);

private:
    glm::vec3 m_eyePosition {0.f, 0.f, 3.f};
    glm::vec3 m_lookAt {0.f, 0.f, 0.f};
    glm::vec3 m_viewUp {0.f, 1.f, 0.f};
};
