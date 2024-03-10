#include "PathfinderPCH.h"
#include "RenderGraphBuilder.h"
#include "RenderGraph.h"

#include <simdjson.h>

namespace Pathfinder
{

namespace RenderGraphUtils
{

static ERenderGraphResourceType StringToResourceType(const std::string& type)
{
    if (strcmp(type.data(), "texture") == 0)
        return ERenderGraphResourceType::RENDER_GRAPH_RESOURCE_TYPE_TEXTURE;
    else if (strcmp(type.data(), "attachment") == 0)
        return ERenderGraphResourceType::RENDER_GRAPH_RESOURCE_TYPE_ATTACHMENT;
    else if (strcmp(type.data(), "buffer") == 0)
        return ERenderGraphResourceType::RENDER_GRAPH_RESOURCE_TYPE_BUFFER;
    else if (strcmp(type.data(), "reference") == 0)
    {
        // This is used for resources that need to create an edge but are not actually
        // used by the render pass
        return ERenderGraphResourceType::RENDER_GRAPH_RESOURCE_TYPE_REFERENCE;
    }

    PFR_ASSERT(false, "Unknown render graph resource type!");
    return ERenderGraphResourceType::RENDER_GRAPH_RESOURCE_TYPE_REFERENCE;
}

}  // namespace RenderGraphUtils

Unique<RenderGraph> RenderGraphBuilder::Create(const std::filesystem::path& renderGraphSpecificationPath)
{
    PFR_ASSERT(!renderGraphSpecificationPath.empty(), "Invalid render graph specification path!");

    using namespace simdjson;
    ondemand::parser parser;
    const auto parsedJson  = padded_string::load(renderGraphSpecificationPath.string());
    ondemand::document doc = parser.iterate(parsedJson);

    std::string debugName = s_DEFAULT_STRING;
    if (auto nameField = doc.find_field("name"); !nameField.is_null())
    {
        debugName = std::string(nameField.get_string().value());
        PFR_ASSERT(!debugName.empty(), "Node name is empty!");
    }
    auto rg = MakeUnique<RenderGraph>(debugName);

    auto passesField = doc.find_field("passes");
    PFR_ASSERT(!passesField.is_null(), "Passes field is invalid!");

    auto passesArr = passesField.get_array();
    PFR_ASSERT(!passesArr.is_empty(), "Passes array is empty!");

#if 0
    const auto passCount = passesArr.count_elements().value();

    for (size_t passIdx{}; passIdx < passCount; ++passIdx)
    {
        auto inputs  = passesArr.at(passIdx)["inputs"].get_array();
        auto outputs = passesArr.at(passIdx)["outputs"].get_array();

        //   const std::string nodeName = std::string(passesArr.at(passIdx)["name"].get_c_str());
        RenderGraphNode rgNode = {/*nodeName*/};

        for (auto input : inputs)
        {
      //      auto td = input.at(1);

            auto typeField = input.find_field_unordered("type");
            auto nameField = input.find_field_unordered("name");

            LOG_TRACE("%s", input.get_raw_json_string().value().raw());
            //    LOG_TRACE("%s", input["name"].get_string().value().data());
        }

        /* for (sizet ii = 0; ii < pass_inputs.size(); ++ii)
         {
             json pass_input = pass_inputs[ii];

             FrameGraphResourceInputCreation input_creation{};

             std::string input_type = pass_input.value("type", "");
             RASSERT(!input_type.empty());

             std::string input_name = pass_input.value("name", "");
             RASSERT(!input_name.empty());

             input_creation.type                   = string_to_resource_type(input_type.c_str());
             input_creation.resource_info.external = false;

             input_creation.name = string_buffer.append_use_f("%s", input_name.c_str());

             node_creation.inputs.push(input_creation);
         }*/

        rg->AddNode(rgNode);
        // auto d = arr.at(passIdx)["name"];
        // auto outputs = arr.at(passIdx)["outputs"];

        // LOG_TRACE("%s", inputs.at(0)["name"].get_string().value().data());
        // LOG_TRACE("%s", outputs.at(0)["name"].get_string().value().data());
        // LOG_TRACE("%s", arr.at(passIdx)["name"].get_string().value().data());
    }
#endif

    return rg;
}

}  // namespace Pathfinder