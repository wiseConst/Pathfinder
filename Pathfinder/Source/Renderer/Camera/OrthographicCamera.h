#ifndef ORTHOGRAPHICCAMERA_H
#define ORTHOGRAPHICCAMERA_H

#include "Camera.h"
#include "Core/Application.h"
#include "Core/Window.h"
#include "Core/Input.h"

#include "Events/KeyEvent.h"
#include "Events/MouseEvent.h"
#include "Events/WindowEvent.h"

namespace Pathfinder
{

class OrthographicCamera final : public Camera
{
  public:
    OrthographicCamera() : Camera()
    {
        const auto& window = Application::Get().GetWindow();
        m_AR               = static_cast<float>(window->GetSpecification().Width) / static_cast<float>(window->GetSpecification().Height);

        m_Projection =
            glm::ortho(-s_CameraWidth * m_AR, s_CameraWidth * m_AR, -s_CameraWidth, s_CameraWidth, -s_CameraWidth, s_CameraWidth);

        m_ZoomLevel = s_CameraWidth;
        RecalculateViewMatrix();
    }

    ~OrthographicCamera() override = default;

    NODISCARD FORCEINLINE const float GetNearPlaneDepth() const final override { return -m_ZoomLevel; }
    NODISCARD FORCEINLINE const float GetFarPlaneDepth() const final override { return m_ZoomLevel; }

    void OnUpdate(const float deltaTime) final override
    {
        m_DeltaTime = deltaTime;

        // Saving some CPU cycles sine matrix multiplication sucks
        bool bNeedsViewMatrixRecalculation = false;

        // Movement
        if (Input::IsKeyPressed(EKey::KEY_S))
        {
            m_Position -= m_Up * s_MovementSpeed * deltaTime;
            bNeedsViewMatrixRecalculation = true;
        }

        if (Input::IsKeyPressed(EKey::KEY_W))
        {
            m_Position += m_Up * s_MovementSpeed * deltaTime;
            bNeedsViewMatrixRecalculation = true;
        }

        if (Input::IsKeyPressed(EKey::KEY_A))
        {
            m_Position -= m_Right * s_MovementSpeed * deltaTime;
            bNeedsViewMatrixRecalculation = true;
        }

        if (Input::IsKeyPressed(EKey::KEY_D))
        {
            m_Position += m_Right * s_MovementSpeed * deltaTime;
            bNeedsViewMatrixRecalculation = true;
        }

        // Rotation
        if (Input::IsKeyPressed(EKey::KEY_Q))
        {
            m_Rotation.z -= s_MovementSpeed * s_MovementSpeed * deltaTime;
            bNeedsViewMatrixRecalculation = true;
        }

        if (Input::IsKeyPressed(EKey::KEY_E))
        {
            m_Rotation.z += s_MovementSpeed * s_MovementSpeed * deltaTime;
            bNeedsViewMatrixRecalculation = true;
        }

        if (Input::IsKeyPressed(EKey::KEY_Z))
        {
            m_Rotation.y -= s_MovementSpeed * s_MovementSpeed * deltaTime;
            bNeedsViewMatrixRecalculation = true;
        }

        if (Input::IsKeyPressed(EKey::KEY_X))
        {
            m_Rotation.y += s_MovementSpeed * s_MovementSpeed * deltaTime;
            bNeedsViewMatrixRecalculation = true;
        }

        if (Input::IsKeyPressed(EKey::KEY_C))
        {
            m_Rotation.x -= s_MovementSpeed * s_MovementSpeed * deltaTime;
            bNeedsViewMatrixRecalculation = true;
        }

        if (Input::IsKeyPressed(EKey::KEY_V))
        {
            m_Rotation.x += s_MovementSpeed * s_MovementSpeed * deltaTime;
            bNeedsViewMatrixRecalculation = true;
        }

        if (bNeedsViewMatrixRecalculation) RecalculateViewMatrix();
    }

    void OnEvent(Event& e) final override
    {
        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<MouseMovedEvent>([&](const MouseMovedEvent& e) { return OnMouseMoved(e); });
        dispatcher.Dispatch<MouseScrolledEvent>([&](const MouseScrolledEvent& e) { return OnMouseScrolled(e); });
        dispatcher.Dispatch<WindowResizeEvent>([&](const WindowResizeEvent& e) { return OnWindowResized(e); });
    }

    Frustum GetFrustum() final override
    {
        Frustum fr = {};
        fr.Bottom  = glm::vec3(m_Position.x, m_Position.y, m_Forward.z);
        fr.Top     = glm::vec3(0, 0, 0);
        fr.Left    = glm::vec3(0, 0, 0);
        fr.Right   = glm::vec3(0, 0, 0);
        fr.Far     = glm::vec3(0, 0, m_ZoomLevel * m_Position.z);
        fr.Near    = glm::vec3(0, 0, m_ZoomLevel * m_Position.z);

        return fr;
    }

  protected:
    static constexpr float s_CameraWidth = 8.0f;
    float m_ZoomLevel                    = 10.0f;
    float m_ZoomSpeed                    = 0.75f;

    void RecalculateViewMatrix() final override
    {
        const glm::mat4 transform = glm::rotate(glm::mat4(1.0f), glm::radians(m_Rotation.y), glm::vec3(0, 1, 0)) *
                                    glm::rotate(glm::mat4(1.0f), glm::radians(m_Rotation.x), glm::vec3(1, 0, 0)) *
                                    glm::rotate(glm::mat4(1.0f), glm::radians(m_Rotation.z), glm::vec3(0, 0, 1)) *
                                    glm::translate(glm::mat4(1.0f), m_Position);

        m_View = glm::inverse(transform);
    }

    bool OnMouseMoved(const MouseMovedEvent& e) final override { return false; }

    bool OnMouseScrolled(const MouseScrolledEvent& e) final override
    {
        PFR_ASSERT(m_ZoomSpeed > 0, "Zoom speed can't be negative!");

        const auto& mouseScrolledEvent = (MouseScrolledEvent&)e;
        m_ZoomLevel -= mouseScrolledEvent.GetOffsetY() * m_ZoomSpeed;

        if (m_ZoomLevel < m_ZoomSpeed * 2) m_ZoomLevel = m_ZoomSpeed * 2;
        m_Projection = glm::ortho(-m_ZoomLevel * m_AR, m_ZoomLevel * m_AR, -m_ZoomLevel, m_ZoomLevel, -m_ZoomLevel, m_ZoomLevel);

        return true;
    }

    bool OnWindowResized(const WindowResizeEvent& e) final override
    {
        m_AR         = e.GetAspectRatio();
        m_Projection = glm::ortho(-m_ZoomLevel * m_AR, m_ZoomLevel * m_AR, -m_ZoomLevel, m_ZoomLevel, -m_ZoomLevel, m_ZoomLevel);
        return true;
    }
};

}  // namespace Pathfinder

#endif  // ORTHOGRAPHICCAMERA_H
