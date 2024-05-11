#include "PathfinderPCH.h"
#include "SceneManager.h"

#include "Core/Application.h"

#include "Renderer/RendererCoreDefines.h"
#include "Scene.h"
#include "Entity.h"

#include <nlohmann/json.hpp>

namespace glm
{

static void to_json(auto& j, const glm::vec2& P)
{
    j = {{"x", P.x}, {"y", P.y}};
};

static void from_json(const auto& j, glm::vec2& P)
{
    P.x = j.at("x").get<float>();
    P.y = j.at("y").get<float>();
}

static void to_json(auto& j, const glm::vec3& P)
{
    j = {{"x", P.x}, {"y", P.y}, {"z", P.z}};
};

static void from_json(const auto& j, glm::vec3& P)
{
    P.x = j.at("x").get<float>();
    P.y = j.at("y").get<float>();
    P.z = j.at("z").get<float>();
}

static void to_json(auto& j, const glm::vec4& P)
{
    j = {{"x", P.x}, {"y", P.y}, {"z", P.z}, {"w", P.w}};
};

static void from_json(const auto& j, glm::vec4& P)
{
    P.x = j.at("x").get<float>();
    P.y = j.at("y").get<float>();
    P.z = j.at("z").get<float>();
    P.w = j.at("w").get<float>();
}

}  // namespace glm

namespace Pathfinder
{

static void SerializeEntity(nlohmann::ordered_json& out, const Entity entity)
{
    PFR_ASSERT(entity.HasComponent<IDComponent>(), "Every entity should contain ID component!");

    const std::string UUIDstring = std::to_string(entity.GetUUID());
    out.emplace(UUIDstring, out.object());
    auto& node = out[UUIDstring];

    if (entity.HasComponent<TagComponent>())
    {
        const auto& tc = entity.GetComponent<TagComponent>();
        node.emplace("TagComponent", tc.Tag);
    }

    if (entity.HasComponent<TransformComponent>())
    {
        const auto& tc = entity.GetComponent<TransformComponent>();
        glm::to_json(node["TransformComponent"]["Translation"], tc.Translation);
        glm::to_json(node["TransformComponent"]["Rotation"], tc.Rotation);
        glm::to_json(node["TransformComponent"]["Scale"], tc.Scale);
    }

    if (entity.HasComponent<MeshComponent>())
    {
        const auto& mc = entity.GetComponent<MeshComponent>();
        node["MeshComponent"].emplace("MeshSource", mc.MeshSource);
        node["MeshComponent"].emplace("bDrawBoundingSphere", mc.bDrawBoundingSphere);
    }

    if (entity.HasComponent<PointLightComponent>())
    {
        const auto& plc = entity.GetComponent<PointLightComponent>();
        glm::to_json(node["PointLightComponent"]["Color"], plc.Color);
        node["PointLightComponent"]["Intensity"]           = plc.Intensity;
        node["PointLightComponent"]["Radius"]              = plc.Radius;
        node["PointLightComponent"]["MinRadius"]           = plc.MinRadius;
        node["PointLightComponent"]["bCastShadows"]        = plc.bCastShadows;
        node["PointLightComponent"]["bDrawBoundingSphere"] = plc.bDrawBoundingSphere;
    }

    if (entity.HasComponent<DirectionalLightComponent>())
    {
        const auto& dlc = entity.GetComponent<DirectionalLightComponent>();
        glm::to_json(node["DirectionalLightComponent"]["Color"], dlc.Color);
        node["DirectionalLightComponent"]["Intensity"]    = dlc.Intensity;
        node["DirectionalLightComponent"]["bCastShadows"] = dlc.bCastShadows;
    }

    if (entity.HasComponent<SpotLightComponent>())
    {
        const auto& slc = entity.GetComponent<SpotLightComponent>();
        glm::to_json(node["SpotLightComponent"]["Direction"], slc.Direction);
        glm::to_json(node["SpotLightComponent"]["Color"], slc.Color);
        node["SpotLightComponent"]["Intensity"]    = slc.Intensity;
        node["SpotLightComponent"]["Height"]       = slc.Height;
        node["SpotLightComponent"]["Radius"]       = slc.Radius;
        node["SpotLightComponent"]["InnerCutOff"]  = slc.InnerCutOff;
        node["SpotLightComponent"]["OuterCutOff"]  = slc.OuterCutOff;
        node["SpotLightComponent"]["bCastShadows"] = slc.bCastShadows;
    }
}

void SceneManager::Serialize(const Shared<Scene>& scene, const std::filesystem::path& sceneFilePath)
{
    PFR_ASSERT(scene && !scene->m_Name.empty(), "Invalid scene!");
    PFR_ASSERT(!sceneFilePath.empty(), "Invalid scene save file path!");
    Timer t = {};

    // TODO: Add SceneDir ?
    const auto& appSpec                     = Application::Get().GetSpecification();
    std::filesystem::path sceneSaveFilePath = std::filesystem::path(appSpec.WorkingDir) / appSpec.AssetsDir / sceneFilePath;
    sceneSaveFilePath.replace_extension(".pfrscene");

    nlohmann::ordered_json json;
    json["Scene"] = scene->GetName();

    json.emplace("Entities", json.object());
    auto& entities_node = json["Entities"];
    scene->ForEach<IDComponent>(
        [&](const auto entityID, IDComponent& idComponent)
        {
            Entity entity(entityID, scene.get());
            SerializeEntity(entities_node, entity);
        });

    std::ofstream out(sceneSaveFilePath.string().data(), std::ios::out | std::ios::trunc);
    if (!out.is_open())
    {
        LOG_WARN("Failed to serialize scene! %s", sceneFilePath.string().data());
        return;
    }

    out << std::setw(2) << json << std::endl;
    out.close();

    LOG_TAG_TRACE(SCENE_MANAGER, "Time taken to serialize \"%s\", %f seconds.", sceneFilePath.string().data(), t.GetElapsedSeconds());
}

void SceneManager::Deserialize(Shared<Scene>& scene, const std::filesystem::path& sceneFilePath)
{
    PFR_ASSERT(!sceneFilePath.empty(), "Invalid scene save file path!");
    Timer t = {};

    // TODO: Add SceneDir ?
    const auto& appSpec                     = Application::Get().GetSpecification();
    std::filesystem::path sceneSaveFilePath = std::filesystem::path(appSpec.WorkingDir) / appSpec.AssetsDir / sceneFilePath;
    sceneSaveFilePath.replace_extension(".pfrscene");

    std::ifstream in(sceneSaveFilePath.string().data(), std::ios::in);
    if (!in.is_open())
    {
        LOG_WARN("Failed to open scene! %s", sceneFilePath.string().data());
        return;
    }

    const nlohmann::ordered_json json = nlohmann::json::parse(in);
    in.close();

    scene                   = MakeShared<Scene>(json["Scene"]);
    nlohmann::json entities = json["Entities"];
    for (auto& item : entities.items())
    {
        const nlohmann::json& node = item.value();

        const std::string tag = node["TagComponent"].get<std::string>();
        const uint64_t id     = std::stoull(item.key());
        Entity entity         = scene->CreateEntityWithUUID(id, tag);

        {
            auto& tc = entity.GetComponent<TransformComponent>();
            glm::from_json(node["TransformComponent"]["Translation"], tc.Translation);
            glm::from_json(node["TransformComponent"]["Rotation"], tc.Rotation);
            glm::from_json(node["TransformComponent"]["Scale"], tc.Scale);
        }

        // TODO: Add camera component
        // if (node.contains("CameraComponent"))

        if (node.contains("MeshComponent"))
        {
            const auto meshSource = node["MeshComponent"]["MeshSource"].get<std::string>();
            auto& mc              = entity.AddComponent<MeshComponent>(meshSource);

            if (node["MeshComponent"].contains("bDrawBoundingSphere"))
                mc.bDrawBoundingSphere = node["MeshComponent"]["bDrawBoundingSphere"].get<bool>();
        }

        if (node.contains("PointLightComponent"))
        {
            auto& plc = entity.AddComponent<PointLightComponent>();

            if (node["PointLightComponent"].contains("Color")) glm::from_json(node["PointLightComponent"]["Color"], plc.Color);
            if (node["PointLightComponent"].contains("Intensity")) plc.Intensity = node["PointLightComponent"]["Intensity"].get<float>();
            if (node["PointLightComponent"].contains("Radius")) plc.Radius = node["PointLightComponent"]["Radius"].get<float>();
            if (node["PointLightComponent"].contains("MinRadius")) plc.MinRadius = node["PointLightComponent"]["MinRadius"].get<float>();
            if (node["PointLightComponent"].contains("bCastShadows"))
                plc.bCastShadows = node["PointLightComponent"]["bCastShadows"].get<uint32_t>();
            if (node["PointLightComponent"].contains("bDrawBoundingSphere"))
                plc.bDrawBoundingSphere = node["PointLightComponent"]["bDrawBoundingSphere"].get<bool>();
        }

        if (node.contains("DirectionalLightComponent"))
        {
            auto& dlc = entity.AddComponent<DirectionalLightComponent>();

            if (node["DirectionalLightComponent"].contains("Color")) glm::from_json(node["DirectionalLightComponent"]["Color"], dlc.Color);

            if (node["DirectionalLightComponent"].contains("Intensity"))
                dlc.Intensity = node["DirectionalLightComponent"]["Intensity"].get<float>();
            if (node["DirectionalLightComponent"].contains("bCastShadows"))
                dlc.bCastShadows = node["DirectionalLightComponent"]["bCastShadows"].get<uint32_t>();
        }

        if (node.contains("SpotLightComponent"))
        {
            auto& slc = entity.AddComponent<SpotLightComponent>();

            if (node["SpotLightComponent"].contains("Direction")) glm::from_json(node["SpotLightComponent"]["Direction"], slc.Direction);
            if (node["SpotLightComponent"].contains("Color")) glm::from_json(node["SpotLightComponent"]["Color"], slc.Color);

            if (node["SpotLightComponent"].contains("Intensity")) slc.Intensity = node["SpotLightComponent"]["Intensity"].get<float>();
            if (node["SpotLightComponent"].contains("Height")) slc.Height = node["SpotLightComponent"]["Height"].get<float>();
            if (node["SpotLightComponent"].contains("Radius")) slc.Radius = node["SpotLightComponent"]["Radius"].get<float>();
            if (node["SpotLightComponent"].contains("InnerCutOff"))
                slc.InnerCutOff = node["SpotLightComponent"]["InnerCutOff"].get<float>();
            if (node["SpotLightComponent"].contains("OuterCutOff"))
                slc.OuterCutOff = node["SpotLightComponent"]["OuterCutOff"].get<float>();
            if (node["SpotLightComponent"].contains("bCastShadows"))
                slc.bCastShadows = node["SpotLightComponent"]["bCastShadows"].get<uint32_t>();
        }
    }

    LOG_TAG_TRACE(SCENE_MANAGER, "Time taken to deserialize \"%s\", %f seconds.", sceneFilePath.string().data(), t.GetElapsedSeconds());
}

}  // namespace Pathfinder