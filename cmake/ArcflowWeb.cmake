# The browser builds the same application sources with a Canvas renderer.
include("${CMAKE_CURRENT_LIST_DIR}/ArcflowLibraries.cmake")
set_target_properties(arcflow_compiler PROPERTIES SUFFIX ".js"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tools")
target_link_options(arcflow_compiler PRIVATE -sENVIRONMENT=node -sNODERAWFS=1
    -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=16777216 -sEXIT_RUNTIME=1)
if(DEFINED ENV{EMSDK_NODE})
    set(ARCFLOW_WEB_NODE "$ENV{EMSDK_NODE}")
else()
    find_program(ARCFLOW_WEB_NODE node REQUIRED)
endif()
set(ARCFLOW_DEMO_PACKAGE "${CMAKE_CURRENT_BINARY_DIR}/assets/arcflow_demo.afp")
add_custom_command(OUTPUT "${ARCFLOW_DEMO_PACKAGE}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/assets"
    COMMAND "${ARCFLOW_WEB_NODE}" "$<TARGET_FILE:arcflow_compiler>" --demo "${ARCFLOW_DEMO_PACKAGE}"
    DEPENDS arcflow_compiler VERBATIM)
add_custom_target(arcflow_demo_package DEPENDS "${ARCFLOW_DEMO_PACKAGE}")

add_library(arcflow_web_bridge OBJECT src/web/browser_bridge.c)
target_include_directories(arcflow_web_bridge PUBLIC src)
add_executable(arcflow_editor src/editor/editor_main.c src/editor/editor_app.c)
target_include_directories(arcflow_editor PRIVATE src)
target_compile_definitions(arcflow_editor PRIVATE ARCFLOW_DOCUMENT_FONT_DIR="/assets/fonts"
    ARCFLOW_EDITOR_FONT_PATH="/assets/fonts/LiberationSans-Regular.ttf")
target_link_libraries(arcflow_editor PRIVATE arcflow_renderer arcflow_editor_core arcflow_update arcflow_web_bridge)
set_target_properties(arcflow_editor PROPERTIES OUTPUT_NAME index)

add_executable(arcflow_player src/player/player_main.c src/player/player_app.c)
target_include_directories(arcflow_player PRIVATE src)
target_compile_definitions(arcflow_player PRIVATE ARCFLOW_DEMO_PACKAGE="/assets/arcflow_demo.afp")
target_link_libraries(arcflow_player PRIVATE arcflow_renderer arcflow_web_bridge)
set_target_properties(arcflow_player PROPERTIES OUTPUT_NAME player)

file(GLOB ARCFLOW_WEB_FONTS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/*")
foreach(app arcflow_editor arcflow_player)
    set_target_properties(${app} PROPERTIES SUFFIX ".html"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/site"
        LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html;${ARCFLOW_WEB_FONTS};${ARCFLOW_DEMO_PACKAGE}")
    target_link_options(${app} PRIVATE -sENVIRONMENT=web -sASYNCIFY=1
        -sASYNCIFY_STACK_SIZE=16777216 -sSTACK_SIZE=16777216
        -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=268435456 -sMAXIMUM_MEMORY=1073741824
        -sFORCE_FILESYSTEM=1 -sEXIT_RUNTIME=1 -lidbfs.js
        "-sEXPORTED_RUNTIME_METHODS=['FS','ccall']"
        "SHELL:--shell-file ${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
        "SHELL:--preload-file ${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts@/assets/fonts"
        "SHELL:--preload-file ${CMAKE_CURRENT_SOURCE_DIR}/assets/editor_demo.afproj@/examples/demo.afproj"
        "SHELL:--preload-file ${ARCFLOW_DEMO_PACKAGE}@/assets/arcflow_demo.afp")
    add_dependencies(${app} arcflow_demo_package)
endforeach()
add_custom_target(arcflow_web DEPENDS arcflow_editor arcflow_player)
configure_file(web/favicon.svg "${CMAKE_CURRENT_BINARY_DIR}/site/favicon.svg" COPYONLY)
install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/site/" DESTINATION .)

foreach(target arcflow_core arcflow_runtime arcflow_project arcflow_editor_core arcflow_update
    arcflow_renderer arcflow_compiler arcflow_web_bridge arcflow_editor arcflow_player)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror
        -Wconversion -Wshadow -Wstrict-prototypes)
endforeach()
