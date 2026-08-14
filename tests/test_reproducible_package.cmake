if(NOT DEFINED ARCFLOW_CPACK_CONFIG OR NOT DEFINED ARCFLOW_WORK_DIRECTORY OR
   NOT DEFINED ARCFLOW_PACKAGE_FILE OR NOT DEFINED ARCFLOW_BUILD_EPOCH)
    message(FATAL_ERROR "reproducibility inputs are required")
endif()
file(REMOVE_RECURSE "${ARCFLOW_WORK_DIRECTORY}")
file(MAKE_DIRECTORY "${ARCFLOW_WORK_DIRECTORY}/first" "${ARCFLOW_WORK_DIRECTORY}/second")
foreach(run first second)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env "SOURCE_DATE_EPOCH=${ARCFLOW_BUILD_EPOCH}"
                "${CMAKE_CPACK_COMMAND}" -G TGZ -B "${ARCFLOW_WORK_DIRECTORY}/${run}"
                --config "${ARCFLOW_CPACK_CONFIG}"
        RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "CPack reproducibility run failed")
    endif()
endforeach()
set(first "${ARCFLOW_WORK_DIRECTORY}/first/${ARCFLOW_PACKAGE_FILE}.tar.gz")
set(second "${ARCFLOW_WORK_DIRECTORY}/second/${ARCFLOW_PACKAGE_FILE}.tar.gz")
if(NOT EXISTS "${first}" OR NOT EXISTS "${second}")
    message(FATAL_ERROR "expected package was not generated")
endif()
file(SHA256 "${first}" first_hash)
file(SHA256 "${second}" second_hash)
if(NOT first_hash STREQUAL second_hash)
    message(FATAL_ERROR "identical release inputs produced different archives")
endif()
file(WRITE "${ARCFLOW_WORK_DIRECTORY}/reproducible.sha256" "${first_hash}\n")
