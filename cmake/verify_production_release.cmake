set(required_inputs
    ARCFLOW_SOURCE_DIRECTORY
    ARCFLOW_BUILD_DIRECTORY
    ARCFLOW_PRODUCT_VERSION
    ARCFLOW_RELEASE_CHANNEL
    ARCFLOW_SOURCE_REVISION
    ARCFLOW_BUILD_EPOCH
    ARCFLOW_SYSTEM_NAME
    ARCFLOW_SYSTEM_PROCESSOR
    ARCFLOW_PROJECT_FORMAT_VERSION
    ARCFLOW_PACKAGE_FORMAT_VERSION
    ARCFLOW_RUNTIME_VERSION
    ARCFLOW_HOST_SDK_VERSION
    ARCFLOW_PORTABLE_PACKAGE
    ARCFLOW_NOTICE_FILE
    ARCFLOW_EXPECT_SYMBOLS)
foreach(input IN LISTS required_inputs)
    if(NOT DEFINED ${input} OR "${${input}}" STREQUAL "")
        message(FATAL_ERROR "production acceptance input ${input} is missing")
    endif()
endforeach()

if(NOT ARCFLOW_PRODUCT_VERSION STREQUAL "1.0.0")
    message(FATAL_ERROR "production release must be stamped Arcflow 1.0.0")
endif()
if(NOT ARCFLOW_RELEASE_CHANNEL STREQUAL "Stable")
    message(FATAL_ERROR "production release must use the Stable channel")
endif()
string(LENGTH "${ARCFLOW_SOURCE_REVISION}" source_revision_length)
if(ARCFLOW_SOURCE_REVISION STREQUAL "unknown" OR
   NOT ARCFLOW_SOURCE_REVISION MATCHES "^[0-9a-fA-F]+$" OR
   source_revision_length LESS 7 OR source_revision_length GREATER 40)
    message(FATAL_ERROR "production release requires a Git source revision")
endif()
if(ARCFLOW_BUILD_EPOCH STREQUAL "0" OR NOT ARCFLOW_BUILD_EPOCH MATCHES "^[0-9]+$")
    message(FATAL_ERROR "production release requires a nonzero reproducible build epoch")
endif()
if(NOT ARCFLOW_HOST_SDK_VERSION STREQUAL "1.0.0")
    message(FATAL_ERROR "production release requires the stable host SDK 1.0.0 ABI")
endif()

set(required_files
    "${ARCFLOW_PORTABLE_PACKAGE}"
    "${ARCFLOW_PORTABLE_PACKAGE}.sha256"
    "${ARCFLOW_NOTICE_FILE}"
    "${ARCFLOW_BUILD_DIRECTORY}/generated/arcflow/arcflow_build_info.h"
    "${ARCFLOW_SOURCE_DIRECTORY}/README.md"
    "${ARCFLOW_SOURCE_DIRECTORY}/docs/production-release.md"
    "${ARCFLOW_SOURCE_DIRECTORY}/docs/runtime-api.md"
    "${ARCFLOW_SOURCE_DIRECTORY}/docs/runtime-package.md")
foreach(required_file IN LISTS required_files)
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR "production release file is missing: ${required_file}")
    endif()
endforeach()

if(ARCFLOW_EXPECT_SYMBOLS)
    foreach(symbol IN ITEMS arcflow_editor.debug arcflow_player.debug arcflow_compiler.debug
                            arcflow_runtime.debug)
        if(NOT EXISTS "${ARCFLOW_BUILD_DIRECTORY}/symbols/${symbol}")
            message(FATAL_ERROR "production debug symbol is missing: ${symbol}")
        endif()
    endforeach()
endif()

file(READ "${ARCFLOW_BUILD_DIRECTORY}/generated/arcflow/arcflow_build_info.h" build_info)
foreach(expected IN ITEMS
        "#define AF_VERSION_STRING \"1.0.0\""
        "#define AF_RELEASE_CHANNEL \"Stable\""
        "#define AF_SOURCE_REVISION \"${ARCFLOW_SOURCE_REVISION}\"")
    string(FIND "${build_info}" "${expected}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "generated build metadata is inconsistent: ${expected}")
    endif()
endforeach()

file(READ "${ARCFLOW_SOURCE_DIRECTORY}/README.md" readme)
string(FIND "${readme}" "Arcflow 1.0" readme_version)
if(readme_version EQUAL -1)
    message(FATAL_ERROR "README does not identify the Arcflow 1.0 release")
endif()
file(READ "${ARCFLOW_SOURCE_DIRECTORY}/docs/runtime-api.md" runtime_api)
string(FIND "${runtime_api}" "Host SDK 1.0.0" sdk_version)
if(sdk_version EQUAL -1)
    message(FATAL_ERROR "runtime API guide does not identify Host SDK 1.0.0")
endif()
file(READ "${ARCFLOW_SOURCE_DIRECTORY}/docs/runtime-package.md" runtime_package)
string(FIND "${runtime_package}" "format ${ARCFLOW_PACKAGE_FORMAT_VERSION}" package_version)
if(package_version EQUAL -1)
    message(FATAL_ERROR "runtime package guide does not match the package format")
endif()
file(READ "${ARCFLOW_NOTICE_FILE}" notices)
string(FIND "${notices}" "Arcflow 1.0.0 third-party notices" notice_version)
if(notice_version EQUAL -1)
    message(FATAL_ERROR "third-party notices do not match the product version")
endif()

file(SHA256 "${ARCFLOW_PORTABLE_PACKAGE}" artifact_sha256)
file(READ "${ARCFLOW_PORTABLE_PACKAGE}.sha256" checksum_file)
string(FIND "${checksum_file}" "${artifact_sha256}" checksum_match)
if(checksum_match EQUAL -1)
    message(FATAL_ERROR "portable package checksum does not match the artifact")
endif()

set(report "${ARCFLOW_BUILD_DIRECTORY}/arcflow-production-readiness.json")
file(WRITE "${report}"
    "{\"version\":\"${ARCFLOW_PRODUCT_VERSION}\","
    "\"channel\":\"${ARCFLOW_RELEASE_CHANNEL}\","
    "\"revision\":\"${ARCFLOW_SOURCE_REVISION}\","
    "\"build_epoch\":${ARCFLOW_BUILD_EPOCH},"
    "\"system\":\"${ARCFLOW_SYSTEM_NAME}-${ARCFLOW_SYSTEM_PROCESSOR}\","
    "\"project_format\":${ARCFLOW_PROJECT_FORMAT_VERSION},"
    "\"package_format\":${ARCFLOW_PACKAGE_FORMAT_VERSION},"
    "\"runtime_version\":${ARCFLOW_RUNTIME_VERSION},"
    "\"host_sdk\":\"${ARCFLOW_HOST_SDK_VERSION}\","
    "\"artifact_sha256\":\"${artifact_sha256}\","
    "\"automated_acceptance\":true}\n")
message(STATUS "Arcflow ${ARCFLOW_PRODUCT_VERSION} production acceptance passed")
message(STATUS "Readiness report: ${report}")
