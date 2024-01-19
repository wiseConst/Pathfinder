#ifndef CAMERA_H
#define CAMERA_H

#include "Core/Core.h"
#include "Core/Math.h"

namespace Pathfinder
{

enum class ECameraType : uint8_t
{
    CAMERA_TYPE_ORTHOGRAPHIC = 0,
    CAMERA_TYPE_PERSPECTIVE
};

// NOTE: I'm so fucking confused by glm::lookAt, does it really return already inversed view matrix??
class Event;
class Camera : private Uncopyable, private Unmovable
{
  public:
    virtual ~Camera() = default;

    NODISCARD FORCEINLINE const auto& GetView() const { return m_View; }
    NODISCARD FORCEINLINE const auto& GetProjection() const { return m_Projection; }
    NODISCARD FORCEINLINE auto GetInverseView() const { return glm::inverse(m_View); }
    NODISCARD FORCEINLINE auto GetViewProjection() const { return m_Projection * m_View; }
    NODISCARD FORCEINLINE const auto& GetPosition() const { return m_Position; }

    NODISCARD static Shared<Camera> Create(const ECameraType cameraType);

    virtual void OnEvent(Event& e)               = 0;
    virtual void OnUpdate(const float deltaTime) = 0;

  protected:
    glm::mat4 m_View       = glm::mat4(1.0f);
    glm::mat4 m_Projection = glm::mat4(1.0f);
    glm::vec3 m_Position   = glm::vec3(0.0f);
    glm::vec3 m_Rotation   = glm::vec3(0.0f);

    glm::vec3 m_Up      = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 m_Forward = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 m_Right   = glm::vec3(1.0f, 0.0f, 0.0f);
    float m_DeltaTime   = 0.0f;

    float m_ZoomLevel                      = 10.0f;
    float m_ZoomSpeed                      = 0.75f;
    static constexpr float s_MovementSpeed = 15.0f;

    Camera()                             = default;
    virtual void RecalculateViewMatrix() = 0;
};

}  // namespace Pathfinder

#endif  // CAMERA_H
