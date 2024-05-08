#include "PathfinderPCH.h"
#include "Submesh.h"

namespace Pathfinder
{

void Submesh::Destroy()
{
    m_VertexPositionBuffer.reset();
    m_VertexAttributeBuffer.reset();
    m_IndexBuffer.reset();

    m_MeshletBuffer.reset();
    m_MeshletVerticesBuffer.reset();
    m_MeshletTrianglesBuffer.reset();
}

}  // namespace Pathfinder