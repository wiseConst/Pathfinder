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
    }

    if (entity.HasComponent<PointLightComponent>())
    {
        const auto& pl = entity.GetComponent<PointLightComponent>().pl;
        glm::to_json(node["PointLightComponent"]["Position"], pl.Position);
        glm::to_json(node["PointLightComponent"]["Color"], pl.Color);
        node["PointLightComponent"]["Intensity"]    = pl.Intensity;
        node["PointLightComponent"]["Radius"]       = pl.Radius;
        node["PointLightComponent"]["MinRadius"]    = pl.MinRadius;
        node["PointLightComponent"]["bCastShadows"] = pl.bCastShadows;
    }

    if (entity.HasComponent<DirectionalLightComponent>())
    {
        const auto& dl = entity.GetComponent<DirectionalLightComponent>().dl;
        glm::to_json(node["DirectionalLightComponent"]["Direction"], dl.Direction);
        glm::to_json(node["DirectionalLightComponent"]["Color"], dl.Color);
        node["DirectionalLightComponent"]["Intensity"]    = dl.Intensity;
        node["DirectionalLightComponent"]["bCastShadows"] = dl.bCastShadows;
    }

    if (entity.HasComponent<SpotLightComponent>())
    {
        const auto& sl = entity.GetComponent<SpotLightComponent>().sl;
        glm::to_json(node["SpotLightComponent"]["Position"], sl.Position);
        glm::to_json(node["SpotLightComponent"]["Direction"], sl.Direction);
        glm::to_json(node["SpotLightComponent"]["Color"], sl.Color);
        node["SpotLightComponent"]["Intensity"]   = sl.Intensity;
        node["SpotLightComponent"]["Height"]      = sl.Height;
        node["SpotLightComponent"]["Radius"]      = sl.Radius;
        node["SpotLightComponent"]["InnerCutOff"] = sl.InnerCutOff;
        node["SpotLightComponent"]["OuterCutOff"] = sl.OuterCutOff;
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
            entity.AddComponent<MeshComponent>(meshSource);
        }

        if (node.contains("PointLightComponent"))
        {
            auto& pl = entity.AddComponent<PointLightComponent>().pl;

            if (node["PointLightComponent"].contains("Position")) glm::from_json(node["PointLightComponent"]["Position"], pl.Position);
            if (node["PointLightComponent"].contains("Color")) glm::from_json(node["PointLightComponent"]["Color"], pl.Color);
            if (node["PointLightComponent"].contains("Intensity")) pl.Intensity = node["PointLightComponent"]["Intensity"].get<float>();
            if (node["PointLightComponent"].contains("Radius")) pl.Radius = node["PointLightComponent"]["Radius"].get<float>();
            if (node["PointLightComponent"].contains("MinRadius")) pl.MinRadius = node["PointLightComponent"]["MinRadius"].get<float>();
            if (node["PointLightComponent"].contains("bCastShadows"))
                pl.bCastShadows = node["PointLightComponent"]["bCastShadows"].get<uint32_t>();
        }

        if (node.contains("DirectionalLightComponent"))
        {
            auto& dl = entity.AddComponent<DirectionalLightComponent>().dl;

            if (node["DirectionalLightComponent"].contains("Direction"))
                glm::from_json(node["DirectionalLightComponent"]["Direction"], dl.Direction);

            if (node["DirectionalLightComponent"].contains("Color")) glm::from_json(node["DirectionalLightComponent"]["Color"], dl.Color);

            if (node["DirectionalLightComponent"].contains("Intensity"))
                dl.Intensity = node["DirectionalLightComponent"]["Intensity"].get<float>();
            if (node["DirectionalLightComponent"].contains("bCastShadows"))
                dl.bCastShadows = node["DirectionalLightComponent"]["bCastShadows"].get<uint32_t>();
        }

        if (node.contains("SpotLightComponent"))
        {
            auto& sl = entity.AddComponent<SpotLightComponent>().sl;

            if (node["SpotLightComponent"].contains("Position")) glm::from_json(node["SpotLightComponent"]["Position"], sl.Position);
            if (node["SpotLightComponent"].contains("Direction")) glm::from_json(node["SpotLightComponent"]["Direction"], sl.Direction);
            if (node["SpotLightComponent"].contains("Color")) glm::from_json(node["SpotLightComponent"]["Color"], sl.Color);

            if (node["SpotLightComponent"].contains("Intensity")) sl.Intensity = node["SpotLightComponent"]["Intensity"].get<float>();
            if (node["SpotLightComponent"].contains("Height")) sl.Height = node["SpotLightComponent"]["Height"].get<float>();
            if (node["SpotLightComponent"].contains("Radius")) sl.Radius = node["SpotLightComponent"]["Radius"].get<float>();
            if (node["SpotLightComponent"].contains("InnerCutOff")) sl.InnerCutOff = node["SpotLightComponent"]["InnerCutOff"].get<float>();
            if (node["SpotLightComponent"].contains("OuterCutOff")) sl.OuterCutOff = node["SpotLightComponent"]["OuterCutOff"].get<float>();
        }
    }

    LOG_TAG_TRACE(SCENE_MANAGER, "Time taken to deserialize \"%s\", %f seconds.", sceneFilePath.string().data(), t.GetElapsedSeconds());
}

}  // namespace Pathfinder