#ifndef INTRINSICS_H
#define INTRINSICS_H

#include <softintrin.h>
#include <immintrin.h>
#include <intrin.h>

namespace Pathfinder
{

static bool AVXSupported()
{
    bool bAVXSupported = false;

    int cpuInfo[4] = {0};
    __cpuid(cpuInfo, 1);
    const auto bOSUsesXSAVE_XRSTORE = cpuInfo[2] & (1 << 27) || false;

    __cpuid(cpuInfo, 0);
    int nIds = cpuInfo[0];

    if (nIds < 0x00000001) return false;
    __cpuid(cpuInfo, 0x00000001);

    const auto bCPUAVXSupport = cpuInfo[2] & (1 << 28) || false;
    if (bOSUsesXSAVE_XRSTORE && bCPUAVXSupport)
    {
        const size_t xcrFeatureMask = _xgetbv(_XCR_XFEATURE_ENABLED_MASK);
        bAVXSupported               = (xcrFeatureMask & 0x6) == 0x6;
    }
    return bAVXSupported;
}

static bool AVX2Supported()
{
    bool bAVX2Supported = false;

    int cpuInfo[4] = {0};
    __cpuid(cpuInfo, 1);
    const auto bOSUsesXSAVE_XRSTORE = cpuInfo[2] & (1 << 27) || false;

    __cpuid(cpuInfo, 0);
    int nIds = cpuInfo[0];

    if (nIds < 0x00000007) return false;
    __cpuid(cpuInfo, 0x00000007);

    const auto bCPUAVX2Support      = cpuInfo[1] & (1 << 5) || false;
    if (bOSUsesXSAVE_XRSTORE && bCPUAVX2Support)
    {
        const size_t xcrFeatureMask = _xgetbv(_XCR_XFEATURE_ENABLED_MASK);
        bAVX2Supported              = (xcrFeatureMask & 0x6) == 0x6;
    }
    return bAVX2Supported;
}

}  // namespace Pathfinder

#endif