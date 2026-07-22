# Dependency manifest. Rules (see .vscode/docs/build.md):
#   - source deps pinned to full commit SHAs, never branches or moving tags
#   - binary artifacts pinned by URL + SHA256
#   - upgrades are deliberate one-line commits
include(FetchContent)

set(FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/third_party/_deps")
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

# Every dependency below is declared SYSTEM: its headers are included with
# -isystem, so warnings raised inside them are suppressed even when the code is
# instantiated in our translation units (header-only libraries like VMA and GLM).
# This is what keeps -Werror on first-party code without patching third-party
# headers or weakening our own flags.

# --- SDL3 --- release-3.4.12, pinned 2026-07-11
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3
    SYSTEM
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG f87239e71e42da91ca317a12eefb82cfbf3393eb)

# --- Vulkan-Headers (includes Vulkan-Hpp) --- v1.4.356, pinned 2026-07-11
# Provides Vulkan::Headers. No Vulkan SDK is required to build: we compile against
# these headers and load entry points at runtime via the Vulkan-Hpp dynamic
# dispatcher (VK_NO_PROTOTYPES; nothing links vulkan-1.lib).
FetchContent_Declare(VulkanHeaders
    SYSTEM
    GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
    GIT_TAG e613b8277f4840b3f3292867bbd3639950676c37)

# --- VulkanMemoryAllocator --- v3.4.0, pinned 2026-07-11
FetchContent_Declare(VulkanMemoryAllocator
    SYSTEM
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
    GIT_TAG 3aa921224c154a0d2c43912bc88e1c42ce1f7607)

# --- GLM --- 1.0.3, pinned 2026-07-11
# Configured (defines, conventions) only via engine/core/math.hpp.
FetchContent_Declare(glm
    SYSTEM
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 8d1fd52e5ab5590e2c81768ace50c72bae28f2ed)

# --- Dear ImGui --- docking branch @ 6029ee37, pinned 2026-07-11
# No upstream CMakeLists: MakeAvailable only populates sources. The imgui static
# library target (core + SDL3/Vulkan backends) is created when app-layer UI lands.
FetchContent_Declare(imgui
    SYSTEM
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG 6029ee3789a2b7898f6423ec0c88cc4e5425f5a9)

# --- Slang --- v2026.13 official prebuilt release, pinned 2026-07-11
# Never built from source (it is a compiler). Imported target is created when the
# shader system lands; fetching now keeps bootstrap = one configure.
FetchContent_Declare(slang
    SYSTEM
    URL https://github.com/shader-slang/slang/releases/download/v2026.13/slang-2026.13-windows-x86_64.zip
    URL_HASH SHA256=6bae110faa3b51ea00316e0a1dd4f40b11eb196a2e3a135074300bf2d5d4c777)

# --- future, same rules: libktx, EnTT, Jolt, meshoptimizer ---

# --- single headers, downloaded like any pinned artifact (URL + SHA256) ---
set(SINGLE_HEADERS_DIR "${FETCHCONTENT_BASE_DIR}/single-headers")
function(fetch_single_header subdir name url sha256)
    file(DOWNLOAD "${url}" "${SINGLE_HEADERS_DIR}/${subdir}/${name}"
         EXPECTED_HASH SHA256=${sha256} STATUS status)
    list(GET status 0 code)
    if(NOT code EQUAL 0)
        message(FATAL_ERROR "download failed: ${url} (${status})")
    endif()
endfunction()

# cgltf v1.15
fetch_single_header(cgltf cgltf.h
    https://raw.githubusercontent.com/jkuhlmann/cgltf/v1.15/cgltf.h
    e378a21c084bf1f288bb799de827bb26906efb024255f1ecf1705ea13f11c6ec)   # sha
# stb_image v2.30, pinned by commit
fetch_single_header(stb stb_image.h
    https://raw.githubusercontent.com/nothings/stb/31c1ad37456438565541f4919958214b6e762fb4/stb_image.h
    594c2fe35d49488b4382dbfaec8f98366defca819d916ac95becf3e75f4200b3)

# SYSTEM include keeps -Werror off them.
add_library(single_headers INTERFACE)
target_include_directories(single_headers SYSTEM INTERFACE "${SINGLE_HEADERS_DIR}")

FetchContent_MakeAvailable(SDL3 VulkanHeaders VulkanMemoryAllocator glm imgui slang)

# The prebuilt slang package ships its own CMake config (imported target slang::slang).
find_package(slang REQUIRED CONFIG NO_DEFAULT_PATH PATHS "${slang_SOURCE_DIR}/cmake")

# imgui ships no CMakeLists; core + SDL3/Vulkan backends only
add_library(imgui STATIC
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp"
)
target_include_directories(imgui SYSTEM PUBLIC
    "${imgui_SOURCE_DIR}"
    "${imgui_SOURCE_DIR}/backends"
)

target_compile_definitions(imgui PUBLIC VK_NO_PROTOTYPES)
target_link_libraries(imgui PUBLIC SDL3::SDL3 Vulkan::Headers)
add_library(imgui::imgui ALIAS imgui)
