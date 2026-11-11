# Rationale

softgpu is a software OpenGL driver for Windows, in plain C. Its goal is to run real OpenGL games as the system's OpenGL driver. This file records the design rules. Follow them in every change.

## Principles

- **Ultra simple.** Write the smallest code that does the job. Prefer deletion to addition.
- **Own implementation.** No Mesa or other OpenGL implementation. Khronos tools are allowed.
- **Self-contained.** The driver has no dependencies. Sample dependencies are hashed archives fetched by CMake.
- **Standard C first.** Use standard C over the Win32 API when both do the job.
- **Fail early and explicitly.** A bad API call returns the error value that WGL or OpenGL defines. Running out of memory stops the process.
- **Windows, x86 and x64, any toolchain.** 32-bit games load a 32-bit driver. MSVC, clang and MinGW-w64 GCC must all build it.

## Code

- **C11** for `_Thread_local` and `<stdatomic.h>`. MSVC still needs `/experimental:c11atomics`.
- **Include order.** `<GL/gl.h>` needs `<windows.h>` first. Put code between them, so include sorting cannot reorder them.
- **No comments in code.**
- **Limits.** Use fixed-size arrays with explicit limits.
- **Commits.** Atomic, with a Conventional Commits subject line only.

## Build

- **DLL name.** `PREFIX ""` keeps `softgpu.dll` on MinGW, which would add `lib`.
- **System libraries.** CMake links `kernel32`, `user32` and `gdi32` by default on every toolchain, so no target lists them.

## ICD

- **Loading.** `opengl32.dll` loads the DLL named by `OpenGLDriverName` (`OpenGLDriverNameWow` for 32-bit apps) in the display adapter's registry key. Windows caches the name when the display driver starts.
- **Exports.** `icd.def` exports the `Drv*` functions by plain name, because x86 `__stdcall` decorates them and `opengl32.dll` looks them up undecorated.
- **Dispatch.** The 336 GL 1.1 entries follow `GLDISPATCHTABLE` in Mesa's `gldrv.h`. Each unimplemented entry is a typed stub that returns 0 and reports itself on `stderr` once, so a real app lists what to implement next.
- **Handles.** `DHGLRC` is a 32-bit `ULONG`, so a context handle is its slot index plus one.
- **Pixel format.** It must set `PFD_SUPPORT_COMPOSITION`, or `opengl32.dll` never calls `DrvCreateContext`. It has 24 depth and 8 stencil bits. `ChoosePixelFormat` skips a format without depth unless the app sets `PFD_DEPTH_DONTCARE`, and SDL2 skips one with fewer depth or stencil bits than the app asks for, so it picks Microsoft's GDI Generic renderer instead.
- **Framebuffer.** A bottom-up 32-bit DIB, so rows are in GL order. `StretchDIBits` presents it.

## Rasterizer

- **GL 1.1 fixed function.** Immediate-mode triangles, quads and polygons, without lighting, lines or points. The depth compare runs before texturing to skip hidden fragments, unless the stencil test is on, since a depth failure then still updates the stencil. The depth write waits for the alpha test, which keeps GL 1.1's results.
- **Clipping.** Triangles are clipped in clip space against the six frustum planes, so window coordinates stay inside the viewport.
- **Fixed point.** Edge functions in 64-bit with 4 subpixel bits. A guard band of 2^19 pixels bounds huge viewports, so products reach 2^48.
- **Interpolation.** Attributes are perspective-correct. Depth is linear in window space and stored as float.
- **Textures.** One table of 4096 names shared by all contexts, as `DrvShareLists` promises. Power-of-two RGBA8 images up to 2048, with `GL_REPEAT` and GL 1.1's mipmap selection.
- **Fill rule.** Both windings draw. `GL_CULL_FACE` culls by window-space winding, as `glFrontFace` and `glCullFace` set. A top-left rule in GL's y-up space draws shared edges once. Its bias stays out of the interpolation weights, or depth drifts on small triangles and hidden faces show through.

## Development

- **harness** plays the part of `opengl32.dll`, so CI can run it without installing the driver.
- **wgltest** and the samples need softgpu installed, so CI only builds them.
- **Reports.** GUI games inherit `stderr`, so redirect it at launch: `game.exe 2> softgpu.log` in cmd, or `Start-Process game.exe -RedirectStandardError softgpu.log -Wait` in PowerShell. PowerShell's `2>` captures nothing from a GUI app.
- **Install.** The scripts keep the original driver names as `Softgpu<Name>` values beside them and keep each value's registry type. A second install keeps the first backup. Uninstall leaves a name alone when a driver update already replaced softgpu. `restart.ps1` restarts the display devices, because of the cached name. The scripts must run in Windows PowerShell 5.1 and PowerShell 7.

## Direction

- **First milestone.** GL 1.x fixed-function games (Quake 3 era), driven by apitrace captures replayed as regression tests.
- **Shaders.** GLSL through glslang to SPIR-V, then softgpu's own IR, interpreted first and JIT-compiled later. Fixed function becomes generated shaders.
- **Speed.** AVX2 over pixel groups, and screen tiles across threads.

## Rejected

| Option | Reason |
|---|---|
| OpenGL ES | Windows games call desktop OpenGL through WGL. `opengl32.dll` loads no ES driver. |
| Vulkan ICD | Cleaner per feature, but far more work before the first game runs. |
| Mesa | The goal is an own implementation. |
| WDDM driver | A kernel driver and signing, only to appear as a separate adapter. |
| C++ | SIMD intrinsics are a C API. |

## Deferred

| Option | Trigger |
|---|---|
| Framebuffer per window | An app draws into one window from two contexts, or swaps a DC that is not current. |
| Log file | A game that only starts through a launcher, so nobody can redirect its `stderr`. |
| SIMD and 32-bit edge stepping | A profile shows the rasterizer. Step in 32-bit inside tiles after 64-bit setup. |
