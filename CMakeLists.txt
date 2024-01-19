cmake_minimum_required(VERSION 3.24 FATAL_ERROR)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(Pathfinder LANGUAGES CXX)
set(CMAKE_CONFIGURATION_TYPES Debug Release)

#set(PROJECT_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/Binaries/${CMAKE_BUILD_TYPE}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}/${PROJECT_NAME}")
set(PROJECT_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Binaries)
message("Output directory: ${PROJECT_OUTPUT_DIRECTORY}")

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CMAKE_COMMAND} -E time")

if (MSVC)
    add_compile_options("/MP")
endif()

add_subdirectory(Pathfinder)
add_subdirectory(Sandbox)

#set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK "${CMAKE_COMMAND} -E time")
#add_compile_options(-H) # Print all files that will be precompiled
#add_compile_options(-ftime-report) # Print stats