cmake_minimum_required(VERSION 3.28)
project(softgpu C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(softgpu SHARED src/icd.c)
set_target_properties(softgpu PROPERTIES PREFIX "")
target_compile_definitions(softgpu PRIVATE SOFTGPU_BUILD)

add_executable(harness src/harness.c)
target_link_libraries(harness PRIVATE softgpu)

set_target_properties(softgpu harness PROPERTIES C_STANDARD 11
                      C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
