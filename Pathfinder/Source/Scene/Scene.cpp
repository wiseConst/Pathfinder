#include <PathfinderPCH.h>
#include "Scene.h"

#include "Entity.h"
#include "Components.h"

#include <Renderer/Renderer.h>
#include <Renderer/Renderer2D.h>
#include <Renderer/Pipeline.h>
#include <Renderer/Shader.h>
#include <Renderer/HWRT.h>
#include <Renderer/Debug/DebugRenderer.h>

namespace Pathfinder
{
FORCEINLINE static DirectionalLight DirectionalLightFromDirectionalLightComponent(const glm::vec3& direction,
                                                                                  const DirectionalLightComponent& dlc)
{
    return {direction, dlc.Intensity, dlc.Color, dlc.bCastShadows};
}

FORCEINLINE static PointLight PointLightFromPointLightComponent(const glm::vec3& position, const PointLightComponent& plc)
{
    return {position, plc.Intensity, plc.Color, plc.Radius, plc.MinRadius, plc.bCastShadows};
}

FORCEINLINE static SpotLight SpotLightFromSpotLightComponent(const glm::vec3& position, const SpotLightComponent& slc)
{
    return {position, slc.Intensity, slc.Direction, slc.Height, slc.Color, slc.Radius, slc.InnerCutOff, slc.OuterCutOff, slc.bCastShadows};
}

Scene::Scene(const std::string& sceneName) : m_Name(sceneName) {}

Scene::~Scene()
{
    for (const auto entityID : m_Registry.view<IDComponent>())
        m_Registry.destroy(entityID);

    m_EntityCount = 0;

    // RayTracingBuilder::DestroyAccelerationStructure(m_TLAS);
}

void Scene::OnUpdate(const float deltaTime)
{
    // RebuildTLAS();

    std::scoped_lock<std::mutex> lock(m_SceneMutex);
    for (const auto entityID : m_Registry.view<IDComponent>())
    {
        Entity entity{entityID, this};
        auto& tc = entity.GetComponent<TransformComponent>();

        if (entity.HasComponent<SpriteComponent>())
        {
            const auto& sc = entity.GetComponent<SpriteComponent>();

            const auto quaternionOrientation = glm::quat{tc.Rotation};
            Renderer2D::DrawQuad(tc.Translation, tc.Scale,
                                 {quaternionOrientation.x, quaternionOrientation.y, quaternionOrientation.z, quaternionOrientation.w},
                                 sc.Color, sc.Texture, sc.Layer);
        }

        if (entity.HasComponent<MeshComponent>())
        {
            const auto& mc                   = entity.GetComponent<MeshComponent>();
            const auto quaternionOrientation = glm::quat{tc.Rotation};
            Renderer::SubmitMesh(mc.Mesh, tc.Translation, tc.Scale,
                                 {quaternionOrientation.x, quaternionOrientation.y, quaternionOrientation.z, quaternionOrientation.w});

            if (Renderer::GetRendererSettings().bDrawColliders || mc.bDrawBoundingSphere)
            {
                //  DebugRenderer::DrawAABB(mc.Mesh, tc, glm::vec4(1, 1, 0, 1));
                DebugRenderer::DrawSphere(
                    mc.Mesh, tc.Translation, tc.Scale,
                    {quaternionOrientation.x, quaternionOrientation.y, quaternionOrientation.z, quaternionOrientation.w},
                    glm::vec4(0, 0, 1, 1));
            }
        }

        if (entity.HasComponent<DirectionalLightComponent>())
        {
            const auto& dlc = entity.GetComponent<DirectionalLightComponent>();
            Renderer::AddDirectionalLight(DirectionalLightFromDirectionalLightComponent(tc.Translation, dlc));
        }

        if (entity.HasComponent<PointLightComponent>())
        {
            const auto& plc = entity.GetComponent<PointLightComponent>();
            Renderer::AddPointLight(PointLightFromPointLightComponent(tc.Translation, plc));

            // NOTE: Since I'm using Translation as a position, and retreiving TRS matrix, Translation already there, so I remove it and put
            // it back after rendering.
            if (plc.bDrawBoundingSphere)
            {
                tc.Translation           = glm::vec3(0.0f);
                const glm::vec3 position = tc.Translation;

                const auto quaternionOrientation = glm::quat{tc.Rotation};

                //  DebugRenderer::DrawAABB(mc.Mesh, tc, glm::vec4(1, 1, 0, 1));
                DebugRenderer::DrawSphere(
                    tc.Translation, tc.Scale,
                    {quaternionOrientation.x, quaternionOrientation.y, quaternionOrientation.z, quaternionOrientation.w}, position,
                    plc.Radius, glm::vec4(0, 0, 1, 1));

                tc.Translation = position;
            }
        }

        if (entity.HasComponent<SpotLightComponent>())
        {
            const auto& slc = entity.GetComponent<SpotLightComponent>();
            Renderer::AddSpotLight(SpotLightFromSpotLightComponent(tc.Translation, slc));
        }
    }
}

void Scene::RebuildTLAS()
{
    std::scoped_lock<std::mutex> lock(m_SceneMutex);
    // NOTE: Temp, no TLAS rebuilding/updating for now.
    if (m_TLAS.Buffer && m_TLAS.Handle) return;

    const auto meshEntites = m_Registry.view<MeshComponent>();
    PFR_ASSERT(!meshEntites.empty(), "m_Registry.view<MeshComponent>() is empty().");

    std::vector<Shared<Mesh>> meshes;
    for (const auto& entityID : meshEntites)
    {
        Entity entity{entityID, this};
        meshes.emplace_back(entity.GetComponent<MeshComponent>().Mesh);
    }
    meshes.shrink_to_fit();

    Timer t                                   = {};
    std::vector<AccelerationStructure> blases = RayTracingBuilder::BuildBLASes(meshes);
    m_TLAS                                    = RayTracingBuilder::BuildTLAS(blases);

    for (auto& blas : blases)
        RayTracingBuilder::DestroyAccelerationStructure(blas);

    PipelineLibrary::Get(Renderer::GetRendererData()->RTPipelineHash)->GetSpecification().Shader->Set("u_SceneTLAS", m_TLAS);
    PipelineLibrary::Get(Renderer::GetRendererData()->RTComputePipelineHash)->GetSpecification().Shader->Set("u_SceneTLAS", m_TLAS);
    LOG_DEBUG("Time taken to rebuild TLAS: %0.3f ms.", t.GetElapsedMilliseconds());
}

Entity Scene::CreateEntity(const std::string& entityName)
{
    return CreateEntityWithUUID(UUID{}, entityName);
}

Entity Scene::CreateEntityWithUUID(const UUID uuid, const std::string& entityName)
{
    PFR_ASSERT(!entityName.empty(), "Entity name is empty!");

    std::scoped_lock<std::mutex> lock(m_SceneMutex);
    ++m_EntityCount;

    // Creating entity with 3 core components.
    Entity entity{m_Registry.create(), this};
    entity.AddComponent<IDComponent>(uuid);
    entity.AddComponent<TagComponent>().Tag = entityName;
    entity.AddComponent<TransformComponent>();

    return entity;
}

void Scene::DestroyEntity(const Entity entity)
{
    std::scoped_lock<std::mutex> lock(m_SceneMutex);
    m_Registry.destroy(entity);

    --m_EntityCount;
}
}  // namespace Pathfinder