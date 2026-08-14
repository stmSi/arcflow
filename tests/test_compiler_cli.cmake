if(NOT DEFINED ARCFLOW_COMPILER OR NOT DEFINED ARCFLOW_TEST_DIRECTORY)
    message(FATAL_ERROR "compiler and test directory are required")
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
if(NOT result EQUAL 0 OR NOT help MATCHES "--generate-examples")
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
