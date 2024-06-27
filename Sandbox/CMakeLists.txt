set(PROJECT_NAME Sandbox)

set(CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Source)

# Automatically group all sources into folders for MVS.
file(GLOB_RECURSE SRC_FILES "${CORE_DIR}/*.cpp" "${CORE_DIR}/*.h" "${CORE_DIR}/*.hpp")
set(ALL_FILES ${SRC_FILES})
foreach(FILE ${SRC_FILES})
    file(RELATIVE_PATH REL_FILE ${CMAKE_CURRENT_SOURCE_DIR} ${FILE})
    get_filename_component(DIR "${REL_FILE}" DIRECTORY)
    string(REPLACE "/" "\\" GROUP "${DIR}")

    source_group("${GROUP}" FILES ${FILE})
endforeach()

add_executable(${PROJECT_NAME} ${ALL_FILES})

if(MSVC)
    target_link_options(${PROJECT_NAME} PRIVATE "$<$<CONFIG:RELEASE>:/SUBSYSTEM:WINDOWS>")
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_OUTPUT_DIRECTORY})
target_link_libraries(${PROJECT_NAME} PRIVATE Pathfinder)

target_include_directories(${PROJECT_NAME} PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/Source"
        "${CMAKE_CURRENT_SOURCE_DIR}/../Pathfinder/Source"
)

add_compile_definitions($<$<CONFIG:Debug>:PFR_DEBUG=1>)
add_compile_definitions($<$<CONFIG:Release>:PFR_RELEASE=1>)

add_compile_options($<$<CONFIG:Debug>:-g>)
add_compile_options($<$<CONFIG:Release>:-Ofast -lto>)

if (MSVC)
    foreach(config ${CMAKE_CONFIGURATION_TYPES})
        file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/Assets DESTINATION "${PROJECT_OUTPUT_DIRECTORY}/${config}")
    endforeach()
else()
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/Assets DESTINATION ${PROJECT_OUTPUT_DIRECTORY})
endif()


# Since I'm sometimes using visual studio in build folder, specify this:
if(MSVC)
set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/Sandbox)
endif()