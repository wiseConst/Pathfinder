#pragma once

// CORE
#include "Core/Core.h"
#include "Core/Log.h"
#include "Core/Application.h"
#include "Layers/Layer.h"
#include "Layers/UILayer.h"
#include "Core/Window.h"
#include "Core/Threading.h"

// MISC
#include "Core/Math.h"
#include "Core/Timer.h"
#include "Core/Keys.h"
#include "Core/Input.h"

// EVENTS
#include "Events/Events.h"
#include "Events/WindowEvent.h"
#include "Events/KeyEvent.h"
#include "Events/MouseEvent.h"

// RENDERING
#include "Renderer/Pipeline.h"
#include "Renderer/Swapchain.h"
#include "Renderer/RendererAPI.h"
#include "Renderer/Renderer2D.h"
#include "Renderer/Texture2D.h"
#include "Renderer/Image.h"
#include "Renderer/Renderer.h"
#include "Renderer/Camera/Camera.h"
#include "Renderer/Camera/OrthographicCamera.h"
#include "Renderer/Camera/PerspectiveCamera.h"
#include "Renderer/Mesh/Mesh.h"
#include "Renderer/Mesh/Submesh.h"
#include "Renderer/Material.h"
#include "Renderer/Debug/DebugRenderer.h"

// ECS
#include "Scene/Components.h"
#include "Scene/SceneManager.h"
#include "Scene/Scene.h"
#include "Scene/Entity.h"

// IMGUI
#include <imgui.h>

#include "PathfinderPCH.h"