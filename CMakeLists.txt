cmake_minimum_required(VERSION 3.9)
cmake_policy(SET CMP0042 NEW)
set (CMAKE_CXX_STANDARD 17)

set (CMAKE_EXPORT_COMPILE_COMMANDS true)
set (CMAKE_COLOR_DIAGNOSTICS true)

project (node-vgmstream)

include_directories(${CMAKE_JS_INC})

file(GLOB SOURCE_FILES "src/lib.cpp")

file(GLOB VGMSTREAM_UTILS "vgmstream/cli/vgmstream_cli_utils.c" "vgmstream/cli/wav_utils.c")

add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES} ${CMAKE_JS_SRC} ${VGMSTREAM_UTILS})

include_directories(${CMAKE_SOURCE_DIR})
add_subdirectory("${CMAKE_SOURCE_DIR}/vgmstream")

add_custom_target(
  vgmstream_version
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/vgmstream
  COMMAND make version
)
add_dependencies(${PROJECT_NAME} vgmstream_version)

set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "" SUFFIX ".node")
target_link_libraries(${PROJECT_NAME} ${CMAKE_JS_LIB})
target_link_libraries(${PROJECT_NAME} libvgmstream_shared)

# Include Node-API wrappers
execute_process(COMMAND node -p "require('node-addon-api').include"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE NODE_ADDON_API_DIR
        )
string(REGEX REPLACE "[\r\n\"]" "" NODE_ADDON_API_DIR ${NODE_ADDON_API_DIR})

target_include_directories(${PROJECT_NAME} PRIVATE ${NODE_ADDON_API_DIR})

# define NAPI_VERSION
add_definitions(-DNAPI_VERSION=8)
