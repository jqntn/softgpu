# softgpu

A software GPU for Windows: an OpenGL driver in plain C that renders on the CPU.

Requires CMake 3.28+ and a C11 compiler: MSVC (from a Developer Command Prompt), clang or MinGW-w64 GCC. Build x64 in `build` and x86 in `build-x86`.

```sh
cmake -B build -G Ninja
cmake --build build
build\harness
```

To make softgpu the system's OpenGL driver, run from an elevated PowerShell:

```powershell
scripts\install.ps1
build\wgltest
scripts\uninstall.ps1
```

Every OpenGL app uses softgpu until you uninstall.
