#include "Camera.h"
#include <glm/gtc/matrix_transform.hpp>

glm::mat4 Camera::GetViewMatrix() const noexcept
{
    return glm::lookAt(m_eyePosition, m_lookAt, m_viewUp);
}

glm::mat4 Camera::GetProjectMatrix(const float aspect) const noexcept
{
    return glm::perspective(glm::radians(45.f), aspect, 0.1f, 100.f);
}
