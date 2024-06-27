# **Pathfinder - my toy RHI-agnostic GPU-driven bindless renderer that I'm building to learn graphics and the latest GPU features.**

## Progress.

* UE4 PBR shading model. Forward+ gpu-driven mesh-shading, reversed Z. Tone-Mapping + Gamma-Correction + SSAO + Bloom + HDR(RGBA16F). As you see I'm completely GPU-bound, 99% load, RTX 3050 Ti laptop kinda weak :(
![Alt text](/Resources/Images/7_3.png)

## Features
- glTF model loading.
- Highly utilized ReBAR usage.
- RenderGraph based.
- RHI-agnostic (currently built with Vulkan 1.3 and BDA)
- ECS, scenes system.
- Compute Tiled Light Culling + 2.5D
- PBR HDR Renderer
- Post-processing (Bloom, SSAO)
- Texture compression(BC1-BC7)
- Mesh-Shading(everything is drawn in 1 multi draw indirect call)
- Rapid 2D Batch-Rendering(requires no VRAM).
- Compute Object Culling(gpu-driven)
- ImGui
- Cascaded Shadow Mapping(WIP)
- DebugRenderer(spheres, cones, aabbs, lines, etc..)
- Bindless(descriptor set per frame for images and textures, buffers are used in shaders through BDA)
- Multithreaded pipeline building via efficient C++23 threadpool.

## Dependencies
 - [AMD Compressonator](https://github.com/GPUOpen-Tools/compressonator.git)
 - [Entt](https://github.com/skypjack/entt.git)
 - [Fastgltf](https://github.com/spnda/fastgltf.git)
 - [GLFW](https://github.com/glfw/glfw.git)
 - [glm](https://github.com/g-truc/glm.git)
 - [ImGui](https://github.com/ocornut/imgui.git)
 - [meshoptimizer](https://github.com/zeux/meshoptimizer.git)
 - [nlohmann_json](https://github.com/nlohmann/json.git)
 - [spdlog](https://github.com/gabime/spdlog.git)
 - [stb](https://github.com/nothings/stb.git)
 - [unordered_dense](https://github.com/martinus/unordered_dense.git)
 - [VMA](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git)
 - [volk](https://github.com/zeux/volk.git)
 - [Vulkan SDK](https://www.lunarg.com/vulkan-sdk/)

# Clone:
```python
git clone --recursive -b main --single-branch https://github.com/wiseConst/Pathfinder.git
```
# Build:
```python
cd Pathfinder && mkdir build && cd build && cmake ..
```
