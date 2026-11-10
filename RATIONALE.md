# Rationale

softgpu is a software GPU for Windows: an OpenGL installable client driver (ICD) in plain C that renders on the CPU. Its goal is to run real OpenGL games as the system's OpenGL driver. This file records the design rules. Follow them in every change.

## Principles

- **Ultra simple.** Write the smallest code that does the job. Add an abstraction, option or helper only when code needs it. Prefer deletion to addition.
- **Own implementation.** No Mesa or other OpenGL implementation. Khronos tools are allowed.
- **Fail early and explicitly.** A bad API call returns the error value that WGL or OpenGL defines. Running out of memory stops the process with a message.
- **Windows x64 only.** The ICD interface exists only on Windows. The build fails on other targets.

## Code

- **C11.** `_Thread_local` holds the current context and `_Static_assert` guards the target. CMake passes `/std:c11`.
- **Format.** clang-format uses Google style, left pointer alignment and braces on every control statement. `IncludeBlocks: Preserve` keeps `<windows.h>` before `<GL/gl.h>`. Files use LF line endings.
- **Names.** clang-tidy enforces Google naming as errors, with the `SOFTGPU_` macro prefix.
- **No comments in code.**
- **Limits.** Use fixed-size arrays with explicit limits. Contexts live in a 64-slot array.
- **Commits.** Make atomic commits with a Conventional Commits subject line only.

## ICD

- **Loading.** `opengl32.dll` loads the DLL named by the `OpenGLDriverName` value under the display adapter's key, `HKLM\SYSTEM\CurrentControlSet\Control\Class\{4d36e968-e325-11ce-bfc1-08002be10318}\000X`. 32-bit apps use `OpenGLDriverNameWow`.
- **Exports.** The 18 `Drv*` functions that `opengl32.dll` calls, from Mesa's `gldrv.h`.
- **Dispatch.** `DrvSetContext` returns the GL 1.1 table of 336 entries. The order comes from `GLDISPATCHTABLE` in Mesa's `gldrv.h`. Unimplemented entries share one stub that returns 0, which is valid only under the x64 calling convention.
- **Handles.** `DHGLRC` is a 32-bit `ULONG`, so a context handle is its slot index plus one, not a pointer. 0 means failure.
- **Pixel format.** One format: RGBA8, double-buffered, drawn to a window. No depth or stencil yet.
- **Framebuffer.** It lives in the context and matches the window's client area at make-current and after each swap. It is a bottom-up 32-bit DIB, so rows are in GL order. `StretchDIBits` presents it.

## Development

- **harness** links the ICD directly and plays the part of `opengl32.dll`. It checks the exports, clears to a color, swaps and reads the pixel back. Nothing touches the registry.
- **Registry install** changes a system-wide setting. Back up the original value first. GPU driver updates overwrite it.

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
| 32-bit build | A 32-bit game. It needs a stub per signature and `OpenGLDriverNameWow`. |
| Framebuffer per window | An app draws into one window from two contexts, or swaps a DC that is not current. |
| Visible error messages | The first GUI app through the registry. `stderr` is invisible there. |
| Depth and stencil formats | Depth testing. |
| SIMD clear | A profile shows it. |
