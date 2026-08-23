if(NOT DEFINED ARCFLOW_COMPILER OR NOT DEFINED ARCFLOW_TEST_DIRECTORY OR
   NOT DEFINED ARCFLOW_SOURCE_DIRECTORY OR NOT DEFINED ARCFLOW_PROJECT_FORMAT_VERSION)
    message(FATAL_ERROR "compiler, source, format version, and test directory are required")
endif()

file(REMOVE_RECURSE "${ARCFLOW_TEST_DIRECTORY}")
file(MAKE_DIRECTORY "${ARCFLOW_TEST_DIRECTORY}")
set(project "${ARCFLOW_TEST_DIRECTORY}/ci-project.afproj")
set(package "${ARCFLOW_TEST_DIRECTORY}/ci-project.afp")

execute_process(COMMAND "${ARCFLOW_COMPILER}" --new-project "${project}"
                RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "new-project command failed")
endif()
execute_process(COMMAND "${ARCFLOW_COMPILER}" --validate-project "${project}"
                RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "project validation failed")
endif()
execute_process(COMMAND "${ARCFLOW_COMPILER}" --check-determinism "${project}"
                RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "determinism check failed")
endif()
execute_process(COMMAND "${ARCFLOW_COMPILER}" --beta-audit "${project}"
                RESULT_VARIABLE result OUTPUT_VARIABLE beta_audit)
if(NOT result EQUAL 0 OR NOT beta_audit MATCHES "beta audit READY")
    message(FATAL_ERROR "beta project audit failed")
endif()
set(legacy_project "${ARCFLOW_SOURCE_DIRECTORY}/assets/editor_demo.afproj")
set(migrated_project "${ARCFLOW_TEST_DIRECTORY}/migrated-project.afproj")
execute_process(COMMAND "${ARCFLOW_COMPILER}" --migrate-project "${legacy_project}"
                        "${migrated_project}"
                RESULT_VARIABLE result OUTPUT_VARIABLE migration)
if(NOT result EQUAL 0 OR
   NOT migration MATCHES "project format 3 -> ${ARCFLOW_PROJECT_FORMAT_VERSION}" OR
   NOT EXISTS "${migrated_project}")
    message(FATAL_ERROR "historical project migration failed")
endif()
file(STRINGS "${migrated_project}" migrated_header LIMIT_COUNT 1)
if(NOT migrated_header STREQUAL "arcflow_project ${ARCFLOW_PROJECT_FORMAT_VERSION}")
    message(FATAL_ERROR "migration did not write the current project format")
endif()
execute_process(COMMAND "${ARCFLOW_COMPILER}" --beta-audit "${migrated_project}"
                RESULT_VARIABLE result OUTPUT_VARIABLE migrated_audit)
if(NOT result EQUAL 0 OR NOT migrated_audit MATCHES "beta audit READY")
    message(FATAL_ERROR "migrated project failed the beta audit")
endif()
file(SHA256 "${migrated_project}" migrated_hash_before)
execute_process(COMMAND "${ARCFLOW_COMPILER}" --migrate-project "${migrated_project}"
                        "${migrated_project}"
                RESULT_VARIABLE result OUTPUT_VARIABLE normalized_migration)
file(SHA256 "${migrated_project}" migrated_hash_after)
file(SHA256 "${migrated_project}.bak1" migrated_backup_hash)
if(NOT result EQUAL 0 OR
   NOT normalized_migration MATCHES
       "project format ${ARCFLOW_PROJECT_FORMAT_VERSION} -> ${ARCFLOW_PROJECT_FORMAT_VERSION}" OR
   NOT migrated_hash_before STREQUAL migrated_hash_after OR
   NOT migrated_hash_before STREQUAL migrated_backup_hash)
    message(FATAL_ERROR "in-place project migration was not atomic and deterministic")
endif()
math(EXPR future_format_version "${ARCFLOW_PROJECT_FORMAT_VERSION} + 1")
set(future_project "${ARCFLOW_TEST_DIRECTORY}/future-project.afproj")
set(future_output "${ARCFLOW_TEST_DIRECTORY}/future-project-migrated.afproj")
file(WRITE "${future_project}"
     "arcflow_project ${future_format_version}\ncanvas 1280 720\n")
execute_process(COMMAND "${ARCFLOW_COMPILER}" --migrate-project "${future_project}"
                        "${future_output}"
                RESULT_VARIABLE result ERROR_QUIET)
if(result EQUAL 0 OR EXISTS "${future_output}")
    message(FATAL_ERROR "unsupported future project produced migration output")
endif()
execute_process(COMMAND "${ARCFLOW_COMPILER}" --compile-release "${project}" "${package}"
                RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "headless compilation failed")
endif()
execute_process(COMMAND "${ARCFLOW_COMPILER}" --validate "${package}"
                RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "compiled package validation failed")
endif()
execute_process(COMMAND "${ARCFLOW_COMPILER}" --make-update-manifest "${package}"
                        "https://updates.example/arcflow.afp" "" "${package}.afupdate"
                RESULT_VARIABLE result ERROR_QUIET)
if(result EQUAL 0 OR EXISTS "${package}.afupdate")
    message(FATAL_ERROR "empty update signature was accepted")
endif()
file(SHA256 "${package}" package_hash)
file(WRITE "${ARCFLOW_TEST_DIRECTORY}/package.sha256" "${package_hash}\n")

execute_process(COMMAND "${ARCFLOW_COMPILER}" --help
                RESULT_VARIABLE result OUTPUT_VARIABLE help)
if(NOT result EQUAL 0 OR NOT help MATCHES "--generate-examples" OR
   NOT help MATCHES "--migrate-project")
    message(FATAL_ERROR "compiler help is incomplete")
endif()
execute_process(COMMAND "${ARCFLOW_COMPILER}" --list-templates
                RESULT_VARIABLE result OUTPUT_VARIABLE templates)
if(NOT result EQUAL 0 OR NOT templates MATCHES "vector-animation" OR
   NOT templates MATCHES "particle-effect")
    message(FATAL_ERROR "template catalog is incomplete")
endif()
execute_process(COMMAND "${ARCFLOW_COMPILER}" --list-guides
                RESULT_VARIABLE result OUTPUT_VARIABLE guides)
if(NOT result EQUAL 0 OR NOT guides MATCHES "runtime-integration" OR
   NOT guides MATCHES "limitations")
    message(FATAL_ERROR "guide catalog is incomplete")
endif()
execute_process(COMMAND "${ARCFLOW_COMPILER}" --guide c-api
                RESULT_VARIABLE result OUTPUT_VARIABLE guide)
if(NOT result EQUAL 0 OR NOT guide MATCHES "arcflow_runtime.h" OR
   NOT guide MATCHES "runtime ABI")
    message(FATAL_ERROR "versioned C API guide is incomplete")
endif()
set(template_project "${ARCFLOW_TEST_DIRECTORY}/vector-template.afproj")
execute_process(COMMAND "${ARCFLOW_COMPILER}" --new-project-template vector-animation
                        "${template_project}"
                RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "template project generation failed")
endif()
execute_process(COMMAND "${ARCFLOW_COMPILER}" --validate-project "${template_project}"
                RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "template project validation failed")
endif()
set(example_directory "${ARCFLOW_TEST_DIRECTORY}/examples")
execute_process(COMMAND "${ARCFLOW_COMPILER}" --generate-examples "${example_directory}"
                RESULT_VARIABLE result OUTPUT_QUIET)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "example generation failed")
endif()
foreach(example IN ITEMS blank guided-basics vector-animation interactive-button responsive-ui
                         particle-effect)
    if(NOT EXISTS "${example_directory}/${example}.afproj" OR
       NOT EXISTS "${example_directory}/${example}.afp")
        message(FATAL_ERROR "generated example '${example}' is incomplete")
    endif()
    execute_process(COMMAND "${ARCFLOW_COMPILER}" --validate
                            "${example_directory}/${example}.afp"
                    RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "generated example '${example}' is invalid")
    endif()
endforeach()
