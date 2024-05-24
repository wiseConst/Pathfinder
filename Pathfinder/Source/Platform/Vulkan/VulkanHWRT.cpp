#include <PathfinderPCH.h>
#include "VulkanHWRT.h"

#include "VulkanContext.h"
#include "VulkanDevice.h"
#include "VulkanBuffer.h"
#include "VulkanCommandBuffer.h"

#include "Renderer/Material.h"
#include "Renderer/Mesh/Mesh.h"
#include "Renderer/Mesh/Submesh.h"
#include "Renderer/Renderer.h"


namespace Pathfinder
{

std::vector<AccelerationStructure> VulkanRayTracingBuilder::BuildBLASesImpl(const std::vector<Shared<Mesh>>& meshes)
{
    const auto& context = VulkanContext::Get();
    const auto& device  = context.GetDevice();

    std::vector<BLASInput> blasInput;
    for (auto& mesh : meshes)
    {
        for (auto& submesh : mesh->GetSubmeshes())
        {
            auto& input                         = blasInput.emplace_back();
            VkDeviceAddress vertexBufferAddress = device->GetBufferDeviceAddress((VkBuffer)submesh->GetVertexPositionBuffer()->Get());
            VkDeviceAddress indexBufferAddress  = device->GetBufferDeviceAddress((VkBuffer)submesh->GetIndexBuffer()->Get());

            VkAccelerationStructureGeometryTrianglesDataKHR triangles = {
                VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR};
            triangles.vertexFormat             = VK_FORMAT_R32G32B32_SFLOAT;
            triangles.vertexData.deviceAddress = vertexBufferAddress;
            triangles.vertexStride             = sizeof(MeshPositionVertex);
            triangles.maxVertex                = static_cast<uint32_t>(
                submesh->GetVertexPositionBuffer()->GetSpecification().Capacity / sizeof(MeshPositionVertex) - 1);
            triangles.indexType                   = VK_INDEX_TYPE_UINT32;
            triangles.indexData.deviceAddress     = indexBufferAddress;
            triangles.transformData.deviceAddress = 0;

            VkAccelerationStructureGeometryKHR geometry = {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
            geometry.geometry.triangles                 = triangles;
            geometry.geometryType                       = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
            geometry.flags                              = submesh->GetMaterial()->IsOpaque() ? VK_GEOMETRY_OPAQUE_BIT_KHR : 0;

            VkAccelerationStructureBuildRangeInfoKHR offsetInfo = {};
            offsetInfo.firstVertex                              = 0;
            offsetInfo.primitiveOffset                          = 0;
            offsetInfo.transformOffset                          = 0;
            offsetInfo.primitiveCount = static_cast<uint32_t>(submesh->GetIndexBuffer()->GetSpecification().Capacity /
                                                              sizeof(uint32_t) / 3);  // Number of triangles

            input.GeometryData.emplace_back(geometry);
            input.OffsetInfo.emplace_back(offsetInfo);
        }
    }

    struct BuildAccelerationStructure
    {
        VkAccelerationStructureBuildGeometryInfoKHR buildInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
        VkAccelerationStructureBuildSizesInfoKHR sizeInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
        const VkAccelerationStructureBuildRangeInfoKHR* rangeInfo;
        AccelerationStructure as;  // result acceleration structure
        AccelerationStructure cleanupAS;
    };

    // Building blases
    const auto nbBlas = static_cast<uint32_t>(blasInput.size());
    VkDeviceSize asTotalSize{0};     // Memory size of all allocated BLAS
    uint32_t nbCompactions{0};       // Nb of BLAS requesting compaction
    VkDeviceSize maxScratchSize{0};  // Largest scratch size

    // Preparing the information for the acceleration build commands.
    std::vector<BuildAccelerationStructure> buildAs(nbBlas);
    for (uint32_t idx = 0; idx < nbBlas; ++idx)
    {
        // Filling partially the VkAccelerationStructureBuildGeometryInfoKHR for querying the build sizes.
        buildAs[idx].buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        buildAs[idx].buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        buildAs[idx].buildInfo.flags =
            VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        buildAs[idx].buildInfo.geometryCount = static_cast<uint32_t>(blasInput[idx].GeometryData.size());
        buildAs[idx].buildInfo.pGeometries   = blasInput[idx].GeometryData.data();

        // Build range information
        buildAs[idx].rangeInfo = blasInput[idx].OffsetInfo.data();

        // Finding sizes to create acceleration structures and scratch
        std::vector<uint32_t> maxPrimCount(blasInput[idx].OffsetInfo.size());
        for (auto tt = 0; tt < blasInput[idx].OffsetInfo.size(); tt++)
            maxPrimCount[tt] = blasInput[idx].OffsetInfo[tt].primitiveCount;  // Number of primitives/triangles
        device->GetASBuildSizes(VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildAs[idx].buildInfo, maxPrimCount.data(),
                                &buildAs[idx].sizeInfo);

        // Extra info
        asTotalSize += buildAs[idx].sizeInfo.accelerationStructureSize;
        maxScratchSize = std::max(maxScratchSize, buildAs[idx].sizeInfo.buildScratchSize);
        nbCompactions += (buildAs[idx].buildInfo.flags & VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR) ==
                         VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;
    }

    // Allocate a query pool for storing the needed size for every BLAS compaction.
    VkQueryPool queryPool{VK_NULL_HANDLE};
    const auto& logicalDevice = device->GetLogicalDevice();
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
    constexpr VkDeviceSize batchLimit{256'000'000};  // 256 MB

    BufferSpecification sbSpec = {EBufferUsage::BUFFER_USAGE_SHADER_DEVICE_ADDRESS | EBufferUsage::BUFFER_USAGE_STORAGE};
    sbSpec.Capacity      = batchLimit;
    auto scratchBuffer         = Buffer::Create(sbSpec);

    VkDeviceAddress scratchBufferAddress = scratchBuffer->GetBDA();
    for (uint32_t i = 0; i < nbBlas; ++i)
    {
        indices.emplace_back(i);
        batchSize += buildAs[i].sizeInfo.accelerationStructureSize;
        // Over the limit or last BLAS element
        if (batchSize >= batchLimit || i == nbBlas - 1)
        {
            const CommandBufferSpecification cbSpec = {
                ECommandBufferType::COMMAND_BUFFER_TYPE_GRAPHICS, ECommandBufferLevel::COMMAND_BUFFER_LEVEL_PRIMARY,
                Renderer::GetRendererData()->FrameIndex, ThreadPool::MapThreadID(ThreadPool::GetMainThreadID())};
            auto vkCmdBuf = MakeShared<VulkanCommandBuffer>(cbSpec);
            vkCmdBuf->BeginRecording(true);

            //  VkCommandBuffer cmdBuf = m_cmdPool.createCommandBuffer();
            //    cmdCreateBlas(cmdBuf, indices, buildAs, scratchAddress, queryPool);
            {
                if (queryPool)  // For querying the compaction size
                    vkResetQueryPool(logicalDevice, queryPool, 0, static_cast<uint32_t>(indices.size()));
                uint32_t queryCnt{0};

                for (const auto& idx : indices)
                {
                    // Actual allocation of buffer and acceleration structure.
                    VkAccelerationStructureCreateInfoKHR asCI{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
                    asCI.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
                    asCI.size = buildAs[idx].sizeInfo.accelerationStructureSize;  // Will be used to allocate memory.

                    BufferSpecification abSpec = {EBufferUsage::BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE |
                                                  EBufferUsage::BUFFER_USAGE_SHADER_DEVICE_ADDRESS};
                    abSpec.Capacity      = asCI.size;
                    buildAs[idx].as.Buffer     = Buffer::Create(abSpec);

                    asCI.buffer = (VkBuffer)buildAs[idx].as.Buffer->Get();
                    VK_CHECK(vkCreateAccelerationStructureKHR(logicalDevice, &asCI, nullptr,
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
                    VkMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
                    barrier.srcAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
                    barrier.dstAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR;
                    vkCmdBuf->InsertBarrier(VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                                            VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &barrier, 0, nullptr, 0,
                                            nullptr);

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
            vkCmdBuf->Submit()->Wait();

            // Actual BLAS compaction
            if (queryPool)
            {
                const CommandBufferSpecification cbSpec = {
                    ECommandBufferType::COMMAND_BUFFER_TYPE_GRAPHICS, ECommandBufferLevel::COMMAND_BUFFER_LEVEL_PRIMARY,
                    Renderer::GetRendererData()->FrameIndex, ThreadPool::MapThreadID(ThreadPool::GetMainThreadID())};
                auto vkCmdBuf = MakeShared<VulkanCommandBuffer>(cbSpec);
                vkCmdBuf->BeginRecording(true);

                uint32_t queryCtn{0};
                std::vector<AccelerationStructure> cleanupAS;  // previous AS to destroy

                // Get the compacted size result back
                std::vector<VkDeviceSize> compactSizes(static_cast<uint32_t>(indices.size()));
                vkGetQueryPoolResults(logicalDevice, queryPool, 0, (uint32_t)compactSizes.size(),
                                      compactSizes.size() * sizeof(VkDeviceSize), compactSizes.data(), sizeof(VkDeviceSize),
                                      VK_QUERY_RESULT_WAIT_BIT);

                for (const auto& idx : indices)
                {
                    buildAs[idx].cleanupAS                          = buildAs[idx].as;           // previous AS to destroy
                    buildAs[idx].sizeInfo.accelerationStructureSize = compactSizes[queryCtn++];  // new reduced size

                    // Creating a compact version of the AS
                    VkAccelerationStructureCreateInfoKHR asCI{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
                    asCI.size = buildAs[idx].sizeInfo.accelerationStructureSize;
                    asCI.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

                    BufferSpecification abSpec = {EBufferUsage::BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE |
                                                  EBufferUsage::BUFFER_USAGE_SHADER_DEVICE_ADDRESS};
                    abSpec.Capacity      = asCI.size;
                    buildAs[idx].as.Buffer     = Buffer::Create(abSpec);

                    asCI.buffer = (VkBuffer)buildAs[idx].as.Buffer->Get();
                    VK_CHECK(vkCreateAccelerationStructureKHR(logicalDevice, &asCI, nullptr,
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
                vkCmdBuf->Submit()->Wait();
            }

            // Destroy the non-compacted version
            for (const auto& i : indices)
            {
                vkDestroyAccelerationStructureKHR(logicalDevice, (VkAccelerationStructureKHR)buildAs[i].cleanupAS.Handle, nullptr);
                buildAs[i].cleanupAS.Buffer.reset();
                buildAs[i].cleanupAS.Handle = nullptr;
            }

            // Reset
            batchSize = 0;
            indices.clear();
        }
    }

    // Logging reduction
    if (queryPool)
    {
        const VkDeviceSize compactSize = std::accumulate(
            buildAs.begin(), buildAs.end(), 0ULL, [](const auto& a, const auto& b) { return a + b.sizeInfo.accelerationStructureSize; });
        const float fractionSmaller = (asTotalSize == 0) ? 0 : (asTotalSize - compactSize) / float(asTotalSize);
        LOG_INFO("RT BLAS reducing from: {:.3f} MB to: {:.3f} MB, get rid of: {:.3f} MB. ({:2.2f}% smaller) \n",
                     asTotalSize / 1024.0f / 1024.0f, compactSize / 1024.0f / 1024.0f, (asTotalSize - compactSize) / 1024.0f / 1024.0f,
                     fractionSmaller * 100.f, "%");
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

    // create tlas
    std::vector<VkAccelerationStructureInstanceKHR> instances;
    for (size_t i{}; i < blases.size(); ++i)
    {
        VkAccelerationStructureInstanceKHR rayInst = instances.emplace_back();
        rayInst.transform.matrix[0][0] = rayInst.transform.matrix[1][1] = rayInst.transform.matrix[2][2] =
            1.0f;                         // Position of the instance
        rayInst.instanceCustomIndex = i;  // gl_InstanceCustomIndexEXT

        rayInst.accelerationStructureReference =
            context.GetDevice()->GetAccelerationStructureAddress((VkAccelerationStructureKHR)blases[i].Handle);
        rayInst.flags                                  = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        rayInst.mask                                   = 0xFF;  //  Only be hit if rayMask & instance.mask != 0
        rayInst.instanceShaderBindingTableRecordOffset = 0;     // We will use the same hit group for all objects
    }

    {
        // build tlas
        // Cannot call buildTlas twice except to update.
        // assert(m_tlas.accel == VK_NULL_HANDLE || update);
        uint32_t countInstance = static_cast<uint32_t>(instances.size());

        const CommandBufferSpecification cbSpec = {
            ECommandBufferType::COMMAND_BUFFER_TYPE_GRAPHICS, ECommandBufferLevel::COMMAND_BUFFER_LEVEL_PRIMARY,
            Renderer::GetRendererData()->FrameIndex, ThreadPool::MapThreadID(ThreadPool::GetMainThreadID())};
        auto vkCmdBuf = MakeShared<VulkanCommandBuffer>(cbSpec);
        vkCmdBuf->BeginRecording(true);

        BufferSpecification ibSpec = {EBufferUsage::BUFFER_USAGE_SHADER_DEVICE_ADDRESS |
                                      EBufferUsage::BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY |
                                      EBufferUsage::BUFFER_USAGE_TRANSFER_DESTINATION};
        // Create a buffer holding the actual instance data (matrices++) for use by the AS builder
        auto instancesBuffer = Buffer::Create(
            ibSpec,instances.data(), sizeof(instances[0]) * instances.size());  // Buffer of instances containing the matrices and BLAS ids

        // Make sure the copy of the instance buffer are copied before triggering the acceleration structure build
        VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        vkCmdPipelineBarrier((VkCommandBuffer)vkCmdBuf->Get(), VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &barrier, 0, nullptr, 0, nullptr);
        ++Renderer::GetStats().BarrierCount;

        // Creating the TLAS

        // Wraps a device pointer to the above uploaded instances.
        VkDeviceAddress instBufferAddr = instancesBuffer->GetBDA();

        VkAccelerationStructureGeometryInstancesDataKHR instancesVk{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR};
        instancesVk.data.deviceAddress = instBufferAddr;

        // Put the above into a VkAccelerationStructureGeometryKHR. We need to put the instances struct in a union and label it as instance
        // data.
        VkAccelerationStructureGeometryKHR topASGeometry{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
        topASGeometry.geometryType       = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        topASGeometry.geometry.instances = instancesVk;

        // Find sizes
        VkAccelerationStructureBuildGeometryInfoKHR buildInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
        buildInfo.type          = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        buildInfo.flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;  // flags;
        buildInfo.geometryCount = 1;
        buildInfo.pGeometries   = &topASGeometry;
        const bool update       = false;  // animation
        buildInfo.mode          = update ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        buildInfo.srcAccelerationStructure = VK_NULL_HANDLE;

        VkAccelerationStructureBuildSizesInfoKHR sizeInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
        vkGetAccelerationStructureBuildSizesKHR(logicalDevice, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &countInstance,
                                                &sizeInfo);

        VkAccelerationStructureCreateInfoKHR asCI{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
        asCI.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        asCI.size = sizeInfo.accelerationStructureSize;

        BufferSpecification abSpec = {EBufferUsage::BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE |
                                      EBufferUsage::BUFFER_USAGE_SHADER_DEVICE_ADDRESS};
        abSpec.Capacity      = asCI.size;
        builtTLAS.Buffer           = Buffer::Create(abSpec);

        asCI.buffer = (VkBuffer)builtTLAS.Buffer->Get();
        VK_CHECK(vkCreateAccelerationStructureKHR(logicalDevice, &asCI, nullptr, (VkAccelerationStructureKHR*)&builtTLAS.Handle),
                 "Failed to create acceleration structure!");

        BufferSpecification tlasSbSpec = {EBufferUsage::BUFFER_USAGE_STORAGE | EBufferUsage::BUFFER_USAGE_SHADER_DEVICE_ADDRESS};
        tlasSbSpec.Capacity      = sizeInfo.buildScratchSize;
        auto tlasScratchBuffer         = Buffer::Create(tlasSbSpec);

        VkDeviceAddress scratchAddress = tlasScratchBuffer->GetBDA();

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
        vkCmdBuf->Submit()->Wait();
    }

    return builtTLAS;
}

void VulkanRayTracingBuilder::DestroyAccelerationStructureImpl(AccelerationStructure& as)
{
    VulkanContext::Get().GetDevice()->WaitDeviceOnFinish();

    vkDestroyAccelerationStructureKHR(VulkanContext::Get().GetDevice()->GetLogicalDevice(), (VkAccelerationStructureKHR)as.Handle, nullptr);
    as.Buffer.reset();
    as.Handle = nullptr;
}

}  // namespace Pathfinder
