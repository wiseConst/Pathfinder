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

add_compile_definitions($<$<CONFIG:Debug>:PFR_DEBUG=1>)
add_compile_definitions($<$<CONFIG:Release>:PFR_RELEASE=1>)

add_compile_options($<$<CONFIG:Debug>:-g -lto>)
add_compile_options($<$<CONFIG:Release>:-Ofast -lto>)

if (MSVC)
    foreach(config ${CMAKE_CONFIGURATION_TYPES})
        file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/Assets DESTINATION "${PROJECT_OUTPUT_DIRECTORY}/${config}")
    endforeach()
else()
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/Assets DESTINATION ${PROJECT_OUTPUT_DIRECTORY})
endif()