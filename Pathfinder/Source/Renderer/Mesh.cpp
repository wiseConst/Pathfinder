#include "PathfinderPCH.h"
#include "Mesh.h"

#include <fastgltf/parser.hpp>
#include <fastgltf/types.hpp>

#include <meshoptimizer.h>

namespace Pathfinder
{

Mesh::Mesh(const std::string& meshPath)
{
    // Creates a Parser instance. Optimally, you should reuse this across loads, but don't use it
    // across threads. To enable extensions, you have to pass them into the parser's constructor.
    fastgltf::Parser parser;

    // The GltfDataBuffer class is designed for re-usability of the same JSON string. It contains
    // utility functions to load data from a std::filesystem::path, copy from an existing buffer,
    // or re-use an already existing allocation. Note that it has to outlive the process of every
    // parsing function you call.
    fastgltf::GltfDataBuffer data;
    data.loadFromFile(meshPath);

    // This loads the glTF file into the gltf object and parses the JSON. For GLB files, use
    // Parser::loadBinaryGLTF instead.
    // You can detect the type of glTF using fastgltf::determineGltfFileType.
    // auto asset = parser.loadGltf(&data, path.parent_path(), fastgltf::Options::None);
    // if (auto error = asset.error(); error != fastgltf::Error::None)
    // {
    //     // Some error occurred while reading the buffer, parsing the JSON, or validating the data.
    // }

    // The glTF 2.0 asset is now ready to be used. Simply call asset.get(), asset.get_if() or
    // asset-> to get a direct reference to the Asset class. You can then access the glTF data
    // structures, like, for example, with buffers:
    // for (auto& buffer : asset->buffers)
    // {
    //     // Process the buffers.
    // }

    // Optionally, you can now also call the fastgltf::validate method. This will more strictly
    // enforce the glTF spec and is not needed most of the time, though I would certainly
    // recommend it in a development environment or when debugging to avoid mishaps.

    //fastgltf::validate(asset.get());
}

Mesh::~Mesh() {}

Shared<Mesh> Mesh::Create(const std::string& meshPath)
{
    return MakeShared<Mesh>(meshPath);
}

void Mesh::Destroy() {}

}  // namespace Pathfinder