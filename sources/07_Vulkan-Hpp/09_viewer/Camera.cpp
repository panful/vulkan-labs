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

void Camera::SetEyePosition(glm::vec3&& eyePosition)
{
    m_eyePosition = std::move(eyePosition);
}

void Camera::SetLookAt(glm::vec3&& lookAt)
{
    m_lookAt = std::move(lookAt);
}

void Camera::SetViewUp(glm::vec3&& viewUp)
{
    m_viewUp = std::move(viewUp);
}
