#include "PathfinderPCH.h"
#include "Input.h"

// NOTE: Right now events are only coming from main window.
#include "Application.h"
#include "Window.h"

// NOTE: I can also move implementations into Window itself
#include <GLFW/glfw3.h>

namespace Pathfinder
{

static GLFWwindow* GetGLFWHandle()
{
    return static_cast<GLFWwindow*>(Application::Get().GetWindow()->Get());
}

bool Input::IsKeyPressed(const EKey key)
{
    PFR_ASSERT(key >= EKey::KEY_SPACE && key <= EKey::KEY_LAST, "Enum in not appropriate key range!");
    return glfwGetKey(GetGLFWHandle(), (int32_t)key) == GLFW_PRESS || Input::IsKeyRepeated(key);
}

bool Input::IsKeyReleased(const EKey key)
{
    PFR_ASSERT(key >= EKey::KEY_SPACE && key <= EKey::KEY_LAST, "Enum in not appropriate key range!");
    return glfwGetKey(GetGLFWHandle(), (int32_t)key) == GLFW_RELEASE;
}

bool Input::IsKeyRepeated(const EKey key)
{
    PFR_ASSERT(key >= EKey::KEY_SPACE && key <= EKey::KEY_LAST, "Enum in not appropriate key range!");
    return glfwGetKey(GetGLFWHandle(), (int32_t)key) == GLFW_REPEAT;
}

bool Input::IsMouseButtonPressed(const EKey button)
{
    PFR_ASSERT(button >= EKey::MOUSE_BUTTON_1 && button <= EKey::MOUSE_BUTTON_LAST, "Enum in not appropriate mouse button range!");
    return glfwGetMouseButton(GetGLFWHandle(), (int32_t)button) == GLFW_PRESS || IsMouseButtonRepeated(button);
}

bool Input::IsMouseButtonReleased(const EKey button)
{
    PFR_ASSERT(button >= EKey::MOUSE_BUTTON_1 && button <= EKey::MOUSE_BUTTON_LAST, "Enum in not appropriate mouse button range!");
    return glfwGetMouseButton(GetGLFWHandle(), (int32_t)button) == GLFW_RELEASE;
}

bool Input::IsMouseButtonRepeated(const EKey button)
{
    PFR_ASSERT(button >= EKey::MOUSE_BUTTON_1 && button <= EKey::MOUSE_BUTTON_LAST, "Enum in not appropriate mouse button range!");
    return glfwGetMouseButton(GetGLFWHandle(), (int32_t)button) == GLFW_REPEAT;
}

std::pair<int32_t, int32_t> Input::GetMousePosition()
{
    double xpos = 0.0, ypos = 0.0;
    glfwGetCursorPos(GetGLFWHandle(), &xpos, &ypos);

    return {static_cast<int32_t>(xpos), static_cast<int32_t>(ypos)};
}

}  // namespace Pathfinder