cmake_minimum_required(VERSION 3.28)
project(softgpu C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

FetchContent_Declare(
  glfw
  URL https://github.com/glfw/glfw/archive/refs/tags/3.5.1.tar.gz
  URL_HASH SHA256=5234f4f29473e9a06bc7847d8371858dd135d38466eeeaa652fdc9f8f9ff0c20
  EXCLUDE_FROM_ALL SYSTEM)

FetchContent_MakeAvailable(glfw)

add_library(softgpu SHARED src/icd.c src/icd.def)
set_target_properties(softgpu PROPERTIES PREFIX "")
target_compile_options(softgpu
                       PRIVATE $<$<C_COMPILER_ID:MSVC>:/experimental:c11atomics>)

add_executable(harness test/harness.c)
target_include_directories(harness PRIVATE src)
target_link_libraries(harness PRIVATE softgpu)

add_executable(wgltest test/wgltest.c)
target_link_libraries(wgltest PRIVATE opengl32)

add_executable(triangle sample/triangle.c)
target_link_libraries(triangle PRIVATE glfw opengl32)

set_target_properties(softgpu harness wgltest triangle
                      PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON
                                 C_EXTENSIONS OFF)
