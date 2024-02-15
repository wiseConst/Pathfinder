#ifndef CAMERA_H
#define CAMERA_H

#include "Core/Core.h"
#include "Core/Math.h"
#include "Globals.h"  // So here I assume that camera creates us a frustum

namespace Pathfinder
{

enum class ECameraType : uint8_t
{
    CAMERA_TYPE_ORTHOGRAPHIC = 0,
    CAMERA_TYPE_PERSPECTIVE
};

class Event;
class MouseMovedEvent;
class MouseScrolledEvent;
class WindowResizeEvent;
class Camera : private Uncopyable, private Unmovable
{
  public:
    virtual ~Camera() = default;

    // NOTE: Returns inverse view matrix.
    // glm::lookAt automatically gives you inversed view matrix
    // for ortho I do inversion by hand
    NODISCARD FORCEINLINE const auto& GetView() const { return m_View; }

    NODISCARD FORCEINLINE auto GetViewProjection() const { return m_Projection * m_View; }
    NODISCARD FORCEINLINE const auto& GetProjection() const { return m_Projection; }
    NODISCARD FORCEINLINE auto GetInverseProjection() const { return glm::inverse(m_Projection); }
    NODISCARD FORCEINLINE const auto& GetPosition() const { return m_Position; }
    NODISCARD FORCEINLINE const auto& GetForwardVector() const { return m_Forward; }

    NODISCARD FORCEINLINE virtual const float GetNearPlaneDepth() const = 0;
    NODISCARD FORCEINLINE virtual const float GetFarPlaneDepth() const  = 0;

    NODISCARD static Shared<Camera> Create(const ECameraType cameraType);

    virtual void OnEvent(Event& e)               = 0;
    virtual void OnUpdate(const float deltaTime) = 0;

    virtual Frustum GetFrustum() = 0;

  protected:
    glm::mat4 m_View       = glm::mat4(1.0f);
    glm::mat4 m_Projection = glm::mat4(1.0f);
    glm::vec3 m_Position   = glm::vec3(0.0f);
    glm::vec3 m_Rotation   = glm::vec3(0.0f);

    glm::vec3 m_Right    = glm::vec3(1.0f, 0.0f, 0.0f);
    const glm::vec3 m_Up = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 m_Forward  = glm::vec3(0.0f, 0.0f, -1.0f);  // RH coordinate system, +Z points towards you(the viewer)
    float m_DeltaTime    = 0.0f;

    float m_AR                             = 1.0f;
    static constexpr float s_MovementSpeed = 15.0f;

    Camera()                                                  = default;
    virtual void RecalculateViewMatrix()                      = 0;
    virtual bool OnMouseMoved(const MouseMovedEvent& e)       = 0;
    virtual bool OnMouseScrolled(const MouseScrolledEvent& e) = 0;
    virtual bool OnWindowResized(const WindowResizeEvent& e)  = 0;
};

}  // namespace Pathfinder

#endif  // CAMERA_H
