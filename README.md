# **Pathfinder, as the name implies it's Vulkan-based path-tracing/mesh-shading renderer.**

## The way it goes... (since it's private I post this to have a living memory)

* Vulkan window clear color to linux-style colors. 
![Alt text](/Resources/Images/1.png)

* First rendered quad no way.
![Alt text](/Resources/Images/2.png)

* Batch-rendering 2D
![Alt text](/Resources/Images/3.png)

* Mesh-shading!
![Alt text](/Resources/Images/4_1_sponza_ms.png)

* Texture loading. Infinite grid(thanks to https://asliceofrendering.com/scene%20helper/2020/01/05/InfiniteGrid/).
![Alt text](/Resources/Images/5.png)

* Basic Blinn-Phong shading with normal mapping. Next PBR && multiple light sources with light culling.
![Alt text](/Resources/Images/6.png)