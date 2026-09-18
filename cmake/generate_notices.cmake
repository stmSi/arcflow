if(NOT DEFINED OUTPUT OR NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "OUTPUT and SOURCE_ROOT are required")
endif()

set(licenses
    "SDL|third_party/SDL/LICENSE.txt"
    "Vulkan-Headers|third_party/Vulkan-Headers/LICENSE.md"
    "volk|third_party/volk/LICENSE.md"
    "FreeType|third_party/freetype/LICENSE.TXT"
    "HarfBuzz|third_party/harfbuzz/COPYING"
    "Bundled test fonts|third_party/harfbuzz/test/COPYING"
    "Liberation document fonts|assets/fonts/LICENSE-Liberation.txt")
set(notices "Arcflow ${PROJECT_VERSION} third-party notices\n")
foreach(entry IN LISTS licenses)
    string(REPLACE "|" ";" fields "${entry}")
    list(GET fields 0 name)
    list(GET fields 1 relative)
    file(READ "${SOURCE_ROOT}/${relative}" text)
    string(APPEND notices "\n===== ${name} =====\n${text}\n")
endforeach()
file(WRITE "${OUTPUT}" "${notices}")
