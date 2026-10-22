# Arcflow's binary package format stays outside the generic engine.
add_library(arcflow_core STATIC src/assets/package.c)
set_target_properties(arcflow_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_dependencies(arcflow_core arcflow_learning)
target_include_directories(arcflow_core PUBLIC include "${CMAKE_CURRENT_BINARY_DIR}/generated"
    PRIVATE src)
target_link_libraries(arcflow_core PUBLIC af::engine)

add_library(arcflow_runtime STATIC
    src/runtime/public_api.c
    src/runtime/runtime_bindings.c
    src/runtime/host_sdk.c
)
target_include_directories(arcflow_runtime PUBLIC include third_party/Vulkan-Headers/include PRIVATE src)
target_link_libraries(arcflow_runtime PUBLIC arcflow_core)

if(NOT EMSCRIPTEN)
add_library(arcflow_runtime_shared SHARED
    src/runtime/public_api.c
    src/runtime/runtime_bindings.c
    src/runtime/host_sdk.c
)
set_target_properties(arcflow_runtime_shared PROPERTIES OUTPUT_NAME arcflow_runtime)
target_include_directories(arcflow_runtime_shared PUBLIC include third_party/Vulkan-Headers/include
    PRIVATE src)
target_link_libraries(arcflow_runtime_shared PUBLIC arcflow_core)

endif()

add_library(arcflow_project STATIC
    src/onboarding/onboarding.c
    src/project/project.c
    src/project/project_collaboration.c
    src/project/project_templates.c
    src/project/image_decode.c
    src/project/image_svg.c
    src/project/project_images.c
    src/project/interchange.c
)
target_include_directories(arcflow_project PUBLIC src PRIVATE include)
target_link_libraries(arcflow_project PUBLIC arcflow_core PRIVATE SDL3::SDL3-static)

add_library(arcflow_editor_core STATIC
    src/editor/editor_commands.c
    src/editor/editor_frames.c
    src/editor/editor_fonts.c
    src/editor/editor_text.c
    src/editor/editor_compound.c
    src/editor/editor_export.c
    src/editor/editor_snapping.c
    src/editor/editor_gizmo.c
    src/editor/editor_vector_tools.c
    src/editor/editor_preferences.c
    src/editor/editor_recovery.c
    src/editor/editor_workflow.c
    src/plugin/plugin_host.c
)
target_include_directories(arcflow_editor_core PUBLIC src PRIVATE include)
target_link_libraries(arcflow_editor_core PUBLIC arcflow_project af::ui PRIVATE freetype SDL3::SDL3-static)

add_library(arcflow_update STATIC src/update/update.c)
target_include_directories(arcflow_update PUBLIC src PRIVATE include)
target_link_libraries(arcflow_update PUBLIC arcflow_core)

# This adapter translates the generic Vulkan device context to the Arcflow runtime API.
add_library(arcflow_renderer STATIC src/renderer/runtime_adapter.c)
target_include_directories(arcflow_renderer PUBLIC include PRIVATE src)
if(EMSCRIPTEN)
    target_link_libraries(arcflow_renderer PUBLIC af::canvas arcflow_runtime)
else()
    target_link_libraries(arcflow_renderer PUBLIC af::vulkan arcflow_runtime)
endif()

add_executable(arcflow_compiler src/compiler/compiler_main.c)
target_include_directories(arcflow_compiler PRIVATE src)
target_link_libraries(arcflow_compiler PRIVATE arcflow_project)
