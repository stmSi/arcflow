find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
set(ARCFLOW_LEARNING_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/generated/arcflow")
if(EMSCRIPTEN)
    set(ARCFLOW_LEARNING_SITE "${CMAKE_CURRENT_BINARY_DIR}/site/learn")
else()
    set(ARCFLOW_LEARNING_SITE "${CMAKE_CURRENT_BINARY_DIR}/learn")
endif()
set(ARCFLOW_LEARNING_OUTPUTS
    "${ARCFLOW_LEARNING_GENERATED}/onboarding_topics.h"
    "${ARCFLOW_LEARNING_GENERATED}/onboarding_guides.inc"
    "${ARCFLOW_LEARNING_SITE}/index.html"
    "${ARCFLOW_LEARNING_SITE}/print.html"
    "${ARCFLOW_LEARNING_SITE}/learn.css"
    "${ARCFLOW_LEARNING_SITE}/learn.js")
add_custom_command(OUTPUT ${ARCFLOW_LEARNING_OUTPUTS}
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/build-learning.py"
        --generated "${ARCFLOW_LEARNING_GENERATED}" --site "${ARCFLOW_LEARNING_SITE}"
        --version "${PROJECT_VERSION} (${ARCFLOW_SOURCE_REVISION})"
    DEPENDS tools/build-learning.py docs/learning/catalog.json
        docs/learning/learn.css docs/learning/learn.js
    VERBATIM)
add_custom_target(arcflow_learning ALL DEPENDS ${ARCFLOW_LEARNING_OUTPUTS})
add_test(NAME learning_catalog COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_learning.py")
