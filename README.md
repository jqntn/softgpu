# softgpu

A software GPU for Windows: an OpenGL driver (ICD) in plain C that renders on the CPU.

## Build

Requires CMake 3.28+ and a C11 compiler: MSVC, clang or MinGW-w64 GCC. With MSVC, use the Developer Command Prompt of each architecture. 32-bit apps need the x86 build.

```sh
cmake -B build -G Ninja
cmake --build build
cmake -B build-x86 -G Ninja
cmake --build build-x86
```

## Test

`build\harness` loads the driver directly, clears a window to a color and checks the pixel. It prints `ok` on success.

`build\wgltest` runs the same check as an ordinary OpenGL app, through `opengl32.dll`. It needs softgpu installed and fails unless the renderer is softgpu.

## Install

With both builds done, run from an elevated PowerShell:

```powershell
scripts\install.ps1
scripts\uninstall.ps1
```

`install.ps1` makes softgpu the OpenGL driver of every display adapter, for 64-bit and 32-bit apps. Every OpenGL app uses it until `uninstall.ps1` restores the original drivers. A GPU driver update also undoes the install. If script execution is disabled, run `powershell -ExecutionPolicy Bypass -File scripts\install.ps1`.
