#pragma once

#include "Camera.h"
#include "Core/Application.h"
#include "Core/Window.h"
#include "Core/Input.h"

#include "Events/KeyEvent.h"
#include "Events/MouseEvent.h"
#include "Events/WindowEvent.h"

namespace Pathfinder
{
static constexpr float s_MAX_FOV = 130.0F;
static constexpr float s_MIN_FOV = 15.0F;

static constexpr float s_MIN_ZNEAR = 10e-3f;
static constexpr float s_MAX_ZFAR  = 10e2f;

static constexpr float s_MAX_PITCH = 89.0F;

class PerspectiveCamera final : public Camera
{
  public:
    PerspectiveCamera() : Camera()
    {
        const auto& window = Application::Get().GetWindow();
        m_AR               = static_cast<float>(window->GetSpecification().Width) / static_cast<float>(window->GetSpecification().Height);

        RecalculateProjectionMatrix();
        RecalculateViewMatrix();
        RecalculateCullFrustum();
    }

    ~PerspectiveCamera() override = default;

    glm::mat4 GetUnreversedProjection() const final override
    {
        return glm::perspective(glm::radians(m_FOV), m_AR, GetNearPlaneDepth(), GetFarPlaneDepth());
    }
    NODISCARD FORCEINLINE const float GetNearPlaneDepth() const final override { return s_MIN_ZNEAR; }
    NODISCARD FORCEINLINE const float GetFarPlaneDepth() const final override { return s_MAX_ZFAR; }

    NODISCARD FORCEINLINE const float GetZoom() const final override { return m_FOV; }

    void OnUpdate(const float deltaTime) final override
    {
        m_DeltaTime = deltaTime;

        bool bNeedsViewMatrixRecalculation = false;
        if (Input::IsKeyPressed(EKey::KEY_W))
        {
            m_Position += m_Forward * m_DeltaTime * s_MovementSpeed;
            bNeedsViewMatrixRecalculation = true;
        }

        if (Input::IsKeyPressed(EKey::KEY_S))
        {
            m_Position -= m_Forward * m_DeltaTime * s_MovementSpeed;
            bNeedsViewMatrixRecalculation = true;
        }

        if (Input::IsKeyPressed(EKey::KEY_D))
        {
            m_Position += m_Right * m_DeltaTime * s_MovementSpeed;
            bNeedsViewMatrixRecalculation = true;
        }

        if (Input::IsKeyPressed(EKey::KEY_A))
        {
            m_Position -= m_Right * m_DeltaTime * s_MovementSpeed;
            bNeedsViewMatrixRecalculation = true;
        }

        if (Input::IsKeyPressed(EKey::KEY_SPACE))
        {
            m_Position += m_Up * m_DeltaTime * s_MovementSpeed;
            bNeedsViewMatrixRecalculation = true;
        }
        if (Input::IsKeyPressed(EKey::KEY_LEFT_CONTROL))
        {
            m_Position -= m_Up * m_DeltaTime * s_MovementSpeed;
            bNeedsViewMatrixRecalculation = true;
        }

        if (bNeedsViewMatrixRecalculation)
        {
            RecalculateViewMatrix();
            RecalculateCullFrustum();
        }
    }

    void OnEvent(Event& e) final override
    {
        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<MouseMovedEvent>([&](const auto& e) { return OnMouseMoved(e); });
        dispatcher.Dispatch<MouseScrolledEvent>([&](const auto& e) { return OnMouseScrolled(e); });
        dispatcher.Dispatch<WindowResizeEvent>([&](const auto& e) { return OnWindowResized(e); });
    }

  protected:
    float m_FOV      = 90.0f;
    float m_FOVSpeed = 1.f;

    float m_Yaw   = -90.0f;
    float m_Pitch = 0.0f;
    float m_LastX = 0.0f;
    float m_LastY = 0.0f;

    float m_Sensitivity = 0.5f;
    bool m_bFirstInput  = true;

    void RecalculateViewMatrix() final override
    {
        const auto upVec = glm::cross(m_Right, m_Forward);
        m_View           = glm::lookAt(m_Position, m_Position + m_Forward, upVec);
    }

    bool OnMouseMoved(const MouseMovedEvent& e) final override
    {
        const float newPosX = e.GetPosX();
        const float newPosY = e.GetPosY();

        if (m_bFirstInput)
        {
            m_bFirstInput = false;

            m_LastX = newPosX;
            m_LastY = newPosY;
        }

        // Note: the coordinates of the mouse are expressed relative to the top left corner of the window
        // whose +x axis points right and +y axis points down, so we revert deltaY
        const float deltaY = -(newPosY - m_LastY);
        const float deltaX = newPosX - m_LastX;

        m_LastX = newPosX;
        m_LastY = newPosY;

        if (Input::IsMouseButtonPressed(EKey::MOUSE_BUTTON_RIGHT) && Input::IsKeyPressed(EKey::KEY_LEFT_SHIFT))
        {
            m_Position.y += deltaY * m_DeltaTime * s_MovementSpeed / 2;
            RecalculateViewMatrix();
            RecalculateCullFrustum();
        }

        if (!Input::IsMouseButtonPressed(EKey::MOUSE_BUTTON_LEFT)) return false;

        m_Yaw += deltaX * m_Sensitivity;
        m_Pitch += deltaY * m_Sensitivity;
        m_Pitch = std::clamp(m_Pitch, -s_MAX_PITCH, s_MAX_PITCH);

        const float cosPitch = glm::cos(glm::radians(m_Pitch));
        m_Rotation.x         = glm::cos(glm::radians(m_Yaw)) * cosPitch;
        m_Rotation.y         = glm::sin(glm::radians(m_Pitch));
        m_Rotation.z         = glm::sin(glm::radians(m_Yaw)) * cosPitch;
        m_Forward            = glm::normalize(m_Rotation);

        m_Right = glm::normalize(glm::cross(m_Forward, m_Up));
        RecalculateViewMatrix();
        RecalculateCullFrustum();

        return true;
    }

    bool OnMouseScrolled(const MouseScrolledEvent& e) final override
    {
        PFR_ASSERT(m_FOV > 0.0f, "FOV can't be negative!");

        const auto& mouseScrolledEvent = (MouseScrolledEvent&)e;
        m_FOV -= mouseScrolledEvent.GetOffsetY() * m_FOVSpeed;
        m_FOV = std::clamp(m_FOV, s_MIN_FOV, s_MAX_FOV);

        RecalculateProjectionMatrix();
        RecalculateCullFrustum();

        return true;
    }

    bool OnWindowResized(const WindowResizeEvent& e) final override
    {
        m_AR = e.GetAspectRatio();
        RecalculateProjectionMatrix();
        RecalculateCullFrustum();

        return true;
    }

    void RecalculateProjectionMatrix() final override
    {
        // NOTE: reversed-z requires swapping far and near.
        m_Projection = glm::perspective(glm::radians(m_FOV), m_AR, GetFarPlaneDepth(), GetNearPlaneDepth());
    }

    void RecalculateCullFrustum()
    {
        const float halfVSide = GetFarPlaneDepth() * glm::tan(glm::radians(m_FOV) * .5f);
        const float halfHSide = halfVSide * m_AR;
        const auto forwardFar = m_Forward * GetFarPlaneDepth();
        const auto trueUpVec  = glm::cross(m_Right, m_Forward);

        // Left, Right, Top, Bottom, Near, Far
        m_CullFrustum.Planes[0] = ComputePlane(m_Position, glm::cross(forwardFar - m_Right * halfHSide, trueUpVec));
        m_CullFrustum.Planes[1] = ComputePlane(m_Position, glm::cross(trueUpVec, forwardFar + m_Right * halfHSide));
        m_CullFrustum.Planes[2] = ComputePlane(m_Position, glm::cross(forwardFar + trueUpVec * halfVSide, m_Right));
        m_CullFrustum.Planes[3] = ComputePlane(m_Position, glm::cross(m_Right, forwardFar - trueUpVec * halfVSide));
        m_CullFrustum.Planes[4] = ComputePlane(m_Position + m_Forward * GetNearPlaneDepth(), m_Forward);
        m_CullFrustum.Planes[5] = ComputePlane(m_Position + forwardFar, -m_Forward);
    }
};

}  // namespace Pathfinder
