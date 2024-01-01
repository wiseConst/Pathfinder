set(PROJECT_NAME Pathfinder)

set(CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Source)

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

set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/Binaries/${CMAKE_BUILD_TYPE}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}/${PROJECT_NAME}")
target_precompile_headers(${PROJECT_NAME} PRIVATE ${CORE_DIR}/PathfinderPCH.h)

target_include_directories(${PROJECT_NAME} PUBLIC ${CORE_DIR})

# GLFW
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/vendor/GLFW)
target_link_libraries(${PROJECT_NAME} PRIVATE glfw)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/vendor/GLFW/include)

# VOLK
if (WIN32)
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)
elseif (APPLE)
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_XLIB_KHR)
elseif (UNIX)
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_MACOS_MVK)
endif ()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/vendor/volk)
target_link_libraries(${PROJECT_NAME} PRIVATE volk)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/vendor/volk)

# glm
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/vendor/glm)
target_link_libraries(${PROJECT_NAME} PRIVATE glm)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/vendor/glm)




























# fastgltf
add_subdirectory(vendor/fastgltf)


if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DPFR_DEBUG=1)
    add_compile_options(-g)
elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
    add_definitions(-DPFR_RELEASE=1)
    add_compile_options(-Ofast)
elseif (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    add_definitions(-DPFR_RELEASE=1)
    add_compile_options(-g)
endif ()