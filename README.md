# softgpu

A software GPU for Windows: an OpenGL driver (ICD) in plain C that renders on the CPU.

## Build

Requires CMake 3.28+ and a C11 compiler: MSVC, clang or MinGW-w64 GCC. CMake fetches GLFW for the sample. With MSVC, use the Developer Command Prompt of each architecture. 32-bit apps need the x86 build.

```sh
cmake -B build -G Ninja
cmake --build build
cmake -B build-x86 -G Ninja
cmake --build build-x86
```

## Test

`build\harness` loads the driver directly, clears a window, draws a triangle and checks a pixel inside and outside it. It prints `ok` on success.

`build\wgltest` is an ordinary OpenGL app that goes through `opengl32.dll`, clears a window and checks the pixel. It needs softgpu installed and fails unless the renderer is softgpu.

## Sample

`build\triangle` draws a colored triangle with GLFW and OpenGL 1.1. With softgpu installed, softgpu renders it.

## Install

With both builds done, run from an elevated PowerShell:

```powershell
scripts\install.ps1
scripts\uninstall.ps1
```

`install.ps1` makes softgpu the OpenGL driver of every display adapter, for 64-bit and 32-bit apps. Every OpenGL app uses it until `uninstall.ps1` restores the original drivers. Both take effect after a reboot or a restart of the display driver. A GPU driver update also undoes the install.
