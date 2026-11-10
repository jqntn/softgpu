# softgpu

A software GPU for Windows: an OpenGL driver (ICD) in plain C that renders on the CPU.

Requires CMake 3.28+ and a C11 compiler for Windows x86 or x64: MSVC, clang or MinGW-w64 GCC. With MSVC, run from the Developer Command Prompt for the target architecture. Use one build directory per architecture, for example `build-x86` for 32-bit games.

```sh
cmake -B build -G Ninja
cmake --build build
build\harness
```

`harness` loads the driver directly, clears the window to a color and checks the pixel. It prints `ok` on success.

`wgltest` is an ordinary OpenGL app. It goes through `opengl32.dll`, so it needs the display adapter's `OpenGLDriverName` (x64) or `OpenGLDriverNameWow` (x86) to point at `softgpu.dll`. It fails unless the renderer is softgpu, then runs the same check.
