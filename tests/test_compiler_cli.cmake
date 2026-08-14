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
