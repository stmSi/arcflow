cmake_minimum_required(VERSION 3.25)
project(arcflow VERSION 0.1.0 LANGUAGES C)

if(POLICY CMP0219)
    cmake_policy(SET CMP0219 NEW)
    set(CMAKE_POLICY_DEFAULT_CMP0219 NEW)
endif()

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ARCFLOW_ENABLE_VALIDATION "Enable Vulkan validation in debug builds" ON)
option(ARCFLOW_BUILD_GPU_TESTS "Build Vulkan smoke tests" ON)

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
set(SDL_DIALOG OFF CACHE BOOL "" FORCE)
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
    src/style/style.c
    src/layout/layout.c
    src/text/text.c
    src/component/component.c
    src/interaction/interaction.c
    src/state_machine/state_machine.c
    src/composition/composition.c
    src/deformation/deformation.c
    src/rigging/rigging.c
    src/particle/particle.c
    src/assets/package.c
)

add_library(arcflow_core STATIC ${ARCFLOW_PROJECT_SOURCES})
target_include_directories(arcflow_core PUBLIC include PRIVATE src)
if(UNIX)
target_link_libraries(arcflow_core PUBLIC m)
endif()
target_link_libraries(arcflow_core PUBLIC freetype harfbuzz)

add_library(arcflow_runtime STATIC
    src/runtime/public_api.c
)
target_include_directories(arcflow_runtime PUBLIC include third_party/Vulkan-Headers/include PRIVATE src)
target_link_libraries(arcflow_runtime PUBLIC arcflow_core)

add_library(arcflow_project STATIC
    src/project/project.c
)
target_include_directories(arcflow_project PUBLIC src PRIVATE include)
target_link_libraries(arcflow_project PUBLIC arcflow_core)

add_library(arcflow_editor_core STATIC
    src/editor/editor_commands.c
)
target_include_directories(arcflow_editor_core PUBLIC src PRIVATE include)
target_link_libraries(arcflow_editor_core PUBLIC arcflow_project)

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
arcflow_shader(particle.comp particle.comp.spv)
arcflow_shader(particle.vert particle.vert.spv)
arcflow_shader(particle.frag particle.frag.spv)
add_custom_target(arcflow_shaders ALL DEPENDS
    "${ARCFLOW_SHADER_DIR}/analytic.vert.spv"
    "${ARCFLOW_SHADER_DIR}/analytic.frag.spv"
    "${ARCFLOW_SHADER_DIR}/path.vert.spv"
    "${ARCFLOW_SHADER_DIR}/path.frag.spv"
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
    ARCFLOW_EDITOR_FONT_PATH="${CMAKE_CURRENT_SOURCE_DIR}/third_party/harfbuzz/test/api/fonts/NotoSans-Bold.ttf"
)
target_link_libraries(arcflow_editor PRIVATE arcflow_renderer arcflow_editor_core)

enable_testing()
add_executable(arcflow_tests tests/test_main.c)
target_include_directories(arcflow_tests PRIVATE src third_party/volk third_party/Vulkan-Headers/include)
target_compile_definitions(arcflow_tests PRIVATE
    VK_NO_PROTOTYPES
    ARCFLOW_TEST_VERTEX_SHADER="${ARCFLOW_SHADER_DIR}/analytic.vert.spv"
    ARCFLOW_TEST_FRAGMENT_SHADER="${ARCFLOW_SHADER_DIR}/analytic.frag.spv"
    ARCFLOW_TEST_FONT="${CMAKE_CURRENT_SOURCE_DIR}/third_party/harfbuzz/test/shape/data/in-house/fonts/NotoNastaliqUrdu-Regular.ttf"
    $<$<BOOL:${ARCFLOW_BUILD_GPU_TESTS}>:AF_BUILD_GPU_TESTS=1>
)
target_link_libraries(arcflow_tests PRIVATE arcflow_core arcflow_runtime arcflow_project
    arcflow_editor_core arcflow_volk)
add_dependencies(arcflow_tests arcflow_shaders)
add_test(NAME arcflow_tests COMMAND arcflow_tests)

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
    arcflow_core arcflow_runtime arcflow_project arcflow_editor_core arcflow_renderer
    arcflow_compiler arcflow_player_app arcflow_player arcflow_editor arcflow_tests
)
foreach(target IN LISTS ARCFLOW_OWN_TARGETS)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Werror
            -Wconversion -Wshadow -Wstrict-prototypes
        )
    endif()
endforeach()
