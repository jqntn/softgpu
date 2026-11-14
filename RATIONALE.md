# Rationale

softgpu is a software OpenGL driver for Windows, in plain C. Its goal is to run real OpenGL games as the system's OpenGL driver. This file records the design rules. Follow them in every change.

## Principles

- **Ultra simple.** Write the smallest code that does the job. Prefer deletion to addition.
- **Readable.** The driver stays easy to read. Speed comes from structure, like threads and tiles, before low-level tricks.
- **Own implementation.** No Mesa or other OpenGL implementation. Khronos tools are allowed.
- **Self-contained.** The driver may depend only on small official tools, like Khronos' glslang, and needs none today. Dependencies are hashed archives fetched by CMake.
- **Standard C first.** Use standard C over the Win32 API when both do the job.
- **Fail early and explicitly.** A bad API call returns the error value that WGL or OpenGL defines. Running out of memory stops the process.
- **Windows, x86 and x64, any toolchain.** 32-bit games load a 32-bit driver. MSVC, clang and MinGW-w64 GCC must all build it.

## Code

- **C11** for `_Thread_local` and `<stdatomic.h>`. MSVC still needs `/experimental:c11atomics`.
- **Include order.** `<GL/gl.h>` needs `<windows.h>` first. Put code between them, so include sorting cannot reorder them.
- **No comments in code.**
- **Limits.** Use fixed-size arrays with explicit limits.
- **Layout.** One file, so the per-pixel path inlines without link-time optimization. `SOFTGPU_INLINE` forces the per-pixel functions MSVC still leaves out of line. Each function comes before its first caller, in pipeline order.
- **Commits.** Atomic, with a Conventional Commits subject line only.

## Build

- **DLL name.** `PREFIX ""` keeps `softgpu.dll` on MinGW, which would add `lib`.
- **System libraries.** CMake links `kernel32`, `user32` and `gdi32` by default on every toolchain, so softgpu only lists `dwmapi`.

## ICD

- **Loading.** `opengl32.dll` loads the DLL named by `OpenGLDriverName` (`OpenGLDriverNameWow` for 32-bit apps) in the display adapter's registry key. Windows caches the name when the display driver starts.
- **Exports.** `icd.def` exports the `Drv*` functions by plain name, because x86 `__stdcall` decorates them and `opengl32.dll` looks them up undecorated.
- **Dispatch.** The 336 GL 1.1 entries follow `GLDISPATCHTABLE` in Mesa's `gldrv.h`, the layout `opengl32.dll` indexes. Extensions come through `DrvGetProcAddress`, since the table holds GL 1.1 only.
- **Handles.** `DHGLRC` is a 32-bit `ULONG`, so a context handle is its slot index plus one.
- **Pixel formats.** They must set `PFD_SUPPORT_COMPOSITION`, or `opengl32.dll` never calls `DrvCreateContext`. They come double- and single-buffered, all with depth and stencil, and only some add an accumulation buffer, or Microsoft's GDI Generic renderer wins: `ChoosePixelFormat` skips formats without depth and falls back to it when no format matches the app's buffering, and SDL2 prefers it to a format with an accumulation buffer the app did not ask for.
- **Framebuffer.** Each window has one, shared by every context drawing to it, as WGL requires. Color buffers are bottom-up DIBs, so rows are in GL order. `DrvSwapBuffers` copies back to front, as `PFD_SWAP_COPY` promises.
- **Swap interval.** `WGL_EXT_swap_control` is listed by both extension string queries, since SDL2 only asks the ARB one. GDI cannot flip on the refresh, so a swap waits for DWM's next composition with `DwmFlush`. A timer, as in Mesa's WGL, drifts from the refresh.

## OpenGL

- **GL 1.1 fixed function.** All of GL 1.1 in RGBA mode, the version softgpu reports. The depth compare runs before texturing to skip hidden fragments, unless the stencil test is on, since a depth failure then still updates the stencil. The depth write waits for the alpha test, which keeps GL 1.1's results.
- **Extensions.** Only those real games need: `GL_ARB_multitexture` and `GL_ARB_texture_env_combine` let QuakeSpasm light its world in one pass, and `GL_EXT_texture_env_add` keeps its EXT name beside the ARB one, since Quake 3 only looks for the EXT name.
- **Array enables.** `glEnable` and `glDisable` also take the six array names and act as `glEnableClientState`, as NVIDIA and Mesa do. GL 1.1 would raise `GL_INVALID_ENUM`, but code tested only on those drivers relies on it.
- **Dithering.** Every channel rounds to the nearest 8-bit value, with `GL_DITHER` on or off. GL 4.2 and later allow this. GL 1.1 asks for truncation when dithering is off, but dithering means nothing at 8 bits per channel, and NVIDIA and Mesa ignore it too.

## Rasterizer

- **Fixed point.** Window x and y snap to 4 subpixel bits, so culling and every rasterizer see the same positions. Edge functions are 64-bit, and clipping and the viewport size limit keep their products in range. A guard band keeps window coordinates convertible to `int`.
- **Fill rule.** A top-left rule in GL's y-up space draws shared edges once. Its bias stays out of the interpolation weights, or depth drifts on small triangles and hidden faces show through.
- **Interpolation.** Attributes are perspective-correct. Depth interpolates from the first corner, so a flat polygon keeps its exact depth and one polygon offset unit still separates it. Fog interpolates signed eye z and takes its magnitude per fragment, so a polygon crossing the eye plane is not fogged near the eye.
- **Lines and points.** Aliased lines step the major axis at pixel centers, which the spec allows in place of the diamond-exit rule. A smooth polygon samples its whole outline, so its fan triangles leave no seams.
- **Float math.** MSVC calls `fminf`, `fmaxf` and `floorf` out of line, so pixel code uses its own versions with the same results. `log2f` stays for mipmapping, since an approximation would change pixels and an exact own version ran slower.

## Development

- **harness** plays the part of `opengl32.dll`, so CI can run it without installing the driver.
- **wgltest** and the samples need softgpu installed, so CI only builds them.
- **Piglit.** Its OpenGL 1.0, 1.1 and WGL groups pass, except tests that use later features without checking the version.
- **Install.** The scripts keep the original driver names, so uninstall can restore them, unless a driver update already replaced softgpu. Display devices restart, because Windows caches the name. The scripts must run in Windows PowerShell 5.1 and PowerShell 7.
- **Benchmarks.** `bench` and `scripts\quake-bench.ps1` run unseen and pinned to the performance cores, since Windows moves unfocused programs to efficiency cores. The Quake script runs a copy of QuakeSpasm without the sleep it takes when unfocused. Results repeat within 1%.

## Direction

- **Scope.** OpenGL 1.1 fixed-function games, driven by apitrace captures replayed as regression tests. Piglit's OpenGL 1.0 and 1.1 tests check conformance.
- **Speed.** Plain C, in this order: no C runtime calls per pixel, screen tiles across threads, block coverage tests in the rasterizer, then pixel loops over small blocks that compilers vectorize. A profile picks each step, and none may cost readability. Profiles and tuning use MSVC Release builds; the other toolchains only have to build.

## Rejected

| Option | Reason |
|---|---|
| OpenGL ES | Windows games call desktop OpenGL through WGL. `opengl32.dll` loads no ES driver. |
| Vulkan ICD | Cleaner per feature, but far more work before the first game runs. |
| Mesa | The goal is an own implementation. |
| WDDM driver | A kernel driver and signing, only to appear as a separate adapter. |
| C++ | C keeps the driver small and readable. SIMD intrinsics are a C API too. |
| AVX2 | CPUs without it need a runtime check and a second code path kept identical. Threads give more for that cost. |
| LLVM | A large C++ dependency, for code generation that GL 1.1 does not need. |

## Deferred

| Option | Trigger |
|---|---|
| Hand-written SSE2 | After the plain-C speed steps, a profile still shows per-pixel math that the compiler did not vectorize. |
| 32-bit edge stepping | A profile shows edge stepping, a small share of the time today. Step in 32-bit inside tiles after 64-bit setup. |
| GL 2.0 and shaders | A game that needs GL 2.0. GLSL then goes through glslang and the SPIRV-Tools optimizer to SPIR-V, decoded once and interpreted over pixel blocks, with no IR of softgpu's own. Fixed function moves onto shaders only once they run faster. |
