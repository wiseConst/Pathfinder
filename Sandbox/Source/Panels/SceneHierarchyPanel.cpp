#include "SceneHierarchyPanel.h"
#include "Pathfinder.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include "Scene/Scene.h"
#include "Scene/Entity.h"
#include "Scene/Components.h"

namespace Pathfinder
{
static void DrawVec3Control(const std::string& label, glm::vec3& values, const glm::vec2& range = glm::vec2(0.f),
                            const float resetValue = 0.0f, const float columnWidth = 100.0f)
{
    ImGui::PushID(label.data());
    ImGui::Columns(2);  // First for label, second for values

    ImGui::SetColumnWidth(0, columnWidth);  // Label width
    ImGui::Text(label.data());
    ImGui::NextColumn();

    ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{0.0f, 0.0f});

    const float LineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
    ImVec2 ButtonSize      = ImVec2{LineHeight + 3.0f, LineHeight};

    ImGui::PushStyleColor(ImGuiCol_Button, {0.8f, 0.1f, 0.15f, 1.0f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.9f, 0.2f, 0.2f, 1.0f});
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, {0.8f, 0.1f, 0.15f, 1.0f});

    if (ImGui::Button("X", ButtonSize)) values.x = resetValue;

    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    ImGui::DragFloat("##X", &values.x, 0.05f, range.x, range.y, "%.2f");
    ImGui::PopItemWidth();
    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_Button, {0.1f, 0.8f, 0.15f, 1.0f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.2f, 0.9f, 0.2f, 1.0f});
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, {0.1f, 0.8f, 0.15f, 1.0f});

    if (ImGui::Button("Y", ButtonSize)) values.y = resetValue;
    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    ImGui::DragFloat("##Y", &values.y, 0.05f, range.x, range.y, "%.2f");
    ImGui::PopItemWidth();
    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_Button, {0.15f, 0.1f, 0.8f, 1.0f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.2f, 0.2f, 0.9f, 1.0f});
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, {0.15f, 0.1f, 0.8f, 1.0f});

    if (ImGui::Button("Z", ButtonSize)) values.z = resetValue;
    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    ImGui::DragFloat("##Z", &values.z, 0.05f, range.x, range.y, "%.2f");
    ImGui::PopItemWidth();
    ImGui::SameLine();

    ImGui::PopStyleVar();
    ImGui::Columns(1);
    ImGui::PopID();
}

template <typename T, typename UIFunction> static void DrawComponent(const std::string& label, Entity entity, UIFunction&& uiFunction)
{
    if (!entity.HasComponent<T>()) return;

    constexpr auto TreeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth |
                                   ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowItemOverlap;

    const ImVec2 ContentRegionAvailable = ImGui::GetContentRegionAvail();

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{4, 4});
    const float LineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
    ImGui::Separator();
    const bool bIsOpened = ImGui::TreeNodeEx((void*)typeid(T).hash_code(), TreeNodeFlags, label.data());
    ImGui::PopStyleVar();

    ImGui::SameLine(ContentRegionAvailable.x - LineHeight * 0.5f);
    if (ImGui::Button("...", ImVec2{LineHeight, LineHeight})) ImGui::OpenPopup("ComponentSettings");

    bool bIsRemoved = false;
    if (ImGui::BeginPopup("ComponentSettings"))
    {
        if (ImGui::MenuItem("Remove Component")) bIsRemoved = true;

        ImGui::EndPopup();
    }

    if (bIsOpened)
    {
        auto& Component = entity.GetComponent<T>();
        uiFunction(Component);

        ImGui::TreePop();
    }

    if (bIsRemoved) entity.RemoveComponent<T>();
}

SceneHierarchyPanel::SceneHierarchyPanel(const Shared<Scene>& context) : m_Context(context) {}

void SceneHierarchyPanel::SetContext(const Shared<Scene>& context)
{
    m_Context          = context;
    m_SelectionContext = {};
}

void SceneHierarchyPanel::OnImGuiRender()
{
    ImGui::Begin("World Outliner");

    if (m_Context)
    {
        m_Context->ForEach<IDComponent>(
            [&](auto entityID, const auto& idComponent)
            {
                Entity entity{entityID, m_Context.get()};
                DrawEntityNode(entity);
            });

        // Deselection
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && ImGui::IsWindowHovered()) m_SelectionContext = {};

        // Second flag specifies that popup window should only be opened if only MB_Right
        // down in the blank space(nothing's hovered)
        if (ImGui::BeginPopupContextWindow(0, ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
        {
            if (ImGui::MenuItem("Create Empty Entity")) m_Context->CreateEntity("Empty Entity");

            ImGui::EndPopup();
        }
    }
    ImGui::End();

    ImGui::Begin("Details");
    if (m_SelectionContext.IsValid()) ShowComponents(m_SelectionContext);
    ImGui::End();
}

void SceneHierarchyPanel::DrawEntityNode(Entity entity)
{
    auto& tag = entity.GetComponent<TagComponent>();

    ImGuiTreeNodeFlags SelectionFlag = m_SelectionContext == entity ? ImGuiTreeNodeFlags_Selected : 0;
    const ImGuiTreeNodeFlags flags   = SelectionFlag | (ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth);
    bool bIsOpened                   = ImGui::TreeNodeEx((void*)(uint64_t)entity, flags, tag.Tag.data());

    if (ImGui::IsItemClicked())
    {
        m_SelectionContext = entity;
    }

    bool bIsEntityDeleted{false};
    if (ImGui::BeginPopupContextItem())
    {
        if (ImGui::MenuItem("Delete Entity")) bIsEntityDeleted = true;

        ImGui::EndPopup();
    }

    if (bIsOpened)
    {
        // Recursive open childs here
        ImGui::TreePop();
    }

    if (bIsEntityDeleted)
    {
        m_Context->DestroyEntity(entity);
        if (m_SelectionContext == entity) m_SelectionContext = {};
    }
}

void SceneHierarchyPanel::ShowComponents(Entity entity)
{
    if (entity.HasComponent<TagComponent>())
    {
        auto& tag = entity.GetComponent<TagComponent>().Tag;

        char name[256] = {0};
        strcpy_s(name, sizeof(name), tag.data());

        if (ImGui::InputText("##Tag", name, sizeof(name)))
        {
            tag = std::string(name);
        }
    }

    ImGui::PushItemWidth(-1.0f);
    ImGui::SameLine();

    if (ImGui::Button("Add Component")) ImGui::OpenPopup("AddComponent");

    if (ImGui::BeginPopup("AddComponent"))
    {
        if (ImGui::MenuItem("Sprite"))
        {
            m_SelectionContext.AddComponent<SpriteComponent>();
            ImGui::CloseCurrentPopup();
        }

        if (ImGui::MenuItem("Mesh"))
        {
            m_SelectionContext.AddComponent<MeshComponent>();
            ImGui::CloseCurrentPopup();
        }

        if (ImGui::MenuItem("Point Light"))
        {
            m_SelectionContext.AddComponent<PointLightComponent>();
            ImGui::CloseCurrentPopup();
        }

        if (ImGui::MenuItem("Directional Light"))
        {
            m_SelectionContext.AddComponent<DirectionalLightComponent>();
            ImGui::CloseCurrentPopup();
        }

        if (ImGui::MenuItem("Spot Light"))
        {
            m_SelectionContext.AddComponent<SpotLightComponent>();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
    ImGui::PopItemWidth();

    DrawComponent<TransformComponent>("TransformComponent", entity,
                                      [&](auto& tc)
                                      {
                                          // TODO: For DirectionalLights replace Translation(used as direction) for Rotation(better to
                                          // understand).
                                          if (entity.HasComponent<DirectionalLightComponent>())
                                          {
                                              const glm::vec2 range{-1.0f, 1.0f};
                                              DrawVec3Control("Translation", tc.Translation, range);
                                          }
                                          else
                                              DrawVec3Control("Translation", tc.Translation);

                                          DrawVec3Control("Rotation", tc.Rotation);
                                          DrawVec3Control("Scale", tc.Scale);
                                      });

    DrawComponent<PointLightComponent>("PointLightComponent", entity,
                                       [](auto& plc)
                                       {
                                           ImGui::Separator();
                                           ImGui::Checkbox("DrawBoundingSphere", &plc.bDrawBoundingSphere);

                                           ImGui::Separator();
                                           ImGui::Text("Color");
                                           ImGui::ColorPicker3("Color", (float*)&plc.Color);

                                           ImGui::Separator();
                                           ImGui::SliderFloat("Intensity", &plc.Intensity, 0.0f, 100.0f, "%.2f");

                                           ImGui::Separator();
                                           ImGui::SliderFloat("Radius", &plc.Radius, 0.0f, 100.0f, "%.2f");

                                           ImGui::Separator();
                                           ImGui::SliderFloat("MinRadius", &plc.MinRadius, 0.0f, 100.0f, "%.2f");

                                           ImGui::Separator();
                                           ImGui::Checkbox("CastShadows", (bool*)&plc.bCastShadows);
                                       });

    DrawComponent<DirectionalLightComponent>("DirectionalLightComponent", entity,
                                             [](auto& dlc)
                                             {
                                                 ImGui::Separator();
                                                 ImGui::Text("Color");
                                                 ImGui::ColorPicker3("Color", (float*)&dlc.Color);

                                                 ImGui::Separator();
                                                 ImGui::SliderFloat("Intensity", &dlc.Intensity, 0.0f, 100.0f, "%.2f");

                                                 ImGui::Separator();
                                                 ImGui::Checkbox("CastShadows", (bool*)&dlc.bCastShadows);
                                             });

    DrawComponent<SpotLightComponent>("SpotLightComponent", entity,
                                      [](auto& slc)
                                      {
                                          DrawVec3Control("Direction", slc.Direction);

                                          ImGui::Separator();
                                          ImGui::Text("Color");
                                          ImGui::ColorPicker3("Color", (float*)&slc.Color);

                                          ImGui::Separator();
                                          ImGui::SliderFloat("Intensity", &slc.Intensity, 0.0f, 100.0f, "%.2f");

                                          ImGui::Separator();
                                          ImGui::SliderFloat("Height", &slc.Height, 0.0f, 100.0f, "%.2f");

                                          ImGui::Separator();
                                          ImGui::SliderFloat("Radius", &slc.Radius, 0.0f, 100.0f, "%.2f");

                                          ImGui::Separator();
                                          ImGui::SliderFloat("InnerCutOff", &slc.InnerCutOff, 0.0f, 100.0f, "%.2f");

                                          ImGui::Separator();
                                          ImGui::SliderFloat("OuterCutOff", &slc.OuterCutOff, 0.0f, 100.0f, "%.2f");

                                          ImGui::Separator();
                                          ImGui::Checkbox("CastShadows", (bool*)&slc.bCastShadows);
                                      });

    DrawComponent<MeshComponent>("MeshComponent", entity,
                                 [](auto& mc)
                                 {
                                     uint32_t submeshIndex = 0;
                                     for (auto& submesh : mc.Mesh->GetSubmeshes())
                                     {
                                         ImGui::Separator();

                                         const auto submeshStr = "Submesh[" + std::to_string(submeshIndex++) + "]";
                                         ImGui::PushID(submeshStr.data());

                                         ImGui::Text("%s", submeshStr.data());

                                         ImGui::Separator();
                                         ImGui::Checkbox("DrawBoundingSphere", &mc.bDrawBoundingSphere);

                                         const auto& mat = submesh->GetMaterial();
                                         static const glm::vec2 imageSize{256.0f, 256.0f};

                                         PBRData& materialData    = mat->GetPBRData();
                                         bool bIsAnythingAdjusted = false;

                                         ImGui::Text("PBR Properties");
                                         if (ImGui::ColorEdit4("BaseColor", (float*)&materialData.BaseColor) ||
                                             ImGui::SliderFloat("Metallic", &materialData.Metallic, 0.0f, 1.0f) ||
                                             ImGui::SliderFloat("Roughness", &materialData.Roughness, 0.0f, 1.0f))
                                             bIsAnythingAdjusted = true;

                                         static constexpr auto DrawTexture =
                                             [](const Shared<Texture>& texture, const std::string_view& imageName)
                                         {
                                             if (!texture) return;

                                             ImGui::Text("%s", imageName.data());
                                             UILayer::DrawTexture(texture, imageSize, {0, 0}, {1, 1});
                                             ImGui::Separator();
                                         };

                                         DrawTexture(mat->GetAlbedo(), "Albedo");
                                         DrawTexture(mat->GetNormalMap(), "NormalMap");
                                         DrawTexture(mat->GetMetallicRoughness(), "MetallicRoughness");
                                         DrawTexture(mat->GetEmissiveMap(), "EmissiveMap");
                                         DrawTexture(mat->GetAOMap(), "AO");

                                         if (bIsAnythingAdjusted) mat->Update();

                                         ImGui::PopID();
                                     }
                                 });

    DrawComponent<SpriteComponent>("SpriteComponent", entity,
                                   [](auto& sc)
                                   {
                                       ImGui::Separator();
                                       ImGui::Text("Color");
                                       ImGui::ColorPicker4("Color", (float*)&sc.Color);

                                       ImGui::Separator();
                                       ImGui::SliderInt("Layer", (int32_t*)&sc.Layer, 0, 5);

                                       if (sc.Texture)
                                       {
                                           ImGui::Separator();
                                           const auto& textureSpec = sc.Texture->GetSpecification();
                                           UILayer::DrawTexture(sc.Texture, {textureSpec.Width, textureSpec.Height}, {0, 0}, {1, 1});
                                       }
                                   });
}

}  // namespace Pathfinder