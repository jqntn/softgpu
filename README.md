# softgpu

A software GPU for Windows: an OpenGL driver (ICD) in plain C that renders on the CPU.

Requires CMake 3.28+ and a C11 compiler for Windows x86 or x64: MSVC, clang or MinGW-w64 GCC. With MSVC, run from the Developer Command Prompt for the target architecture. Use one build directory per architecture, for example `build-x86` for 32-bit games.

```sh
cmake -B build -G Ninja
cmake --build build
build\harness
```

`harness` loads the driver directly, clears the window to a color and checks the pixel. It prints `ok` on success.

`wgltest` is an ordinary OpenGL app. It goes through `opengl32.dll`, so it needs softgpu installed as the system's OpenGL driver. It fails unless the renderer is softgpu, then runs the same check.

## Install

Build `build` (x64) and `build-x86` (x86), then run from an elevated PowerShell:

```powershell
scripts\install.ps1
scripts\uninstall.ps1
```

`install.ps1` makes softgpu the OpenGL driver of every display adapter, for 64-bit and 32-bit apps. Every OpenGL app uses it until `uninstall.ps1` restores the original drivers. A GPU driver update also undoes the install. If script execution is disabled, run `powershell -ExecutionPolicy Bypass -File scripts\install.ps1`.
