if(NOT DEFINED ARCFLOW_BUILD_DIRECTORY OR NOT DEFINED ARCFLOW_INSTALL_PREFIX)
    message(FATAL_ERROR "build directory and install prefix are required")
endif()
file(REMOVE_RECURSE "${ARCFLOW_INSTALL_PREFIX}")
set(external_root "${ARCFLOW_BUILD_DIRECTORY}/install-test-user-data")
file(REMOVE_RECURSE "${external_root}")
file(MAKE_DIRECTORY "${external_root}/projects" "${external_root}/settings")
file(WRITE "${external_root}/projects/user-project.afproj" "preserve-project\n")
file(WRITE "${external_root}/settings/preferences" "preserve-settings\n")
execute_process(COMMAND "${CMAKE_COMMAND}" --install "${ARCFLOW_BUILD_DIRECTORY}"
                        --prefix "${ARCFLOW_INSTALL_PREFIX}"
                RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "clean installation failed")
endif()
set(compiler "${ARCFLOW_INSTALL_PREFIX}/bin/arcflow_compiler${ARCFLOW_EXECUTABLE_SUFFIX}")
set(runtime_sample
    "${ARCFLOW_INSTALL_PREFIX}/bin/arcflow_runtime_sample${ARCFLOW_EXECUTABLE_SUFFIX}")
set(guided_example "${ARCFLOW_INSTALL_PREFIX}/bin/examples/guided-basics.afp")
set(sample_source "${ARCFLOW_INSTALL_PREFIX}/share/arcflow/examples/runtime_sample.c")
set(installed_readme "${ARCFLOW_INSTALL_PREFIX}/share/doc/arcflow/README.md")
set(installed_release_guide
    "${ARCFLOW_INSTALL_PREFIX}/share/doc/arcflow/production-release.md")
if(NOT EXISTS "${compiler}" OR NOT EXISTS "${runtime_sample}" OR
   NOT EXISTS "${guided_example}" OR NOT EXISTS "${sample_source}" OR
   NOT EXISTS "${installed_readme}" OR NOT EXISTS "${installed_release_guide}" OR
   NOT EXISTS "${ARCFLOW_INSTALL_PREFIX}/bin/fonts/LiberationSans-Regular.ttf")
    message(FATAL_ERROR "installed compiler, examples, runtime sample, editor font, or documentation is missing")
endif()
file(READ "${installed_readme}" readme)
string(FIND "${readme}" "Arcflow 1.0" readme_version)
if(readme_version EQUAL -1)
    message(FATAL_ERROR "installed documentation has the wrong product version")
endif()
if(NOT CMAKE_CROSSCOMPILING)
    execute_process(COMMAND "${compiler}" --version RESULT_VARIABLE result
                    OUTPUT_VARIABLE compiler_version)
    if(NOT result EQUAL 0 OR NOT compiler_version MATCHES "Arcflow 1.0.0")
        message(FATAL_ERROR "installed compiler did not launch with the production version")
    endif()
    execute_process(COMMAND "${compiler}" --guide quick-start RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "installed in-product guide did not launch")
    endif()
    execute_process(COMMAND "${runtime_sample}" "${guided_example}" RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "installed runtime sample did not complete")
    endif()
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" --install "${ARCFLOW_BUILD_DIRECTORY}"
                        --prefix "${ARCFLOW_INSTALL_PREFIX}"
                RESULT_VARIABLE result)
if(NOT result EQUAL 0 OR NOT EXISTS "${external_root}/projects/user-project.afproj" OR
   NOT EXISTS "${external_root}/settings/preferences")
    message(FATAL_ERROR "upgrade installation failed or modified user data")
endif()
include("${ARCFLOW_BUILD_DIRECTORY}/cmake_uninstall.cmake")
if(NOT EXISTS "${external_root}/projects/user-project.afproj" OR
   NOT EXISTS "${external_root}/settings/preferences")
    message(FATAL_ERROR "uninstall removed user projects or settings")
endif()
