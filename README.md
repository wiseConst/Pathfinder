# **Pathfinder, as the name implies it's Vulkan-based path-tracing/mesh-shading renderer.**

## Current state.

* UE4 PBR shading model. Forward+ with Reversed Depth32F Buffer. Tone-Mapping + Gamma-Correction + SSAO + Bloom + HDR(RGBA16F).
![Alt text](/Resources/Images/7_2.png)

## Current Features
- ECS, scenes system(credits to entt)
- Compute Light Culling + 2.5D
- PBR HDR Renderer
- Post processing effects(Bloom, SSAO, HBAO)
- Texture compression(credits to AMD Compressonator)
- Mesh-Shading(done in one multidraw indirect call)
- Rapid 2D Rendering.
- Compute Object Culling
- UI(credits to Dear ImGui)
- DebugRenderer(spheres, cones, aabbs, etc.)
- API-Agnostic RHI(current Vulkan 1.3)
- RenderGraph based 

# Clone:
```python
git clone -b ShootThemUp --single-branch https://github.com/wiseConst/ShooterGameCourse.git
```
# Build:
```python
cd Pathfinder && mkdir build && cd build && cmake ..
```
