set(PROJECT_NAME Sandbox)

project(${PROJECT_NAME})

file(GLOB_RECURSE CORE_SOURCES "Source/*.h" "Source/*.cpp")
source_group("Source" FILES ${CORE_SOURCES})

add_executable(${PROJECT_NAME} ${CORE_SOURCES})

set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/Binaries/${CMAKE_BUILD_TYPE}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}/${PROJECT_NAME}")
target_link_libraries(${PROJECT_NAME} PRIVATE Pathfinder)

target_include_directories(${PROJECT_NAME} PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/Source"
        "${CMAKE_CURRENT_SOURCE_DIR}/../Pathfinder/Source"
)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(Sandbox PRIVATE PFR_DEBUG)
    target_compile_options(Sandbox PRIVATE -g)
elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(Sandbox PRIVATE PFR_RELEASE)
    target_compile_options(Sandbox PRIVATE -Ofast)
elseif (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    target_compile_definitions(Sandbox PRIVATE PFR_RELEASE)
    target_compile_options(Sandbox PRIVATE -g -O3)
endif ()