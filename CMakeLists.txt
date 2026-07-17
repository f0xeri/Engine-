cmake_minimum_required(VERSION 3.28)

project(Engine LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Warnings-as-errors applies to first-party code only (engine/, sandbox/, tools/);
# third-party targets build with their own defaults.
add_library(engine_warnings INTERFACE)
target_compile_options(engine_warnings INTERFACE -Wall -Wextra -Werror)

include(cmake/dependencies.cmake)
include(cmake/assets.cmake)

add_subdirectory(engine)
add_subdirectory(sandbox)
