add_executable(sandbox main.cpp)

target_link_libraries(sandbox PRIVATE engine engine_warnings)

target_compile_definitions(sandbox PRIVATE
    ENGINE_SHADER_DIR="${CMAKE_SOURCE_DIR}/assets/shaders"
    ENGINE_PIPELINE_CACHE="${CMAKE_BINARY_DIR}/pipeline_cache.bin"
    ENGINE_SAMPLE_ASSETS_DIR="${GLTF_SAMPLES_DIR}"
)

# slang.dll loads its sibling DLLs and the standard module from its own directory.
add_custom_command(TARGET sandbox POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${slang_SOURCE_DIR}/bin/slang.dll"
        "${slang_SOURCE_DIR}/bin/slang-compiler.dll"
        "${slang_SOURCE_DIR}/bin/slang-glslang.dll"
        "${slang_SOURCE_DIR}/bin/slang-glsl-module.dll"
        "${slang_SOURCE_DIR}/bin/slang-standard-module-2026.13"
        $<TARGET_FILE_DIR:sandbox>
)
