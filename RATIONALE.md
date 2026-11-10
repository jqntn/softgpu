# Rationale

softgpu is a software GPU for Windows: an OpenGL installable client driver (ICD) in plain C that renders on the CPU. Its goal is to run real OpenGL games as the system's OpenGL driver. This file records the design rules. Follow them in every change.

## Principles

- **Ultra simple.** Write the smallest code that does the job. Add an abstraction, option or helper only when code needs it. Prefer deletion to addition.
- **Own implementation.** No Mesa or other OpenGL implementation. Khronos tools are allowed.
- **Self-contained.** The driver has no dependencies. CMake fetches the sample's GLFW as a tag archive with a SHA-256 hash, as ray does.
- **Standard C first.** Use standard C over the Win32 API when both do the job.
- **Fail early and explicitly.** A bad API call returns the error value that WGL or OpenGL defines. Running out of memory stops the process with a message.
- **Windows only.** The ICD interface exists only on Windows. softgpu builds for x86 and x64, because 32-bit games load a 32-bit driver.
- **Any Windows toolchain.** MSVC, clang and MinGW-w64 GCC. CI builds all three for x86 and x64 and runs the harness.

## Code

- **C11.** `_Thread_local` holds the current context, `<stdatomic.h>` claims context slots and `_Static_assert` checks the size of the dispatch table. CMake passes the compiler's C11 flag.
- **Format.** clang-format uses Google style, left pointer alignment and braces on every control statement. `<GL/gl.h>` needs `<windows.h>` first, so each file puts code between them, where include sorting cannot reorder them: the `SOFTGPU_GL11` X-macro in `icd.h`, the pixel format in `test/wgltest.c`. Files use LF line endings.
- **Names.** clang-tidy enforces Google naming as errors, with the `SOFTGPU_` macro prefix.
- **No comments in code.**
- **Limits.** Use fixed-size arrays with explicit limits. Contexts live in a 64-slot array.
- **Commits.** Make atomic commits with a Conventional Commits subject line only.

## Build

- **DLL name.** The ICD is `softgpu.dll` on every toolchain. MinGW would otherwise name it `libsoftgpu.dll`.
- **C11 atomics on MSVC.** MSVC still marks them experimental, so CMake passes `/experimental:c11atomics` to MSVC only.
- **System libraries.** CMake links `kernel32`, `user32` and `gdi32` by default on every Windows toolchain, so no target lists them.
- **Compile commands.** CMake exports `compile_commands.json`, so clang-tidy sees the same defines, standard and include paths as the compiler.
- **x86 GCC in CI.** The runner ships only 64-bit MinGW, so the x86 job installs `mingw-w64-i686-gcc` from the runner's MSYS2.

## ICD

- **Loading.** `opengl32.dll` loads the DLL named by the `OpenGLDriverName` value under the display adapter's key, `HKLM\SYSTEM\CurrentControlSet\Control\Class\{4d36e968-e325-11ce-bfc1-08002be10318}\000X`. 32-bit apps use `OpenGLDriverNameWow`, which names the x86 build.
- **Exports.** The 18 `Drv*` functions that `opengl32.dll` calls, from Mesa's `gldrv.h`. `icd.def` exports them by their plain names. On x86, `__stdcall` would otherwise decorate them as `_DrvCreateContext@4`, and `opengl32.dll` looks them up undecorated.
- **Dispatch.** `DrvSetContext` returns the GL 1.1 table of 336 entries. The order, return types and parameters come from `GLDISPATCHTABLE` in Mesa's `gldrv.h`, as the `SOFTGPU_GL11` X-macro. Each unimplemented entry gets a typed stub that returns 0 and prints `unimplemented gl<Name>` to `stderr` on its first call, so a real app lists what to implement next.
- **Handles.** `DHGLRC` is a 32-bit `ULONG`, so a context handle is its slot index plus one, not a pointer. 0 means failure.
- **Pixel format.** One format: RGBA8, double-buffered, drawn to a window. It sets `PFD_SUPPORT_COMPOSITION`, as Mesa does. Without it, `opengl32.dll` never calls `DrvCreateContext`, because Windows 8 and later always composite the desktop. It has no depth or stencil yet, so `ChoosePixelFormat` skips it unless the app sets `PFD_DEPTH_DONTCARE`, as `wgltest` does.
- **Framebuffer.** It lives in the context and matches the window's client area at make-current and after each swap. It is a bottom-up 32-bit DIB, so rows are in GL order. `StretchDIBits` presents it.

## Rasterizer

- **Immediate mode.** `glBegin(GL_TRIANGLES)`, `glColor3f` and `glVertex2f` fill a three-vertex buffer, and every third vertex draws a triangle at once. Other primitive modes report themselves as unimplemented. There are no matrices yet, so vertices are in clip space with w = 1.
- **Viewport.** It starts at the window size when the context first becomes current. Window coordinates clamp to a guard band of 2^19 pixels, which keeps the fixed-point math within 64 bits. Real clipping comes with matrices.
- **Triangles.** Edge functions in 64-bit fixed point with 4 subpixel bits, sampled at pixel centers. Both windings draw, because culling is off by default. A top-left rule in GL's y-up space keeps shared edges from being drawn twice. Colors interpolate linearly from barycentric weights, which is exact while w = 1.
- **Flush.** `glFlush` does nothing, because drawing is synchronous.

## Development

- **Layout.** `src` holds the driver, `test` the checks and `sample` the demos.
- **harness** links the ICD directly and plays the part of `opengl32.dll`. It checks the exports, clears, draws a triangle, swaps and reads back one pixel inside the triangle and one outside. Nothing touches the registry. CI runs it for every toolchain and architecture.
- **wgltest** is an ordinary WGL app linked against `opengl32.dll`, so it checks the whole chain once the registry points at softgpu. It fails unless `GL_RENDERER` is `softgpu`, then clears, swaps and reads the pixel back. The x86 build checks `OpenGLDriverNameWow`. CI builds it but cannot run it, because the runner has no softgpu registration.
- **triangle** in `sample` draws a colored triangle with GLFW and GL 1.1 immediate mode. softgpu renders it. GLFW chooses the pixel format itself and skips GDI Generic, so it picks softgpu's format without depth. CI builds it but cannot run it, because it loops until its window closes.
- **Reports.** Errors and unimplemented calls go to `stderr`. GUI games inherit it too, so redirect it at launch: `game.exe 2> softgpu.log` in cmd, or `Start-Process game.exe -RedirectStandardError softgpu.log -Wait` in PowerShell. PowerShell's `2>` captures nothing from a GUI app.
- **Install.** `scripts/install.ps1` points `OpenGLDriverName` and `OpenGLDriverNameWow` of every display adapter with an OpenGL driver at the x64 and x86 builds, after it checks each DLL's architecture. It keeps the original values beside them as `SoftgpuOpenGLDriverName` and `SoftgpuOpenGLDriverNameWow`, and a second run keeps the first backup. It keeps each value's registry type. `scripts/uninstall.ps1` restores the originals, unless a driver update already replaced softgpu. Both need an elevated PowerShell and work in Windows PowerShell 5.1 and PowerShell 7. A GPU driver update undoes the install. Windows caches the driver name when the display driver starts, so both scripts end with `scripts/restart.ps1`. It restarts every display device that has an OpenGL driver with `pnputil /restart-device`, which blanks the screen for a few seconds and resets the GPU for running apps. A device that cannot restart now restarts at the next reboot.

## Direction

- **First milestone.** GL 1.x fixed-function games (Quake 3 era). Record them with apitrace, implement what the traces call, and replay the traces as regression tests against the real GPU's images.
- **Shaders.** GLSL through glslang to SPIR-V, then softgpu's own IR, an interpreter first and a JIT later. Fixed function becomes generated internal shaders.
- **Speed.** AVX2 over pixel groups and screen tiles across threads.

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
| Depth and stencil formats | Depth testing, or the first real app, since apps rarely set `PFD_DEPTH_DONTCARE`. |
| SIMD clear and rasterizer | A profile shows it. |
