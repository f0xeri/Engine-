set(GLTF_SAMPLES_DIR "${CMAKE_SOURCE_DIR}/assets/external/gltf-sample-models")
set(GLTF_SAMPLES_SHA d7a3cc8e51d7c573771ae77a57f16b0662a905c6)

if(NOT EXISTS "${GLTF_SAMPLES_DIR}/2.0/Sponza/glTF/Sponza.gltf")
    message(STATUS "Fetching Sponza sample assets (sparse, ~90 MB)...")
    find_package(Git REQUIRED)

    file(MAKE_DIRECTORY "${GLTF_SAMPLES_DIR}")
    foreach(step
        "init;-q"
        "remote;add;origin;https://github.com/KhronosGroup/glTF-Sample-Models.git"
        "sparse-checkout;set;2.0/Sponza"
        "fetch;-q;--depth;1;--filter=blob:none;origin;${GLTF_SAMPLES_SHA}"
        "checkout;-q;${GLTF_SAMPLES_SHA}")
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" ${step}
            WORKING_DIRECTORY "${GLTF_SAMPLES_DIR}"
            RESULT_VARIABLE result)
        if(NOT result EQUAL 0)
            message(FATAL_ERROR "sample-asset fetch failed at: git ${step}")
        endif()
    endforeach()
endif()
