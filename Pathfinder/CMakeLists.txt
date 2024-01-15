set(PROJECT_NAME Pathfinder)

set(CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Source)
set(VENDOR_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor)

file(GLOB_RECURSE CORE_SOURCES ${CORE_DIR}/Core/*.h ${CORE_DIR}/Core/*.cpp)
source_group("Source/Core" FILES ${CORE_SOURCES})

file(GLOB_RECURSE EVENTS_SOURCES ${CORE_DIR}/Events/*.h ${CORE_DIR}/Events/*.cpp)
source_group("Source/Events" FILES ${EVENTS_SOURCES})

file(GLOB_RECURSE PLATFORM_SOURCES ${CORE_DIR}/Platform ${CORE_DIR}/Platform)
source_group("Source/Platform" FILES ${PLATFORM_SOURCES})

file(GLOB_RECURSE PLATFORM_GLFW_SOURCES ${CORE_DIR}/Platform/GLFW/*.h ${CORE_DIR}/Platform/GLFW/*.cpp)
source_group("Source/Platform/GLFW" FILES ${PLATFORM_GLFW_SOURCES})

file(GLOB_RECURSE PLATFORM_VULKAN_SOURCES ${CORE_DIR}/Platform/Vulkan/*.h ${CORE_DIR}/Platform/Vulkan/*.cpp)
source_group("Source/Platform/Vulkan" FILES ${PLATFORM_VULKAN_SOURCES})

file(GLOB_RECURSE RENDERER_SOURCES ${CORE_DIR}/Renderer/*.h ${CORE_DIR}/Renderer/*.cpp)
source_group("Source/Renderer" FILES ${RENDERER_SOURCES})

file(GLOB_RECURSE LAYERS_SOURCES ${CORE_DIR}/Layers/*.h ${CORE_DIR}/Layer/*.cpp)
source_group("Source/Layers" FILES ${LAYERS_SOURCES})

#TODO: Add memory includes

set(VULKAN_SDK_DIR $ENV{VULKAN_SDK})
message("VULKAN_SDK PATH: ${VULKAN_SDK_DIR}")

set(ALL_FILES
        ${CORE_SOURCES}
        ${EVENTS_SOURCES}
        ${LAYERS_SOURCES}
        ${PLATFORM_SOURCES}
        ${PLATFORM_GLFW_SOURCES}
        ${PLATFORM_VULKAN_SOURCES}
        ${RENDERER_SOURCES}
)

add_library(${PROJECT_NAME} STATIC ${ALL_FILES})
set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY PROJECT_OUTPUT_DIRECTORY)

target_include_directories(${PROJECT_NAME} PUBLIC ${CORE_DIR} ${VENDOR_DIR})
target_precompile_headers(${PROJECT_NAME} PRIVATE ${CORE_DIR}/PathfinderPCH.h)

# g++ filesystem issues ?
if (NOT MSVC)
    target_link_libraries(${PROJECT_NAME} PRIVATE stdc++fs)
endif ()

# GLFW
set(BUILD_SHARED_LIBS OFF)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory(${VENDOR_DIR}/GLFW)
target_link_libraries(${PROJECT_NAME} PRIVATE glfw)
target_include_directories(${PROJECT_NAME} PUBLIC ${VENDOR_DIR}/GLFW/include)

# meshoptimizer
set(MESHOPT_BUILD_SHARED_LIBS OFF)
add_subdirectory(${VENDOR_DIR}/meshoptimizer)
target_link_libraries(${PROJECT_NAME} PRIVATE meshoptimizer)
target_include_directories(${PROJECT_NAME} PUBLIC ${VENDOR_DIR}/meshoptimizer/src)

# volk
if (WIN32)
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)
elseif (APPLE)
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_XLIB_KHR)
elseif (UNIX)
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_MACOS_MVK)
endif ()

add_subdirectory(${VENDOR_DIR}/volk)
target_link_libraries(${PROJECT_NAME} PRIVATE volk)
target_include_directories(${PROJECT_NAME} PUBLIC ${VENDOR_DIR}/volk)

# vma
set(VMA_STATIC_VULKAN_FUNCTIONS OFF)
set(VMA_DYNAMIC_VULKAN_FUNCTIONS ON)
add_subdirectory(${VENDOR_DIR}/vma)
target_link_libraries(${PROJECT_NAME} PRIVATE GPUOpen::VulkanMemoryAllocator)
target_include_directories(${PROJECT_NAME} PUBLIC ${VENDOR_DIR}/vma/include)

# spirv-reflect
set(SPIRV_REFLECT_EXECUTABLE OFF)
set(SPIRV_REFLECT_STATIC_LIB ON)
add_subdirectory(${VENDOR_DIR}/spirv-reflect)
target_include_directories(${PROJECT_NAME} PUBLIC ${VENDOR_DIR}/spirv-reflect)
target_link_libraries(${PROJECT_NAME} PRIVATE spirv-reflect-static)

# shaderc (linked in the end of CMakeLists.txt)
target_include_directories(${PROJECT_NAME} PUBLIC ${VULKAN_SDK_DIR}/Include)

# glm
add_subdirectory(${VENDOR_DIR}/glm)
target_link_libraries(${PROJECT_NAME} PRIVATE glm)
target_include_directories(${PROJECT_NAME} PUBLIC ${VENDOR_DIR}/glm)

# stb
target_include_directories(${PROJECT_NAME} PUBLIC ${VENDOR_DIR}/stb)

# fastgltf
add_subdirectory(${VENDOR_DIR}/fastgltf)
target_link_libraries(${PROJECT_NAME} PRIVATE fastgltf)
target_include_directories(${PROJECT_NAME} PUBLIC ${VENDOR_DIR}/fastgltf/include)

add_compile_definitions($<$<CONFIG:Debug>:PFR_DEBUG=1>)
add_compile_definitions($<$<CONFIG:Release>:PFR_RELEASE=1>)

add_compile_options($<$<CONFIG:Debug>:-g -lto>)
add_compile_options($<$<CONFIG:Release>:-Ofast -lto>)

if(MSVC)
target_link_libraries(${PROJECT_NAME} PRIVATE 
        $<$<CONFIG:Debug>:${VULKAN_SDK_DIR}/Lib/shaderc_combinedd.lib> 
        $<$<CONFIG:Release>:${VULKAN_SDK_DIR}/Lib/shaderc_combined.lib>)
else()
target_link_libraries(${PROJECT_NAME} PRIVATE 
        $<$<CONFIG:Debug>:${VULKAN_SDK_DIR}/Lib/shaderc_sharedd.lib> 
        $<$<CONFIG:Release>:${VULKAN_SDK_DIR}/Lib/shaderc_shared.lib>)
endif()