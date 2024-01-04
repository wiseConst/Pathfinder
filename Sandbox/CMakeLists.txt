set(PROJECT_NAME Sandbox)

file(GLOB_RECURSE CORE_SOURCES "Source/*.h" "Source/*.cpp")
source_group("Source" FILES ${CORE_SOURCES})

add_executable(${PROJECT_NAME} ${CORE_SOURCES})

set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_OUTPUT_DIRECTORY})
target_link_libraries(${PROJECT_NAME} PRIVATE Pathfinder)

target_include_directories(${PROJECT_NAME} PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/Source"
        "${CMAKE_CURRENT_SOURCE_DIR}/../Pathfinder/Source"
)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DPFR_DEBUG=1)
    target_compile_options(Sandbox PRIVATE -g)
elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
    add_definitions(-DPFR_RELEASE=1)
    target_compile_options(Sandbox PRIVATE -Ofast)
elseif (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    add_definitions(-DPFR_RELEASE=1)
    target_compile_options(Sandbox PRIVATE -g)
endif ()

# TODO: Intermediate decision for CLION, current working dirs
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/Assets DESTINATION ${PROJECT_OUTPUT_DIRECTORY})