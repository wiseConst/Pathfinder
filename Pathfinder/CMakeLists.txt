set(PROJECT_NAME Pathfinder)

set(CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Source)
set(VENDOR_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor)

# Automatically group all sources into folders for MVS.
file(GLOB_RECURSE SRC_FILES "${CORE_DIR}/*.cpp" "${CORE_DIR}/*.h" "${CORE_DIR}/*.hpp")
set(ALL_FILES ${SRC_FILES})
foreach(FILE ${SRC_FILES})
    file(RELATIVE_PATH REL_FILE ${CMAKE_CURRENT_SOURCE_DIR} ${FILE})
    get_filename_component(DIR "${REL_FILE}" DIRECTORY)
    string(REPLACE "/" "\\" GROUP "${DIR}")

    source_group("${GROUP}" FILES ${FILE})
endforeach()

message("Building pathfinder rendering engine...")
set(VULKAN_SDK_DIR $ENV{VULKAN_SDK})
message("VULKAN_SDK PATH: ${VULKAN_SDK_DIR}")

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
set_target_properties(glfw PROPERTIES FOLDER "ThirdParty" POSITION_INDEPENDENT_CODE ON)

# meshoptimizer
set(MESHOPT_BUILD_SHARED_LIBS OFF)
add_subdirectory(${VENDOR_DIR}/meshoptimizer)
target_link_libraries(${PROJECT_NAME} PRIVATE meshoptimizer)
target_include_directories(${PROJECT_NAME} PUBLIC ${VENDOR_DIR}/meshoptimizer/src)
set_target_properties(meshoptimizer PROPERTIES FOLDER "ThirdParty" POSITION_INDEPENDENT_CODE ON)

# volk
if (WIN32)
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)
elseif (APPLE)
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_MACOS_MVK)
elseif (UNIX)
    # TODO: Add proper wayland/x11 support
    # set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_XLIB_KHR)
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WAYLAND_KHR)
endif ()

add_subdirectory(${VENDOR_DIR}/volk)
target_link_libraries(${PROJECT_NAME} PRIVATE volk)
target_include_directories(${PROJECT_NAME} PUBLIC ${VENDOR_DIR}/volk)
set_target_properties(volk PROPERTIES FOLDER "ThirdParty" POSITION_INDEPENDENT_CODE ON)

# vma
set(VMA_STATIC_VULKAN_FUNCTIONS OFF)
set(VMA_DYNAMIC_VULKAN_FUNCTIONS ON)
add_subdirectory(${VENDOR_DIR}/vma)
target_link_libraries(${PROJECT_NAME} PRIVATE GPUOpen::VulkanMemoryAllocator)
target_include_directories(${PROJECT_NAME} PUBLIC ${VENDOR_DIR}/vma/include)
set_target_properties(VulkanMemoryAllocator PROPERTIES FOLDER "ThirdParty" POSITION_INDEPENDENT_CODE ON)

# shaderc
target_include_directories(${PROJECT_NAME} PUBLIC ${VULKAN_SDK_DIR}/Include)

if (MSVC)
    target_link_libraries(${PROJECT_NAME} PRIVATE
            $<$<CONFIG:Debug>:${VULKAN_SDK_DIR}/Lib/shaderc_combinedd.lib>
            $<$<CONFIG:Release>:${VULKAN_SDK_DIR}/Lib/shaderc_combined.lib>)
else ()
    target_link_libraries(${PROJECT_NAME} PRIVATE
            $<$<CONFIG:Debug>:${VULKAN_SDK_DIR}/Lib/shaderc_sharedd.lib>
            $<$<CONFIG:Release>:${VULKAN_SDK_DIR}/Lib/shaderc_shared.lib>)
endif()

# glm
add_subdirectory(${VENDOR_DIR}/glm)
target_link_libraries(${PROJECT_NAME} PRIVATE glm)
target_include_directories(${PROJECT_NAME} PUBLIC ${VENDOR_DIR}/glm)
set_target_properties(glm PROPERTIES FOLDER "ThirdParty" POSITION_INDEPENDENT_CODE ON)

# spdlog
set(SPDLOG_ENABLE_PCH ON CACHE INTERNAL "")
set(SPDLOG_NO_EXCEPTIONS ON CACHE INTERNAL "")
add_subdirectory(${VENDOR_DIR}/spdlog)
#target_link_libraries(${PROJECT_NAME} PRIVATE spdlog)
target_include_directories(${PROJECT_NAME} PUBLIC ${VENDOR_DIR}/spdlog/include)
set_target_properties(spdlog PROPERTIES FOLDER "ThirdParty/spdlog" POSITION_INDEPENDENT_CODE ON)

# nlohmann/json
set(JSON_BuildTests OFF CACHE INTERNAL "")
add_subdirectory(${VENDOR_DIR}/nlohmann)
target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json::nlohmann_json)
target_include_directories(${PROJECT_NAME} PUBLIC ${VENDOR_DIR}/nlohmann/single_include)
set_target_properties(nlohmann_json PROPERTIES FOLDER "ThirdParty/nlohmann" POSITION_INDEPENDENT_CODE ON)

# stb
target_include_directories(${PROJECT_NAME} PUBLIC ${VENDOR_DIR}/stb)

# fastgltf
add_subdirectory(${VENDOR_DIR}/fastgltf)
target_link_libraries(${PROJECT_NAME} PRIVATE fastgltf)
target_include_directories(${PROJECT_NAME} PUBLIC ${VENDOR_DIR}/fastgltf/include)
set_target_properties(fastgltf PROPERTIES FOLDER "ThirdParty/fastgltf" POSITION_INDEPENDENT_CODE ON)

# imgui
add_library(imgui STATIC)
target_compile_definitions(imgui PRIVATE IMGUI_IMPL_VULKAN_NO_PROTOTYPES)
set_target_properties(imgui PROPERTIES RUNTIME_OUTPUT_DIRECTORY PROJECT_OUTPUT_DIRECTORY)

target_sources(imgui PRIVATE 
  ${VENDOR_DIR}/imgui/imgui.h
  ${VENDOR_DIR}/imgui/imgui.cpp

  ${VENDOR_DIR}/imgui/imgui_demo.cpp
  ${VENDOR_DIR}/imgui/imgui_draw.cpp
  ${VENDOR_DIR}/imgui/imgui_widgets.cpp
  ${VENDOR_DIR}/imgui/imgui_tables.cpp
  ${VENDOR_DIR}/imgui/backends/imgui_impl_vulkan.cpp
  ${VENDOR_DIR}/imgui/backends/imgui_impl_glfw.cpp
  )
target_include_directories(imgui PUBLIC ${VENDOR_DIR}/imgui ${VENDOR_DIR}/GLFW/include ${VULKAN_SDK_DIR}/include)
target_link_libraries(imgui PRIVATE glfw)
set_target_properties(imgui PROPERTIES FOLDER "ThirdParty" POSITION_INDEPENDENT_CODE ON)

target_link_libraries(${PROJECT_NAME} PRIVATE imgui)
target_include_directories(${PROJECT_NAME} PUBLIC ${VENDOR_DIR}/imgui)

# AMD Compressonator
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
target_link_libraries(${PROJECT_NAME} PRIVATE
            $<$<CONFIG:Debug>:${VENDOR_DIR}/amd_compressonator/lib/bin/Compressonator_MDd.lib>
            $<$<CONFIG:Release>:${VENDOR_DIR}/amd_compressonator/lib/bin/Compressonator_MD.lib>)
else()
    message("TODO: Add libs for linux/macos (AMD Compressonator).")
endif()
target_include_directories(${PROJECT_NAME} PUBLIC ${VENDOR_DIR}/amd_compressonator/include)

# entt
add_subdirectory(${VENDOR_DIR}/entt)
target_include_directories(${PROJECT_NAME} PUBLIC ${VENDOR_DIR}/entt/single_include)
target_link_libraries(${PROJECT_NAME} PRIVATE EnTT::EnTT)

# openmp
find_package(OpenMP REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE OpenMP::OpenMP_CXX)
if(OpenMP_CXX_FOUND)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        set(OpenMP_CXX_FLAGS "/openmp")
    else()
        set(OpenMP_CXX_FLAGS "-fopenmp")
    endif()

    target_compile_options(${PROJECT_NAME} PRIVATE ${OpenMP_CXX_FLAGS})
endif()

# ankerl's fast robin-hood backward shift deletion unordered set/map
add_subdirectory(${VENDOR_DIR}/unordered_dense)
target_link_libraries(${PROJECT_NAME} PRIVATE unordered_dense::unordered_dense)
target_include_directories(${PROJECT_NAME} PUBLIC ${VENDOR_DIR}/unordered_dense/include)

# Renderer compile config.
add_compile_definitions($<$<CONFIG:Debug>:PFR_DEBUG=1>)
add_compile_definitions($<$<CONFIG:Release>:PFR_RELEASE=1>)

add_compile_options($<$<CONFIG:Debug>:-Og>)
add_compile_options($<$<CONFIG:Release>:-O3 -lto>)

# Dynamic MSVC runtime library linking
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded$<$<CONFIG:Debug>:Debug>DLL$<$<NOT:$<CONFIG:Debug>>:>)
endif()