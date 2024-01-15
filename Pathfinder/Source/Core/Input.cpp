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

// TODO: Instead of casting here's should be otobrazhenie moix knopok v knopki glfw
bool Input::IsKeyPressed(const EKey key)
{
    return glfwGetKey(GetGLFWHandle(), (int32_t)key) == GLFW_PRESS || Input::IsKeyRepeated(key);
}

bool Input::IsKeyReleased(const EKey key)
{
    return glfwGetKey(GetGLFWHandle(), (int32_t)key) == GLFW_RELEASE;
}

bool Input::IsKeyRepeated(const EKey key)
{
    return glfwGetKey(GetGLFWHandle(), (int32_t)key) == GLFW_REPEAT;
}

}  // namespace Pathfinder