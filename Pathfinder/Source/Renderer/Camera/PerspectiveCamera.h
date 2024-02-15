#ifndef PERSPECTIVECAMERA_H
#define PERSPECTIVECAMERA_H

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

static constexpr float s_MAX_ZFAR  = 10e2f;
static constexpr float s_MIN_ZNEAR = 10e-3f;

static constexpr float s_MAX_PITCH = 89.0F;

class PerspectiveCamera final : public Camera
{
  public:
    PerspectiveCamera() : Camera()
    {
        const auto& window = Application::Get().GetWindow();
        m_AR               = static_cast<float>(window->GetSpecification().Width) / static_cast<float>(window->GetSpecification().Height);

        m_Projection = glm::perspective(glm::radians(m_FOV), m_AR, s_MIN_ZNEAR, s_MAX_ZFAR);
        RecalculateViewMatrix();
    }

    ~PerspectiveCamera() override = default;

    NODISCARD FORCEINLINE const float GetNearPlaneDepth() const final override { return s_MIN_ZNEAR; }
    NODISCARD FORCEINLINE const float GetFarPlaneDepth() const final override { return s_MAX_ZFAR; }

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

        if (bNeedsViewMatrixRecalculation) RecalculateViewMatrix();
    }

    void OnEvent(Event& e) final override
    {
        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<MouseMovedEvent>([&](const auto& e) { return OnMouseMoved(e); });
        dispatcher.Dispatch<MouseScrolledEvent>([&](const auto& e) { return OnMouseScrolled(e); });
        dispatcher.Dispatch<WindowResizeEvent>([&](const auto& e) { return OnWindowResized(e); });
    }

    // TODO: Implement both in persp and ortho
    Frustum GetFrustum() final override
    {
        Frustum fr = {};
        /* const float halfVSide   = GetFarPlaneDepth() * tanf(m_FOV * .5f);
          const float halfHSide        = halfVSide * m_AR;
          const glm::vec3 frontMultFar = GetFarPlaneDepth() * m_Forward;

          fr.Planes[0] = {m_Position, glm::cross(m_Up, frontMultFar + m_Right * halfHSide)};
          fr.Planes[1] = {m_Position, glm::cross(frontMultFar - m_Right * halfHSide, m_Up)};
          fr.Planes[2] = {m_Position, glm::cross(m_Right, frontMultFar - m_Up * halfVSide)};
          fr.Planes[3] = {m_Position, glm::cross(frontMultFar + m_Up * halfVSide, m_Right)};
          fr.Planes[4] = {m_Position + GetNearPlaneDepth() * m_Forward, glm::length(m_Forward)};
          fr.Planes[5] = {m_Position + frontMultFar, glm::length(-m_Forward)};*/

        return fr;
    }

  protected:
    float m_FOV      = 90.0f;
    float m_FOVSpeed = 1.f;

    float m_Yaw   = -90.0f;
    float m_Pitch = 0.0f;
    float m_LastX = 0.0f;
    float m_LastY = 0.0f;

    float m_Sensitivity = 0.2f;
    bool m_bFirstInput  = true;

    // NOTE: again fucking math, but here's simple explanation(assuming column-major matrix order)
    // View matrix (inversed) constructs like this: Mview = (R)^(-1) * T = (R)^(Transpose) * T - only for rotation matrices
    // So you want to move your objects to camera origin instead of moving "cam"
    // T is mat4 E, with its 3rd column looks like negated camera position to move objects back to the camera
    // And you then want to unrotate your camera to make it point into Z direction[canonical view direction](in my case towards -Z) cuz my
    // coordinate system is RH.
    // So R = [R,U,F], r-right vec for X,u-up vec for Y, f-forward vec for Z
    void RecalculateViewMatrix() final override
    {
        // To imitate true fps cam, I don't need to change my constant m_Up, but create new Up vec based on new Right and Forward vectors.
        const auto upVec = glm::cross(m_Right, m_Forward);

        // NOTE: Using formula: R^(transpose) * T; m_Right, upVec, m_Forward - orthogonal basis
        // Inverse rotation back to canonical view direction since I assume that cam always look towards -Z(RH coordinate system)

        // You'll notice that XY(-Z)=RUF => Z=-F, that's fucking why
        m_View[0][0] = m_Right.x;
        m_View[0][1] = upVec.x;
        m_View[0][2] = -m_Forward.x;

        m_View[1][0] = m_Right.y;
        m_View[1][1] = upVec.y;
        m_View[1][2] = -m_Forward.y;

        m_View[2][0] = m_Right.z;
        m_View[2][1] = upVec.z;
        m_View[2][2] = -m_Forward.z;

        // Translate objects back to the camera origin that's why negating
        m_View[3][0] = -glm::dot(m_Right, m_Position);
        m_View[3][1] = -glm::dot(upVec, m_Position);
        m_View[3][2] = glm::dot(m_Forward, m_Position);  // No "-": -Pos * -F
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
        }

        if (!Input::IsMouseButtonPressed(EKey::MOUSE_BUTTON_LEFT)) return false;

        m_Yaw += deltaX * m_Sensitivity;
        m_Pitch += deltaY * m_Sensitivity;
        m_Pitch = std::clamp(m_Pitch, -s_MAX_PITCH, s_MAX_PITCH);

        m_Rotation.x = glm::cos(glm::radians(m_Yaw)) * glm::cos(glm::radians(m_Pitch));
        m_Rotation.y = glm::sin(glm::radians(m_Pitch));
        m_Rotation.z = glm::sin(glm::radians(m_Yaw)) * glm::cos(glm::radians(m_Pitch));
        m_Forward    = glm::normalize(m_Rotation);

        m_Right = glm::normalize(glm::cross(m_Forward, m_Up));
        RecalculateViewMatrix();

        return true;
    }

    bool OnMouseScrolled(const MouseScrolledEvent& e) final override
    {
        PFR_ASSERT(m_FOV > 0.0f, "FOV can't be negative!");

        const auto& mouseScrolledEvent = (MouseScrolledEvent&)e;
        m_FOV -= mouseScrolledEvent.GetOffsetY() * m_FOVSpeed;
        m_FOV = std::clamp(m_FOV, s_MIN_FOV, s_MAX_FOV);

        m_Projection = glm::perspective(glm::radians(m_FOV), m_AR, s_MIN_ZNEAR, s_MAX_ZFAR);
        return true;
    }

    bool OnWindowResized(const WindowResizeEvent& e) final override
    {
        m_AR         = e.GetAspectRatio();
        m_Projection = glm::perspective(glm::radians(m_FOV), m_AR, s_MIN_ZNEAR, s_MAX_ZFAR);
        return true;
    }
};

}  // namespace Pathfinder

#endif  // PERSPECTIVECAMERA_H
