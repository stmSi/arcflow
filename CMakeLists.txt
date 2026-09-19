cmake_minimum_required(VERSION 3.25)
project(arcflow VERSION 1.0.0 LANGUAGES C)

include(GNUInstallDirs)

if(POLICY CMP0219)
    cmake_policy(SET CMP0219 NEW)
    set(CMAKE_POLICY_DEFAULT_CMP0219 NEW)
endif()

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/project/project.h")
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/src/project/project.h"
    ARCFLOW_PROJECT_FORMAT_DEFINITION
    REGEX "^#define AF_PROJECT_FORMAT_VERSION [0-9]+U$")
if(NOT ARCFLOW_PROJECT_FORMAT_DEFINITION)
    message(FATAL_ERROR "AF_PROJECT_FORMAT_VERSION is missing from project.h")
endif()
string(REGEX REPLACE ".* ([0-9]+)U$" "\\1" ARCFLOW_PROJECT_FORMAT_VERSION
    "${ARCFLOW_PROJECT_FORMAT_DEFINITION}")

function(arcflow_read_unsigned_define source_file definition_name output_variable)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${source_file}")
    file(STRINGS "${source_file}" definition
        REGEX "^#define ${definition_name} [0-9]+U$")
    if(NOT definition)
        message(FATAL_ERROR "${definition_name} is missing from ${source_file}")
    endif()
    string(REGEX REPLACE ".* ([0-9]+)U$" "\\1" value "${definition}")
    set(${output_variable} "${value}" PARENT_SCOPE)
endfunction()

arcflow_read_unsigned_define(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/arcflow/arcflow_version.h"
    AF_RUNTIME_VERSION ARCFLOW_RUNTIME_VERSION)
arcflow_read_unsigned_define(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/arcflow/arcflow_version.h"
    AF_PACKAGE_FORMAT_VERSION ARCFLOW_PACKAGE_FORMAT_VERSION)
arcflow_read_unsigned_define(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/arcflow/arcflow_host_sdk.h"
    AF_HOST_SDK_VERSION_MAJOR ARCFLOW_HOST_SDK_VERSION_MAJOR)
arcflow_read_unsigned_define(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/arcflow/arcflow_host_sdk.h"
    AF_HOST_SDK_VERSION_MINOR ARCFLOW_HOST_SDK_VERSION_MINOR)
arcflow_read_unsigned_define(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/arcflow/arcflow_host_sdk.h"
    AF_HOST_SDK_VERSION_PATCH ARCFLOW_HOST_SDK_VERSION_PATCH)
set(ARCFLOW_HOST_SDK_VERSION
    "${ARCFLOW_HOST_SDK_VERSION_MAJOR}.${ARCFLOW_HOST_SDK_VERSION_MINOR}.${ARCFLOW_HOST_SDK_VERSION_PATCH}")

option(ARCFLOW_ENABLE_VALIDATION "Enable Vulkan validation in debug builds" ON)
option(ARCFLOW_BUILD_GPU_TESTS "Build Vulkan smoke tests" ON)
set(ARCFLOW_RELEASE_CHANNEL "Development" CACHE STRING
    "Release channel: Development, Preview, Beta, or Stable")
set_property(CACHE ARCFLOW_RELEASE_CHANNEL PROPERTY STRINGS Development Preview Beta Stable)
set(ARCFLOW_SOURCE_REVISION "" CACHE STRING "Deterministic source revision stamp")
set(ARCFLOW_BUILD_EPOCH "" CACHE STRING "Reproducible build epoch")
option(ARCFLOW_REQUIRE_SIGNING "Fail release configuration without a signing tool" OFF)
set(ARCFLOW_SIGN_TOOL "" CACHE FILEPATH "Optional artifact signing executable")
set(ARCFLOW_UPDATE_BASE_URL "" CACHE STRING "Base URL used for generated update manifests")
set(ARCFLOW_UPDATE_SIGNATURE "" CACHE STRING "Signature used for generated update manifests")

set(ARCFLOW_RELEASE_CHANNELS Development Preview Beta Stable)
if(NOT ARCFLOW_RELEASE_CHANNEL IN_LIST ARCFLOW_RELEASE_CHANNELS)
    message(FATAL_ERROR "ARCFLOW_RELEASE_CHANNEL must be Development, Preview, Beta, or Stable")
endif()
if(ARCFLOW_SOURCE_REVISION STREQUAL "")
    execute_process(COMMAND git rev-parse --short=12 HEAD
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE ARCFLOW_SOURCE_REVISION
        OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
endif()
if(ARCFLOW_SOURCE_REVISION STREQUAL "")
    set(ARCFLOW_SOURCE_REVISION "unknown")
endif()
if(ARCFLOW_BUILD_EPOCH STREQUAL "" AND DEFINED ENV{SOURCE_DATE_EPOCH})
    set(ARCFLOW_BUILD_EPOCH "$ENV{SOURCE_DATE_EPOCH}")
endif()
if(ARCFLOW_BUILD_EPOCH STREQUAL "")
    execute_process(COMMAND git show -s --format=%ct HEAD
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE ARCFLOW_BUILD_EPOCH
        OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
endif()
if(ARCFLOW_BUILD_EPOCH STREQUAL "")
    set(ARCFLOW_BUILD_EPOCH "0")
endif()
if(NOT ARCFLOW_BUILD_EPOCH MATCHES "^[0-9]+$")
    message(FATAL_ERROR "ARCFLOW_BUILD_EPOCH must contain decimal digits")
endif()
if(ARCFLOW_REQUIRE_SIGNING AND (ARCFLOW_SIGN_TOOL STREQUAL "" OR
                                NOT EXISTS "${ARCFLOW_SIGN_TOOL}"))
    message(FATAL_ERROR "Signing is required but ARCFLOW_SIGN_TOOL is unavailable")
endif()
if(MINGW)
    add_link_options(-static-libgcc -static-libstdc++)
endif()
set(ARCFLOW_BUILD_ID
    "${PROJECT_VERSION}-${ARCFLOW_RELEASE_CHANNEL}-${ARCFLOW_SOURCE_REVISION}-${ARCFLOW_BUILD_EPOCH}")
configure_file(cmake/arcflow_build_info.h.in
    "${CMAKE_CURRENT_BINARY_DIR}/generated/arcflow/arcflow_build_info.h" @ONLY)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/SDL/CMakeLists.txt")
    message(FATAL_ERROR "Pinned SDL3 source is missing; run tools/bootstrap-dependencies.sh")
endif()
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/Vulkan-Headers/include/vulkan/vulkan.h")
    message(FATAL_ERROR "Pinned Vulkan-Headers source is missing; run tools/bootstrap-dependencies.sh")
endif()
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/volk/volk.c")
    message(FATAL_ERROR "Pinned volk source is missing; run tools/bootstrap-dependencies.sh")
endif()
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/freetype/CMakeLists.txt" OR
   NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/harfbuzz/CMakeLists.txt")
    message(FATAL_ERROR "Pinned FreeType/HarfBuzz sources are missing; run tools/bootstrap-dependencies.sh")
endif()

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL_AUDIO OFF CACHE BOOL "" FORCE)
set(SDL_CAMERA OFF CACHE BOOL "" FORCE)
set(SDL_GPU OFF CACHE BOOL "" FORCE)
set(SDL_RENDER OFF CACHE BOOL "" FORCE)
set(SDL_JOYSTICK OFF CACHE BOOL "" FORCE)
set(SDL_HAPTIC OFF CACHE BOOL "" FORCE)
set(SDL_SENSOR OFF CACHE BOOL "" FORCE)
set(SDL_POWER OFF CACHE BOOL "" FORCE)
set(SDL_DIALOG ON CACHE BOOL "" FORCE)
set(SDL_TRAY OFF CACHE BOOL "" FORCE)
set(SDL_WAYLAND OFF CACHE BOOL "" FORCE)
set(ARCFLOW_SAVED_C_EXTENSIONS "${CMAKE_C_EXTENSIONS}")
set(ARCFLOW_SAVED_REQUIRED_DEFINITIONS "${CMAKE_REQUIRED_DEFINITIONS}")
set(CMAKE_C_EXTENSIONS ON)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
endif()
add_subdirectory(third_party/SDL EXCLUDE_FROM_ALL)
set(CMAKE_C_EXTENSIONS "${ARCFLOW_SAVED_C_EXTENSIONS}")
set(CMAKE_REQUIRED_DEFINITIONS "${ARCFLOW_SAVED_REQUIRED_DEFINITIONS}")

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
add_subdirectory(third_party/freetype EXCLUDE_FROM_ALL)
set(HB_BUILD_UTILS OFF CACHE BOOL "" FORCE)
set(HB_BUILD_SUBSET OFF CACHE BOOL "" FORCE)
set(HB_HAVE_FREETYPE ON CACHE BOOL "" FORCE)
add_subdirectory(third_party/harfbuzz EXCLUDE_FROM_ALL)

add_library(arcflow_volk STATIC third_party/volk/volk.c)
target_include_directories(arcflow_volk PUBLIC
    third_party/volk
    third_party/Vulkan-Headers/include
)
if(UNIX AND NOT APPLE)
    target_link_libraries(arcflow_volk PUBLIC dl)
endif()

set(ARCFLOW_PROJECT_SOURCES
    src/core/allocator.c
    src/core/array.c
    src/core/arena.c
    src/core/error.c
    src/core/file_io.c
    src/core/handles.c
    src/core/hash.c
    src/core/log.c
    src/core/math.c
    src/core/png.c
    src/scene/scene.c
    src/animation/animation.c
    src/animation/advanced_animation.c
    src/path/path.c
    src/path/path_boolean.c
    src/path/path_offset.c
    src/style/style.c
    src/layout/layout.c
    src/text/text.c
    src/binding/binding.c
    src/localization/localization.c
    src/performance/metrics.c
    src/stability/diagnostics.c
    src/component/component.c
    src/interaction/interaction.c
    src/state_machine/state_machine.c
    src/composition/composition.c
    src/deformation/deformation.c
    src/rigging/rigging.c
    src/particle/particle.c
    src/assets/image.c
    src/assets/package.c
)

add_library(arcflow_core STATIC ${ARCFLOW_PROJECT_SOURCES})
set_target_properties(arcflow_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(arcflow_core PUBLIC include "${CMAKE_CURRENT_BINARY_DIR}/generated"
    PRIVATE src)
if(UNIX)
target_link_libraries(arcflow_core PUBLIC m)
endif()
target_link_libraries(arcflow_core PUBLIC freetype harfbuzz)

add_library(arcflow_runtime STATIC
    src/runtime/public_api.c
    src/runtime/runtime_bindings.c
    src/runtime/host_sdk.c
)
target_include_directories(arcflow_runtime PUBLIC include third_party/Vulkan-Headers/include PRIVATE src)
target_link_libraries(arcflow_runtime PUBLIC arcflow_core)

add_library(arcflow_runtime_shared SHARED
    src/runtime/public_api.c
    src/runtime/runtime_bindings.c
    src/runtime/host_sdk.c
)
set_target_properties(arcflow_runtime_shared PROPERTIES OUTPUT_NAME arcflow_runtime)
target_include_directories(arcflow_runtime_shared PUBLIC include third_party/Vulkan-Headers/include
    PRIVATE src)
target_link_libraries(arcflow_runtime_shared PUBLIC arcflow_core)

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
    src/editor/editor_fonts.c
    src/editor/editor_text.c
    src/editor/editor_compound.c
    src/editor/editor_actions.c
    src/editor/editor_gizmo.c
    src/editor/editor_inspector.c
    src/editor/editor_vector_tools.c
    src/editor/editor_preferences.c
    src/editor/editor_recovery.c
    src/editor/editor_workflow.c
    src/plugin/plugin_host.c
)
target_include_directories(arcflow_editor_core PUBLIC src PRIVATE include)
target_link_libraries(arcflow_editor_core PUBLIC arcflow_project)

add_library(arcflow_update STATIC src/update/update.c)
target_include_directories(arcflow_update PUBLIC src PRIVATE include)
target_link_libraries(arcflow_update PUBLIC arcflow_core)

set(ARCFLOW_SHADER_TOOLCHAIN "${CMAKE_CURRENT_SOURCE_DIR}/third_party/shader-toolchain/usr")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND
   EXISTS "${ARCFLOW_SHADER_TOOLCHAIN}/bin/glslc" AND
   EXISTS "${ARCFLOW_SHADER_TOOLCHAIN}/bin/spirv-val")
    set(ARCFLOW_GLSLC "${ARCFLOW_SHADER_TOOLCHAIN}/bin/glslc")
    set(ARCFLOW_SPIRV_VAL "${ARCFLOW_SHADER_TOOLCHAIN}/bin/spirv-val")
    set(ARCFLOW_SHADER_TOOL_ENV "${CMAKE_COMMAND}" -E env
        "LD_LIBRARY_PATH=${ARCFLOW_SHADER_TOOLCHAIN}/lib")
else()
    find_program(ARCFLOW_GLSLC glslc REQUIRED)
    find_program(ARCFLOW_SPIRV_VAL spirv-val REQUIRED)
    set(ARCFLOW_SHADER_TOOL_ENV)
endif()
set(ARCFLOW_SHADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
file(MAKE_DIRECTORY "${ARCFLOW_SHADER_DIR}")

function(arcflow_shader SOURCE OUTPUT)
    add_custom_command(
        OUTPUT "${ARCFLOW_SHADER_DIR}/${OUTPUT}"
        COMMAND ${ARCFLOW_SHADER_TOOL_ENV} "${ARCFLOW_GLSLC}" --target-env=vulkan1.3 -O
                "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${SOURCE}"
                -o "${ARCFLOW_SHADER_DIR}/${OUTPUT}"
        COMMAND ${ARCFLOW_SHADER_TOOL_ENV} "${ARCFLOW_SPIRV_VAL}" --target-env vulkan1.3
                "${ARCFLOW_SHADER_DIR}/${OUTPUT}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${SOURCE}"
        VERBATIM
    )
endfunction()

arcflow_shader(analytic.vert analytic.vert.spv)
arcflow_shader(analytic.frag analytic.frag.spv)
arcflow_shader(path.vert path.vert.spv)
arcflow_shader(path.frag path.frag.spv)
arcflow_shader(image.vert image.vert.spv)
arcflow_shader(image.frag image.frag.spv)
arcflow_shader(particle.comp particle.comp.spv)
arcflow_shader(particle.vert particle.vert.spv)
arcflow_shader(particle.frag particle.frag.spv)
add_custom_target(arcflow_shaders ALL DEPENDS
    "${ARCFLOW_SHADER_DIR}/analytic.vert.spv"
    "${ARCFLOW_SHADER_DIR}/analytic.frag.spv"
    "${ARCFLOW_SHADER_DIR}/path.vert.spv"
    "${ARCFLOW_SHADER_DIR}/path.frag.spv"
    "${ARCFLOW_SHADER_DIR}/image.vert.spv"
    "${ARCFLOW_SHADER_DIR}/image.frag.spv"
    "${ARCFLOW_SHADER_DIR}/particle.comp.spv"
    "${ARCFLOW_SHADER_DIR}/particle.vert.spv"
    "${ARCFLOW_SHADER_DIR}/particle.frag.spv"
)

add_library(arcflow_renderer STATIC
    src/platform/platform_sdl.c
    src/renderer/vk_particles.c
    src/renderer/vk_renderer.c
)
target_include_directories(arcflow_renderer PUBLIC include PRIVATE src "${ARCFLOW_SHADER_DIR}")
target_compile_definitions(arcflow_renderer PRIVATE
    VK_NO_PROTOTYPES
    ARCFLOW_SHADER_DIR="${ARCFLOW_SHADER_DIR}"
    $<$<BOOL:${ARCFLOW_ENABLE_VALIDATION}>:AF_ENABLE_VALIDATION=1>
)
target_link_libraries(arcflow_renderer PUBLIC arcflow_runtime arcflow_volk SDL3::SDL3-static)
add_dependencies(arcflow_renderer arcflow_shaders)

add_executable(arcflow_compiler src/compiler/compiler_main.c)
target_include_directories(arcflow_compiler PRIVATE src)
target_link_libraries(arcflow_compiler PRIVATE arcflow_project)

set(ARCFLOW_DEMO_PACKAGE "${CMAKE_CURRENT_BINARY_DIR}/assets/arcflow_demo.afp")
add_custom_command(
    OUTPUT "${ARCFLOW_DEMO_PACKAGE}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/assets"
    COMMAND arcflow_compiler --demo "${ARCFLOW_DEMO_PACKAGE}"
    DEPENDS arcflow_compiler
    VERBATIM
)
add_custom_target(arcflow_demo_package ALL DEPENDS "${ARCFLOW_DEMO_PACKAGE}")

add_library(arcflow_player_app STATIC src/player/player_app.c)
target_include_directories(arcflow_player_app PRIVATE src)
target_compile_definitions(arcflow_player_app PRIVATE ARCFLOW_DEMO_PACKAGE="${ARCFLOW_DEMO_PACKAGE}")
target_link_libraries(arcflow_player_app PUBLIC arcflow_renderer)
add_dependencies(arcflow_player_app arcflow_demo_package)

add_executable(arcflow_player src/player/player_main.c)
target_include_directories(arcflow_player PRIVATE src)
target_link_libraries(arcflow_player PRIVATE arcflow_player_app)

add_executable(arcflow_editor
    src/editor/editor_main.c
    src/editor/editor_app.c
)
target_include_directories(arcflow_editor PRIVATE src)
target_compile_definitions(arcflow_editor PRIVATE
    ARCFLOW_DOCUMENT_FONT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts"
    ARCFLOW_EDITOR_FONT_PATH="${CMAKE_CURRENT_SOURCE_DIR}/third_party/harfbuzz/test/api/fonts/NotoSans-Bold.ttf"
)
target_link_libraries(arcflow_editor PRIVATE arcflow_renderer arcflow_editor_core arcflow_update)

enable_testing()
add_executable(arcflow_tests tests/test_main.c)
target_include_directories(arcflow_tests PRIVATE src third_party/volk third_party/Vulkan-Headers/include)
target_compile_definitions(arcflow_tests PRIVATE
    VK_NO_PROTOTYPES
    ARCFLOW_TEST_VERTEX_SHADER="${ARCFLOW_SHADER_DIR}/analytic.vert.spv"
    ARCFLOW_TEST_FRAGMENT_SHADER="${ARCFLOW_SHADER_DIR}/analytic.frag.spv"
    ARCFLOW_TEST_DOCUMENT_FONT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts"
    ARCFLOW_TEST_DOCUMENT_FONT="${CMAKE_CURRENT_SOURCE_DIR}/third_party/harfbuzz/test/api/fonts/NotoSans-Bold.ttf"
    ARCFLOW_TEST_FONT="${CMAKE_CURRENT_SOURCE_DIR}/third_party/harfbuzz/test/shape/data/in-house/fonts/NotoNastaliqUrdu-Regular.ttf"
    $<$<BOOL:${ARCFLOW_BUILD_GPU_TESTS}>:AF_BUILD_GPU_TESTS=1>
)
target_link_libraries(arcflow_tests PRIVATE arcflow_core arcflow_runtime arcflow_project
    arcflow_editor_core arcflow_volk)
add_dependencies(arcflow_tests arcflow_shaders)
add_test(NAME arcflow_tests COMMAND arcflow_tests)

add_executable(arcflow_performance_tests tests/test_performance.c)
target_include_directories(arcflow_performance_tests PRIVATE src include)
target_link_libraries(arcflow_performance_tests PRIVATE arcflow_core)
add_test(NAME arcflow_performance_tests COMMAND arcflow_performance_tests)

# Optional CPU microbenchmarks; timings are reported, not used as machine-dependent test gates.
add_executable(arcflow_hotpath_benchmarks EXCLUDE_FROM_ALL tests/benchmark_hotpaths.c)
target_include_directories(arcflow_hotpath_benchmarks PRIVATE src include)
target_link_libraries(arcflow_hotpath_benchmarks PRIVATE arcflow_project SDL3::SDL3-static)

add_executable(arcflow_stability_tests tests/test_stability.c)
target_include_directories(arcflow_stability_tests PRIVATE src include)
target_link_libraries(arcflow_stability_tests PRIVATE arcflow_core)
add_test(NAME arcflow_stability_tests COMMAND arcflow_stability_tests)

add_executable(arcflow_beta_performance_tests tests/test_beta_performance.c)
target_include_directories(arcflow_beta_performance_tests PRIVATE src include)
target_link_libraries(arcflow_beta_performance_tests PRIVATE arcflow_runtime arcflow_project
    SDL3::SDL3-static)
add_test(NAME arcflow_beta_performance_tests COMMAND arcflow_beta_performance_tests)
set_tests_properties(arcflow_beta_performance_tests PROPERTIES
    LABELS "beta;production;performance;long-duration"
    TIMEOUT 120)

add_executable(arcflow_release_tests tests/test_release.c)
target_include_directories(arcflow_release_tests PRIVATE src include)
target_link_libraries(arcflow_release_tests PRIVATE arcflow_runtime arcflow_project
    SDL3::SDL3-static)
add_test(NAME arcflow_release_tests COMMAND arcflow_release_tests)
set_tests_properties(arcflow_release_tests PROPERTIES LABELS "production;release" TIMEOUT 120)

add_executable(arcflow_parser_fuzz_tests tests/test_parser_fuzz.c)
target_include_directories(arcflow_parser_fuzz_tests PRIVATE src include)
target_link_libraries(arcflow_parser_fuzz_tests PRIVATE arcflow_project)
add_test(NAME arcflow_parser_fuzz_tests COMMAND arcflow_parser_fuzz_tests)
set_tests_properties(arcflow_parser_fuzz_tests PROPERTIES LABELS "beta;fuzz;security"
    TIMEOUT 120)

add_executable(arcflow_import_fuzz_tests tests/test_import_fuzz.c)
target_include_directories(arcflow_import_fuzz_tests PRIVATE src include)
target_compile_definitions(arcflow_import_fuzz_tests PRIVATE
    ARCFLOW_FUZZ_PNG="${CMAKE_CURRENT_SOURCE_DIR}/third_party/SDL/test/gamepad_axis_arrow.png"
    ARCFLOW_FUZZ_FONT="${CMAKE_CURRENT_SOURCE_DIR}/third_party/harfbuzz/test/shape/data/text-rendering-tests/fonts/TestSFNTOne.otf"
)
target_link_libraries(arcflow_import_fuzz_tests PRIVATE arcflow_project)
add_test(NAME arcflow_import_fuzz_tests COMMAND arcflow_import_fuzz_tests)
set_tests_properties(arcflow_import_fuzz_tests PROPERTIES LABELS "beta;fuzz;security;import"
    TIMEOUT 120)

add_executable(arcflow_localization_tests tests/test_localization.c)
target_include_directories(arcflow_localization_tests PRIVATE src include)
target_link_libraries(arcflow_localization_tests PRIVATE arcflow_core)
add_test(NAME arcflow_localization_tests COMMAND arcflow_localization_tests)

add_executable(arcflow_interchange_tests tests/test_interchange.c src/project/interchange.c)
target_include_directories(arcflow_interchange_tests PRIVATE src include)
target_link_libraries(arcflow_interchange_tests PRIVATE arcflow_core)
add_test(NAME arcflow_interchange_tests COMMAND arcflow_interchange_tests)

add_executable(arcflow_editor_workflow_tests tests/test_editor_workflow.c)
target_include_directories(arcflow_editor_workflow_tests PRIVATE src include)
target_link_libraries(arcflow_editor_workflow_tests PRIVATE arcflow_editor_core)
add_test(NAME arcflow_editor_workflow_tests COMMAND arcflow_editor_workflow_tests)

add_executable(arcflow_editor_recovery_tests tests/test_editor_recovery.c)
target_include_directories(arcflow_editor_recovery_tests PRIVATE src include)
target_link_libraries(arcflow_editor_recovery_tests PRIVATE arcflow_editor_core)
add_test(NAME arcflow_editor_recovery_tests COMMAND arcflow_editor_recovery_tests)

add_executable(arcflow_collaboration_tests tests/test_collaboration.c)
target_include_directories(arcflow_collaboration_tests PRIVATE src include)
target_link_libraries(arcflow_collaboration_tests PRIVATE arcflow_project)
add_test(NAME arcflow_collaboration_tests COMMAND arcflow_collaboration_tests)

add_test(NAME arcflow_compiler_cli_tests
    COMMAND "${CMAKE_COMMAND}"
        "-DARCFLOW_COMPILER=$<TARGET_FILE:arcflow_compiler>"
        "-DARCFLOW_TEST_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/compiler-cli-test"
        "-DARCFLOW_SOURCE_DIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}"
        "-DARCFLOW_PROJECT_FORMAT_VERSION=${ARCFLOW_PROJECT_FORMAT_VERSION}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_compiler_cli.cmake"
)

add_executable(arcflow_platform_tests tests/test_platform.c src/platform/platform_sdl.c)
target_include_directories(arcflow_platform_tests PRIVATE src include)
target_link_libraries(arcflow_platform_tests PRIVATE arcflow_core SDL3::SDL3-static)
add_test(NAME arcflow_platform_tests COMMAND arcflow_platform_tests)

add_executable(arcflow_update_tests tests/test_update.c)
target_include_directories(arcflow_update_tests PRIVATE src include)
target_link_libraries(arcflow_update_tests PRIVATE arcflow_update)
add_test(NAME arcflow_update_tests COMMAND arcflow_update_tests)

add_executable(arcflow_onboarding_tests tests/test_onboarding.c)
target_include_directories(arcflow_onboarding_tests PRIVATE src include)
target_link_libraries(arcflow_onboarding_tests PRIVATE arcflow_project)
add_test(NAME arcflow_onboarding_tests COMMAND arcflow_onboarding_tests)

add_executable(arcflow_runtime_sample examples/runtime_sample.c)
target_include_directories(arcflow_runtime_sample PRIVATE include third_party/Vulkan-Headers/include)
target_link_libraries(arcflow_runtime_sample PRIVATE arcflow_runtime)

set(ARCFLOW_EXAMPLE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/examples")
set(ARCFLOW_EXAMPLE_OUTPUTS)
foreach(example IN ITEMS blank guided-basics vector-animation interactive-button responsive-ui
                         particle-effect)
    list(APPEND ARCFLOW_EXAMPLE_OUTPUTS
        "${ARCFLOW_EXAMPLE_DIRECTORY}/${example}.afproj"
        "${ARCFLOW_EXAMPLE_DIRECTORY}/${example}.afp")
endforeach()
add_custom_command(
    OUTPUT ${ARCFLOW_EXAMPLE_OUTPUTS}
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${ARCFLOW_EXAMPLE_DIRECTORY}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${ARCFLOW_EXAMPLE_DIRECTORY}"
    COMMAND arcflow_compiler --generate-examples "${ARCFLOW_EXAMPLE_DIRECTORY}"
    DEPENDS arcflow_compiler
    VERBATIM)
add_custom_target(arcflow_examples ALL DEPENDS ${ARCFLOW_EXAMPLE_OUTPUTS})
add_test(NAME arcflow_runtime_sample_tests
    COMMAND arcflow_runtime_sample "${ARCFLOW_EXAMPLE_DIRECTORY}/guided-basics.afp")

add_custom_target(arcflow_ci
    COMMAND "${CMAKE_CTEST_COMMAND}" --output-on-failure
    DEPENDS arcflow_tests arcflow_performance_tests arcflow_localization_tests
            arcflow_stability_tests arcflow_beta_performance_tests arcflow_parser_fuzz_tests
            arcflow_import_fuzz_tests arcflow_release_tests
            arcflow_interchange_tests arcflow_editor_workflow_tests
            arcflow_editor_recovery_tests arcflow_collaboration_tests arcflow_compiler
            arcflow_platform_tests arcflow_update_tests arcflow_onboarding_tests
            arcflow_runtime_sample arcflow_examples arcflow_editor arcflow_player
            arcflow_runtime_shared arcflow_demo_package
    USES_TERMINAL
)

find_program(ARCFLOW_CLANG_FORMAT clang-format)
if(ARCFLOW_CLANG_FORMAT)
    file(GLOB_RECURSE ARCFLOW_FORMAT_FILES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/include/arcflow/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.c"
    )
    add_custom_target(arcflow_format_check
        COMMAND "${ARCFLOW_CLANG_FORMAT}" --dry-run --Werror ${ARCFLOW_FORMAT_FILES}
        VERBATIM
    )
endif()

set(ARCFLOW_OWN_TARGETS
    arcflow_core arcflow_runtime arcflow_runtime_shared arcflow_project arcflow_editor_core
    arcflow_update arcflow_renderer
    arcflow_compiler arcflow_player_app arcflow_player arcflow_editor arcflow_tests
    arcflow_performance_tests arcflow_hotpath_benchmarks
    arcflow_stability_tests arcflow_beta_performance_tests arcflow_parser_fuzz_tests
    arcflow_import_fuzz_tests arcflow_release_tests
    arcflow_localization_tests
    arcflow_interchange_tests
    arcflow_editor_workflow_tests
    arcflow_editor_recovery_tests
    arcflow_collaboration_tests
    arcflow_platform_tests
    arcflow_update_tests
    arcflow_onboarding_tests
    arcflow_runtime_sample
)
foreach(target IN LISTS ARCFLOW_OWN_TARGETS)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wshadow -Wstrict-prototypes
            "-ffile-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=."
            "-ffile-prefix-map=${CMAKE_CURRENT_BINARY_DIR}=./build"
        )
    endif()
endforeach()

set(ARCFLOW_NOTICE_FILE "${CMAKE_CURRENT_BINARY_DIR}/THIRD_PARTY_NOTICES.txt")
add_custom_command(
    OUTPUT "${ARCFLOW_NOTICE_FILE}"
    COMMAND "${CMAKE_COMMAND}"
        "-DOUTPUT=${ARCFLOW_NOTICE_FILE}"
        "-DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
        "-DPROJECT_VERSION=${PROJECT_VERSION}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/generate_notices.cmake"
    DEPENDS
        third_party/SDL/LICENSE.txt
        third_party/Vulkan-Headers/LICENSE.md
        third_party/volk/LICENSE.md
        third_party/freetype/LICENSE.TXT
        third_party/harfbuzz/COPYING
        third_party/harfbuzz/test/COPYING
        assets/fonts/LICENSE-Liberation.txt
        cmake/generate_notices.cmake
    VERBATIM)
add_custom_target(arcflow_notices ALL DEPENDS "${ARCFLOW_NOTICE_FILE}")
add_dependencies(arcflow_ci arcflow_notices)

install(TARGETS arcflow_editor arcflow_player arcflow_compiler arcflow_runtime_sample
        arcflow_runtime_shared
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}")
install(TARGETS arcflow_runtime arcflow_core
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
install(DIRECTORY include/arcflow DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/generated/arcflow/arcflow_build_info.h"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/arcflow")
install(FILES
    "${ARCFLOW_SHADER_DIR}/analytic.vert.spv"
    "${ARCFLOW_SHADER_DIR}/analytic.frag.spv"
    "${ARCFLOW_SHADER_DIR}/path.vert.spv"
    "${ARCFLOW_SHADER_DIR}/path.frag.spv"
    "${ARCFLOW_SHADER_DIR}/image.vert.spv"
    "${ARCFLOW_SHADER_DIR}/image.frag.spv"
    "${ARCFLOW_SHADER_DIR}/particle.comp.spv"
    "${ARCFLOW_SHADER_DIR}/particle.vert.spv"
    "${ARCFLOW_SHADER_DIR}/particle.frag.spv"
    DESTINATION "${CMAKE_INSTALL_BINDIR}/shaders")
install(FILES "${ARCFLOW_DEMO_PACKAGE}" DESTINATION "${CMAKE_INSTALL_BINDIR}/assets")
install(DIRECTORY "${ARCFLOW_EXAMPLE_DIRECTORY}/" DESTINATION "${CMAKE_INSTALL_BINDIR}/examples")
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/examples/runtime_sample.c"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/arcflow/examples")
install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/harfbuzz/test/api/fonts/NotoSans-Bold.ttf"
    DESTINATION "${CMAKE_INSTALL_BINDIR}/fonts")
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/" DESTINATION "${CMAKE_INSTALL_BINDIR}/fonts")
install(FILES "${ARCFLOW_NOTICE_FILE}" DESTINATION ".")
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/arcflow")
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/docs/"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/arcflow")

configure_file(cmake/uninstall.cmake.in "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake" @ONLY)
add_custom_target(uninstall
    COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")

find_program(ARCFLOW_OBJCOPY NAMES llvm-objcopy objcopy)
if(ARCFLOW_OBJCOPY)
    set(ARCFLOW_SYMBOL_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/symbols")
    add_custom_target(arcflow_symbols
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${ARCFLOW_SYMBOL_DIRECTORY}"
        COMMAND "${ARCFLOW_OBJCOPY}" --only-keep-debug $<TARGET_FILE:arcflow_editor>
                "${ARCFLOW_SYMBOL_DIRECTORY}/arcflow_editor.debug"
        COMMAND "${ARCFLOW_OBJCOPY}" --only-keep-debug $<TARGET_FILE:arcflow_player>
                "${ARCFLOW_SYMBOL_DIRECTORY}/arcflow_player.debug"
        COMMAND "${ARCFLOW_OBJCOPY}" --only-keep-debug $<TARGET_FILE:arcflow_compiler>
                "${ARCFLOW_SYMBOL_DIRECTORY}/arcflow_compiler.debug"
        COMMAND "${ARCFLOW_OBJCOPY}" --only-keep-debug $<TARGET_FILE:arcflow_runtime_shared>
                "${ARCFLOW_SYMBOL_DIRECTORY}/arcflow_runtime.debug"
        DEPENDS arcflow_editor arcflow_player arcflow_compiler arcflow_runtime_shared
        VERBATIM)
    install(DIRECTORY "${ARCFLOW_SYMBOL_DIRECTORY}/" DESTINATION symbols
            COMPONENT Symbols OPTIONAL)
endif()

if(NOT ARCFLOW_SIGN_TOOL STREQUAL "")
    add_custom_target(arcflow_sign
        COMMAND "${ARCFLOW_SIGN_TOOL}" $<TARGET_FILE:arcflow_editor>
        COMMAND "${ARCFLOW_SIGN_TOOL}" $<TARGET_FILE:arcflow_player>
        COMMAND "${ARCFLOW_SIGN_TOOL}" $<TARGET_FILE:arcflow_compiler>
        COMMAND "${ARCFLOW_SIGN_TOOL}" $<TARGET_FILE:arcflow_runtime_shared>
        DEPENDS arcflow_editor arcflow_player arcflow_compiler arcflow_runtime_shared
        VERBATIM)
endif()

add_test(NAME arcflow_install_tests
    COMMAND "${CMAKE_COMMAND}"
        "-DARCFLOW_BUILD_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}"
        "-DARCFLOW_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/install-test-root"
        "-DARCFLOW_CROSSCOMPILING=${CMAKE_CROSSCOMPILING}"
        "-DARCFLOW_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_install.cmake")

set(CPACK_PACKAGE_NAME "Arcflow")
set(CPACK_PACKAGE_VENDOR "Arcflow")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME
    "Arcflow-${PROJECT_VERSION}-${ARCFLOW_RELEASE_CHANNEL}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
set(ARCFLOW_BINARY_PACKAGE_FILE_NAME "${CPACK_PACKAGE_FILE_NAME}")
set(CPACK_PACKAGE_CHECKSUM SHA256)
set(CPACK_STRIP_FILES TRUE)
if(WIN32)
    set(CPACK_GENERATOR "ZIP;NSIS")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MODIFY_PATH OFF)
else()
    set(CPACK_GENERATOR "TGZ;ZIP")
endif()
include(CPack)

add_custom_target(arcflow_release_bundle
    COMMAND "${CMAKE_CPACK_COMMAND}" --config "${CMAKE_CURRENT_BINARY_DIR}/CPackConfig.cmake"
    DEPENDS arcflow_editor arcflow_player arcflow_compiler arcflow_runtime_shared
            arcflow_runtime_sample arcflow_examples arcflow_notices arcflow_demo_package
    USES_TERMINAL)
if(TARGET arcflow_symbols)
    add_dependencies(arcflow_release_bundle arcflow_symbols)
endif()

if((NOT ARCFLOW_UPDATE_BASE_URL STREQUAL "" OR NOT ARCFLOW_UPDATE_SIGNATURE STREQUAL "") AND
   (ARCFLOW_UPDATE_BASE_URL STREQUAL "" OR ARCFLOW_UPDATE_SIGNATURE STREQUAL ""))
    message(FATAL_ERROR
        "ARCFLOW_UPDATE_BASE_URL and ARCFLOW_UPDATE_SIGNATURE must be configured together")
endif()
if(WIN32)
    set(ARCFLOW_UPDATE_PACKAGE_EXTENSION ".zip")
else()
    set(ARCFLOW_UPDATE_PACKAGE_EXTENSION ".tar.gz")
endif()
if(NOT ARCFLOW_UPDATE_BASE_URL STREQUAL "")
    add_custom_target(arcflow_release_manifest
        COMMAND arcflow_compiler --make-update-manifest
            "${CMAKE_CURRENT_BINARY_DIR}/${ARCFLOW_BINARY_PACKAGE_FILE_NAME}${ARCFLOW_UPDATE_PACKAGE_EXTENSION}"
            "${ARCFLOW_UPDATE_BASE_URL}/${ARCFLOW_BINARY_PACKAGE_FILE_NAME}${ARCFLOW_UPDATE_PACKAGE_EXTENSION}"
            "${ARCFLOW_UPDATE_SIGNATURE}"
            "${CMAKE_CURRENT_BINARY_DIR}/${ARCFLOW_BINARY_PACKAGE_FILE_NAME}.afupdate"
        DEPENDS arcflow_release_bundle arcflow_compiler
        VERBATIM)
endif()

if(NOT WIN32)
    add_custom_target(arcflow_reproducibility
        COMMAND "${CMAKE_COMMAND}"
            "-DARCFLOW_CPACK_CONFIG=${CMAKE_CURRENT_BINARY_DIR}/CPackConfig.cmake"
            "-DARCFLOW_WORK_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/reproducibility"
            "-DARCFLOW_PACKAGE_FILE=${ARCFLOW_BINARY_PACKAGE_FILE_NAME}"
            "-DARCFLOW_BUILD_EPOCH=${ARCFLOW_BUILD_EPOCH}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_reproducible_package.cmake"
        DEPENDS arcflow_editor arcflow_player arcflow_compiler arcflow_runtime_shared
                arcflow_runtime_sample arcflow_examples arcflow_notices arcflow_demo_package
        USES_TERMINAL)
    if(TARGET arcflow_symbols)
        add_dependencies(arcflow_reproducibility arcflow_symbols)
    endif()
endif()

set(ARCFLOW_PORTABLE_PACKAGE
    "${CMAKE_CURRENT_BINARY_DIR}/${ARCFLOW_BINARY_PACKAGE_FILE_NAME}${ARCFLOW_UPDATE_PACKAGE_EXTENSION}")
set(ARCFLOW_EXPECT_SYMBOLS OFF)
if(TARGET arcflow_symbols)
    set(ARCFLOW_EXPECT_SYMBOLS ON)
endif()
add_custom_target(arcflow_production_acceptance
    COMMAND "${CMAKE_COMMAND}"
        "-DARCFLOW_SOURCE_DIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}"
        "-DARCFLOW_BUILD_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}"
        "-DARCFLOW_PRODUCT_VERSION=${PROJECT_VERSION}"
        "-DARCFLOW_RELEASE_CHANNEL=${ARCFLOW_RELEASE_CHANNEL}"
        "-DARCFLOW_SOURCE_REVISION=${ARCFLOW_SOURCE_REVISION}"
        "-DARCFLOW_BUILD_EPOCH=${ARCFLOW_BUILD_EPOCH}"
        "-DARCFLOW_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}"
        "-DARCFLOW_SYSTEM_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR}"
        "-DARCFLOW_PROJECT_FORMAT_VERSION=${ARCFLOW_PROJECT_FORMAT_VERSION}"
        "-DARCFLOW_PACKAGE_FORMAT_VERSION=${ARCFLOW_PACKAGE_FORMAT_VERSION}"
        "-DARCFLOW_RUNTIME_VERSION=${ARCFLOW_RUNTIME_VERSION}"
        "-DARCFLOW_HOST_SDK_VERSION=${ARCFLOW_HOST_SDK_VERSION}"
        "-DARCFLOW_PORTABLE_PACKAGE=${ARCFLOW_PORTABLE_PACKAGE}"
        "-DARCFLOW_NOTICE_FILE=${ARCFLOW_NOTICE_FILE}"
        "-DARCFLOW_EXPECT_SYMBOLS=${ARCFLOW_EXPECT_SYMBOLS}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_production_release.cmake"
    DEPENDS arcflow_ci arcflow_release_bundle
    USES_TERMINAL)
if(TARGET arcflow_reproducibility)
    add_dependencies(arcflow_production_acceptance arcflow_reproducibility)
endif()
