#ifndef ENTRYPOINT_H
#define ENTRYPOINT_H

#include "PlatformDetection.h"

namespace Pathfinder
{

extern Unique<Application> Create(const CommandLineArguments& cmdLineArgs);

int32_t Main(int32_t argc, char** argv)
{
    auto app = Create({argc, argv});
    app->Run();

    return 0;
}

}  // namespace Pathfinder

#endif

#if PFR_RELEASE

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
{
    return Pathfinder::Main(__argc, __argv);
}

#else

int32_t main(int32_t argc, char** argv)
{
    return Pathfinder::Main(argc, argv);
}

#endif
