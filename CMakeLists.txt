cmake_minimum_required(VERSION 3.28)
project(softgpu C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(softgpu SHARED src/icd.c src/icd.def)
set_target_properties(softgpu PROPERTIES PREFIX "")
target_compile_options(softgpu
                       PRIVATE $<$<C_COMPILER_ID:MSVC>:/experimental:c11atomics>)

add_executable(harness src/harness.c)
target_link_libraries(harness PRIVATE softgpu)

add_executable(wgltest src/wgltest.c)
target_link_libraries(wgltest PRIVATE opengl32)

set_target_properties(softgpu harness wgltest PROPERTIES C_STANDARD 11
                      C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
