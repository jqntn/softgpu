# softgpu

A software GPU for Windows: an OpenGL driver (ICD) in plain C that renders on the CPU.

Requires CMake 3.28+ and a C11 compiler for Windows x64: MSVC, clang or MinGW-w64 GCC. With MSVC, run from a Developer Command Prompt (x64).

```sh
cmake -B build -G Ninja
cmake --build build
build\harness
```

`harness` loads the driver directly, clears the window to a color and checks the pixel. It prints `ok` on success.
