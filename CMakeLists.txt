cmake_minimum_required(VERSION 3.28)
project(softgpu C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(softgpu SHARED src/icd.c)
target_compile_definitions(softgpu PRIVATE SOFTGPU_BUILD)
target_link_libraries(softgpu PRIVATE gdi32 user32)

add_executable(harness src/harness.c)
target_link_libraries(harness PRIVATE softgpu gdi32 user32)

set_target_properties(softgpu harness PROPERTIES C_STANDARD 11
                      C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
