#include "PathfinderPCH.h"
#include "VulkanRayTracingBuilder.h"

#include "VulkanContext.h"
#include "VulkanDevice.h"
#include "VulkanBuffer.h"
#include "VulkanCommandBuffer.h"

#include "Renderer/Material.h"
#include "Renderer/Mesh/Submesh.h"
#include "Renderer/Renderer.h"

#include "Core/Threading.h"

namespace Pathfinder
{

std::vector<AccelerationStructure> VulkanRayTracingBuilder::BuildBLASesImpl(const std::vector<Shared<Submesh>>& submeshes)
{
    const auto& context = VulkanContext::Get();

    std::vector<BLASInput> blasInput;
    for (auto& submesh : submeshes)
    {
        VkDeviceAddress vertexBufferAddress =
            context.GetDevice()->GetBufferDeviceAddress((VkBuffer)submesh->GetVertexPositionBuffer()->Get());
        VkDeviceAddress indexBufferAddress = context.GetDevice()->GetBufferDeviceAddress((VkBuffer)submesh->GetIndexBuffer()->Get());

        // Specify where the builder can find the vertices and indices for triangles, and their formats:
        VkAccelerationStructureGeometryTrianglesDataKHR triangles = {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR};
        triangles.vertexFormat                                    = VK_FORMAT_R32G32B32_SFLOAT;
        triangles.vertexData.deviceAddress                        = vertexBufferAddress;
        triangles.vertexStride                                    = sizeof(MeshPositionVertex);
        triangles.maxVertex =
            static_cast<uint32_t>(submesh->GetVertexPositionBuffer()->GetSpecification().BufferCapacity / sizeof(MeshPositionVertex) - 1);
        triangles.indexType                   = VK_INDEX_TYPE_UINT32;
        triangles.indexData.deviceAddress     = indexBufferAddress;
        triangles.transformData.deviceAddress = 0;  // No transform

        // this object that says it handles opaque triangles and points to the above:
        VkAccelerationStructureGeometryKHR geometry = {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
        geometry.geometry.triangles                 = triangles;
        geometry.geometryType                       = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        geometry.flags                              = submesh->GetMaterial()->IsOpaque() ? VK_GEOMETRY_OPAQUE_BIT_KHR : 0;

        // Create offset info that allows us to say how many triangles and vertices to read
        VkAccelerationStructureBuildRangeInfoKHR offsetInfo = {};
        offsetInfo.firstVertex                              = 0;
        offsetInfo.primitiveCount =
            static_cast<uint32_t>(submesh->GetIndexBuffer()->GetSpecification().BufferCapacity / sizeof(uint32_t) /
                                  3);  // Number of triangles offsetInfo.primitiveOffset = 0; offsetInfo.transformOffset = 0;

        BLASInput input = {};
        input.GeometryData.emplace_back(geometry);
        input.OffsetInfo.emplace_back(offsetInfo);

        blasInput.emplace_back(input);
    }

    // Building blases
    // m_cmdPool.init(m_device, m_queueIndex);
    uint32_t nbBlas = static_cast<uint32_t>(blasInput.size());
    VkDeviceSize asTotalSize{0};     // Memory size of all allocated BLAS
    uint32_t nbCompactions{0};       // Nb of BLAS requesting compaction
    VkDeviceSize maxScratchSize{0};  // Largest scratch size

    struct BuildAccelerationStructure
    {
        VkAccelerationStructureBuildGeometryInfoKHR buildInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
        VkAccelerationStructureBuildSizesInfoKHR sizeInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
        const VkAccelerationStructureBuildRangeInfoKHR* rangeInfo;
        AccelerationStructure as;  // result acceleration structure
        AccelerationStructure cleanupAS;
    };

    auto& logicalDevice = context.GetDevice()->GetLogicalDevice();
    // Preparing the information for the acceleration build commands.
    std::vector<BuildAccelerationStructure> buildAs(nbBlas);
    for (uint32_t idx = 0; idx < nbBlas; ++idx)
    {
        // Filling partially the VkAccelerationStructureBuildGeometryInfoKHR for querying the build sizes.
        // Other information will be filled in the createBlas (see #2)
        buildAs[idx].buildInfo.type  = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        buildAs[idx].buildInfo.mode  = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        buildAs[idx].buildInfo.flags = /*blasInput[idx].flags |*/ VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR |
                                       VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;  //| flags;
        buildAs[idx].buildInfo.geometryCount = static_cast<uint32_t>(blasInput[idx].GeometryData.size());
        buildAs[idx].buildInfo.pGeometries   = blasInput[idx].GeometryData.data();

        // Build range information
        buildAs[idx].rangeInfo = blasInput[idx].OffsetInfo.data();

        // Finding sizes to create acceleration structures and scratch
        std::vector<uint32_t> maxPrimCount(blasInput[idx].OffsetInfo.size());
        for (auto tt = 0; tt < blasInput[idx].OffsetInfo.size(); tt++)
            maxPrimCount[tt] = blasInput[idx].OffsetInfo[tt].primitiveCount;  // Number of primitives/triangles
        context.GetDevice()->GetASBuildSizes(VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildAs[idx].buildInfo, maxPrimCount.data(),
                                             &buildAs[idx].sizeInfo);

        // Extra info
        asTotalSize += buildAs[idx].sizeInfo.accelerationStructureSize;
        maxScratchSize = std::max(maxScratchSize, buildAs[idx].sizeInfo.buildScratchSize);
        nbCompactions += (buildAs[idx].buildInfo.flags & VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR) ==
                         VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;
    }

    // Allocate a query pool for storing the needed size for every BLAS compaction.
    VkQueryPool queryPool{VK_NULL_HANDLE};
    if (nbCompactions > 0)  // Is compaction requested?
    {
        assert(nbCompactions == nbBlas);  // Don't allow mix of on/off compaction
        VkQueryPoolCreateInfo qpci{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
        qpci.queryCount = nbBlas;
        qpci.queryType  = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR;
        vkCreateQueryPool(logicalDevice, &qpci, nullptr, &queryPool);
    }

    // Batching creation/compaction of BLAS to allow staying in restricted amount of memory
    std::vector<uint32_t> indices;  // Indices of the BLAS to create
    VkDeviceSize batchSize{0};
    VkDeviceSize batchLimit{256'000'000};  // 256 MB

    BufferSpecification sbSpec = {};
    sbSpec.BufferCapacity      = batchLimit;
    sbSpec.BufferUsage         = EBufferUsage::BUFFER_USAGE_SHADER_DEVICE_ADDRESS | EBufferUsage::BUFFER_USAGE_STORAGE;
    auto scratchBuffer         = Buffer::Create(sbSpec);

    VkDeviceAddress scratchBufferAddress = context.GetDevice()->GetBufferDeviceAddress((VkBuffer)scratchBuffer->Get());
    for (uint32_t i = 0; i < nbBlas; ++i)
    {
        indices.push_back(i);
        batchSize += buildAs[i].sizeInfo.accelerationStructureSize;
        // Over the limit or last BLAS element
        if (batchSize >= batchLimit || i == nbBlas - 1)
        {
            const CommandBufferSpecification cbSpec = {
                ECommandBufferType::COMMAND_BUFFER_TYPE_GRAPHICS, ECommandBufferLevel::COMMAND_BUFFER_LEVEL_PRIMARY,
                Renderer::GetRendererData()->FrameIndex, JobSystem::MapThreadID(JobSystem::GetMainThreadID())};
            auto vkCmdBuf = MakeShared<VulkanCommandBuffer>(cbSpec);
            vkCmdBuf->BeginRecording(true);

            //  VkCommandBuffer cmdBuf = m_cmdPool.createCommandBuffer();
            //    cmdCreateBlas(cmdBuf, indices, buildAs, scratchAddress, queryPool);
            {
                if (queryPool)  // For querying the compaction size
                    vkResetQueryPool(logicalDevice, queryPool, 0, static_cast<uint32_t>(indices.size()));
                uint32_t queryCnt{0};

                for (const auto& idx : indices)
                {  // Actual allocation of buffer and acceleration structure.
                    VkAccelerationStructureCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
                    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
                    createInfo.size = buildAs[idx].sizeInfo.accelerationStructureSize;  // Will be used to allocate memory.

                    BufferSpecification abSpec = {};
                    abSpec.BufferCapacity      = createInfo.size;
                    abSpec.BufferUsage =
                        EBufferUsage::BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE | EBufferUsage::BUFFER_USAGE_SHADER_DEVICE_ADDRESS;
                    buildAs[idx].as.Buffer = Buffer::Create(abSpec);
                    createInfo.buffer      = (VkBuffer)buildAs[idx].as.Buffer->Get();
                    VK_CHECK(vkCreateAccelerationStructureKHR(logicalDevice, &createInfo, nullptr,
                                                              (VkAccelerationStructureKHR*)&buildAs[idx].as.Handle),
                             "Failed to create acceleration structure!");

                    // BuildInfo #2 part
                    buildAs[idx].buildInfo.dstAccelerationStructure =
                        (VkAccelerationStructureKHR)buildAs[idx].as.Handle;                   // Setting where the build lands
                    buildAs[idx].buildInfo.scratchData.deviceAddress = scratchBufferAddress;  // All build are using the same scratch buffer

                    // Building the bottom-level-acceleration-structure
                    vkCmdBuf->BuildAccelerationStructure(1, &buildAs[idx].buildInfo, &buildAs[idx].rangeInfo);

                    // Since the scratch buffer is reused across builds, we need a barrier to ensure one build
                    // is finished before starting the next one.
                    VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
                    barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
                    barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
                    vkCmdBuf->InsertBarrier(VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                                            VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &barrier, 0, nullptr, 0, nullptr);

                    if (queryPool)
                    {
                        // Add a query to find the 'real' amount of memory needed, use for compaction
                        vkCmdWriteAccelerationStructuresPropertiesKHR(
                            (VkCommandBuffer)vkCmdBuf->Get(), 1, &buildAs[idx].buildInfo.dstAccelerationStructure,
                            VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, queryPool, queryCnt++);
                    }
                }
            }

            vkCmdBuf->EndRecording();
            vkCmdBuf->Submit(true, false);
            //         m_cmdPool.submitAndWait(cmdBuf);

            if (queryPool)
            {
                const CommandBufferSpecification cbSpec = {
                    ECommandBufferType::COMMAND_BUFFER_TYPE_GRAPHICS, ECommandBufferLevel::COMMAND_BUFFER_LEVEL_PRIMARY,
                    Renderer::GetRendererData()->FrameIndex, JobSystem::MapThreadID(JobSystem::GetMainThreadID())};
                auto vkCmdBuf = MakeShared<VulkanCommandBuffer>(cbSpec);
                vkCmdBuf->BeginRecording(true);

                //  cmdCompactBlas(cmdBuf, indices, buildAs, queryPool);
                //   m_cmdPool.submitAndWait(cmdBuf);  // Submit command buffer and call vkQueueWaitIdle

                uint32_t queryCtn{0};
                std::vector<AccelerationStructure> cleanupAS;  // previous AS to destroy

                // Get the compacted size result back
                std::vector<VkDeviceSize> compactSizes(static_cast<uint32_t>(indices.size()));
                vkGetQueryPoolResults(logicalDevice, queryPool, 0, (uint32_t)compactSizes.size(),
                                      compactSizes.size() * sizeof(VkDeviceSize), compactSizes.data(), sizeof(VkDeviceSize),
                                      VK_QUERY_RESULT_WAIT_BIT);

                for (auto idx : indices)
                {
                    buildAs[idx].cleanupAS                          = buildAs[idx].as;           // previous AS to destroy
                    buildAs[idx].sizeInfo.accelerationStructureSize = compactSizes[queryCtn++];  // new reduced size

                    // Creating a compact version of the AS
                    VkAccelerationStructureCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
                    createInfo.size = buildAs[idx].sizeInfo.accelerationStructureSize;
                    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

                    BufferSpecification abSpec = {};
                    abSpec.BufferCapacity      = createInfo.size;
                    abSpec.BufferUsage =
                        EBufferUsage::BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE | EBufferUsage::BUFFER_USAGE_SHADER_DEVICE_ADDRESS;
                    buildAs[idx].as.Buffer = Buffer::Create(abSpec);
                    createInfo.buffer      = (VkBuffer)buildAs[idx].as.Buffer->Get();
                    VK_CHECK(vkCreateAccelerationStructureKHR(logicalDevice, &createInfo, nullptr,
                                                              (VkAccelerationStructureKHR*)&buildAs[idx].as.Handle),
                             "Failed to create acceleration structure!");

                    // Copy the original BLAS to a compact version
                    VkCopyAccelerationStructureInfoKHR copyInfo{VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR};
                    copyInfo.src  = buildAs[idx].buildInfo.dstAccelerationStructure;
                    copyInfo.dst  = (VkAccelerationStructureKHR)buildAs[idx].as.Handle;
                    copyInfo.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR;
                    vkCmdCopyAccelerationStructureKHR((VkCommandBuffer)vkCmdBuf->Get(), &copyInfo);
                }

                vkCmdBuf->EndRecording();
                vkCmdBuf->Submit(true, false);
            }

            // Destroy the non-compacted version
            // destroyNonCompacted(indices, buildAs);
            for (auto& i : indices)
            {
                vkDestroyAccelerationStructureKHR(logicalDevice, (VkAccelerationStructureKHR)buildAs[i].cleanupAS.Handle, nullptr);
                buildAs[i].cleanupAS.Buffer.reset();
            }
        }
        // Reset

        batchSize = 0;
        indices.clear();
    }

    // Keeping all the created acceleration structures

    std::vector<AccelerationStructure> blasesOut;
    for (auto& b : buildAs)
    {
        if (!b.as.Handle) continue;

        blasesOut.emplace_back(b.as);
    }

    vkDestroyQueryPool(logicalDevice, queryPool, nullptr);

    return blasesOut;
}

AccelerationStructure VulkanRayTracingBuilder::BuildTLASImpl(const std::vector<AccelerationStructure>& blases)
{
    const auto& context       = VulkanContext::Get();
    const auto& logicalDevice = context.GetDevice()->GetLogicalDevice();

    AccelerationStructure builtTLAS = {};

    // build tlas
    std::vector<VkAccelerationStructureInstanceKHR> tlas;
    for (size_t i{}; i < blases.size(); ++i)
    {
        VkAccelerationStructureInstanceKHR rayInst{};
        rayInst.transform.matrix[0][0] = rayInst.transform.matrix[1][1] = rayInst.transform.matrix[2][2] =
            1.0f;                         // Position of the instance
        rayInst.instanceCustomIndex = i;  // gl_InstanceCustomIndexEXT

        rayInst.accelerationStructureReference =
            context.GetDevice()->GetAccelerationStructureAddress((VkAccelerationStructureKHR)blases[i].Handle);
        rayInst.flags                                  = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        rayInst.mask                                   = 0xFF;  //  Only be hit if rayMask & instance.mask != 0
        rayInst.instanceShaderBindingTableRecordOffset = 0;     // We will use the same hit group for all objects
        tlas.emplace_back(rayInst);
    }

    {
        // create tlas
        // Cannot call buildTlas twice except to update.
        // assert(m_tlas.accel == VK_NULL_HANDLE || update);
        uint32_t countInstance = static_cast<uint32_t>(blases.size());

        const CommandBufferSpecification cbSpec = {
            ECommandBufferType::COMMAND_BUFFER_TYPE_GRAPHICS, ECommandBufferLevel::COMMAND_BUFFER_LEVEL_PRIMARY,
            Renderer::GetRendererData()->FrameIndex, JobSystem::MapThreadID(JobSystem::GetMainThreadID())};
        auto vkCmdBuf = MakeShared<VulkanCommandBuffer>(cbSpec);
        vkCmdBuf->BeginRecording(true);

        // Command buffer to create the TLAS
        //  nvvk::CommandPool genCmdBuf(m_device, m_queueIndex);
        // VkCommandBuffer cmdBuf = genCmdBuf.createCommandBuffer();

        BufferSpecification ibSpec = {};
        ibSpec.BufferUsage =
            EBufferUsage::BUFFER_USAGE_SHADER_DEVICE_ADDRESS | EBufferUsage::BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY;
        ibSpec.BufferCapacity = sizeof(VkAccelerationStructureInstanceKHR) * blases.size();
        // Create a buffer holding the actual instance data (matrices++) for use by the AS builder
        auto instancesBuffer = Buffer::Create(ibSpec);  // Buffer of instances containing the matrices and BLAS ids

        VkBufferDeviceAddressInfo instanceBufferInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr,
                                                     (VkBuffer)instancesBuffer->Get()};
        VkDeviceAddress instBufferAddr = vkGetBufferDeviceAddress(logicalDevice, &instanceBufferInfo);

        // Make sure the copy of the instance buffer are copied before triggering the acceleration structure build
        VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        vkCmdPipelineBarrier((VkCommandBuffer)vkCmdBuf->Get(), VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &barrier, 0, nullptr, 0, nullptr);

        // Creating the TLAS

        // Wraps a device pointer to the above uploaded instances.
        VkAccelerationStructureGeometryInstancesDataKHR instancesVk{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR};
        instancesVk.data.deviceAddress = instBufferAddr;

        // Put the above into a VkAccelerationStructureGeometryKHR. We need to put the instances struct in a union and label it as instance
        // data.
        VkAccelerationStructureGeometryKHR topASGeometry{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
        topASGeometry.geometryType       = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        topASGeometry.geometry.instances = instancesVk;

        // Find sizes
        VkAccelerationStructureBuildGeometryInfoKHR buildInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
        buildInfo.flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;  // flags;
        buildInfo.geometryCount = 1;
        buildInfo.pGeometries   = &topASGeometry;
        bool update             = false;  // animation
        buildInfo.mode          = update ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        buildInfo.type          = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        buildInfo.srcAccelerationStructure = VK_NULL_HANDLE;

        VkAccelerationStructureBuildSizesInfoKHR sizeInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
        vkGetAccelerationStructureBuildSizesKHR(logicalDevice, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &countInstance,
                                                &sizeInfo);
        VkAccelerationStructureCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
        createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        createInfo.size = sizeInfo.accelerationStructureSize;

        BufferSpecification abSpec = {};
        abSpec.BufferCapacity      = createInfo.size;
        abSpec.BufferUsage = EBufferUsage::BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE | EBufferUsage::BUFFER_USAGE_SHADER_DEVICE_ADDRESS;
        builtTLAS.Buffer   = Buffer::Create(abSpec);
        createInfo.buffer  = (VkBuffer)builtTLAS.Buffer->Get();
        VK_CHECK(vkCreateAccelerationStructureKHR(logicalDevice, &createInfo, nullptr, (VkAccelerationStructureKHR*)&builtTLAS.Handle),
                 "Failed to create acceleration structure!");

        BufferSpecification tlasSbSpec = {};
        tlasSbSpec.BufferUsage         = EBufferUsage::BUFFER_USAGE_STORAGE | EBufferUsage::BUFFER_USAGE_SHADER_DEVICE_ADDRESS;
        tlasSbSpec.BufferCapacity      = sizeInfo.buildScratchSize;

        Shared<Buffer> tlasScratchBuffer = Buffer::Create(tlasSbSpec);
        VkBufferDeviceAddressInfo tlasBufferInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, (VkBuffer)tlasScratchBuffer->Get()};
        VkDeviceAddress scratchAddress = vkGetBufferDeviceAddress(logicalDevice, &tlasBufferInfo);

        //  cmdCreateTlas(cmdBuf, countInstance, instBufferAddr, scratchBuffer, flags, update, motion);

        // Update build information
        buildInfo.srcAccelerationStructure  = VK_NULL_HANDLE;
        buildInfo.dstAccelerationStructure  = (VkAccelerationStructureKHR)builtTLAS.Handle;
        buildInfo.scratchData.deviceAddress = scratchAddress;

        // Build Offsets info: n instances
        VkAccelerationStructureBuildRangeInfoKHR buildOffsetInfo{countInstance, 0, 0, 0};
        const VkAccelerationStructureBuildRangeInfoKHR* pBuildOffsetInfo = &buildOffsetInfo;

        // Build the TLAS
        vkCmdBuf->BuildAccelerationStructure(1, &buildInfo, &pBuildOffsetInfo);

        // Finalizing and destroying temporary data
        vkCmdBuf->EndRecording();
        vkCmdBuf->Submit(true, false);
    }

    return builtTLAS;
}

void VulkanRayTracingBuilder::DestroyAccelerationStructureImpl(AccelerationStructure& as)
{
    VulkanContext::Get().GetDevice()->WaitDeviceOnFinish();

    vkDestroyAccelerationStructureKHR(VulkanContext::Get().GetDevice()->GetLogicalDevice(), (VkAccelerationStructureKHR)as.Handle, nullptr);
    as.Buffer.reset();
}

}  // namespace Pathfinder
