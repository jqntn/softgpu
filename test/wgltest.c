#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <windows.h>

static const PIXELFORMATDESCRIPTOR kFormat = {
    .nSize = sizeof(PIXELFORMATDESCRIPTOR),
    .nVersion = 1,
    .dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER |
               PFD_DEPTH_DONTCARE,
    .iPixelType = PFD_TYPE_RGBA,
    .cColorBits = 32,
};

#include <GL/gl.h>

typedef const char*(WINAPI* ExtensionsFn)(HDC);
typedef BOOL(WINAPI* SwapIntervalFn)(int);

enum { kSwaps = 30 };

static void Fail(const char* message) {
  fprintf(stderr, "%s\n", message);
  exit(EXIT_FAILURE);
}

int main(void) {
  RegisterClassA(&(WNDCLASSA){
      .lpfnWndProc = DefWindowProcA,
      .hInstance = GetModuleHandleA(NULL),
      .lpszClassName = "wgltest",
  });
  HWND window = CreateWindowA("wgltest", "wgltest",
                              WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT,
                              CW_USEDEFAULT, 640, 480, NULL, NULL, NULL, NULL);
  if (!window) {
    Fail("CreateWindowA failed");
  }
  HDC hdc = GetDC(window);

  int format = ChoosePixelFormat(hdc, &kFormat);
  if (!format || !SetPixelFormat(hdc, format, &kFormat)) {
    Fail("no pixel format");
  }
  HGLRC context = wglCreateContext(hdc);
  if (!context || !wglMakeCurrent(hdc, context)) {
    Fail("no context");
  }

  const char* renderer = (const char*)glGetString(GL_RENDERER);
  if (!renderer || strcmp(renderer, "softgpu") != 0) {
    fprintf(stderr, "renderer is %s, not softgpu\n",
            renderer ? renderer : "unknown");
    return EXIT_FAILURE;
  }

  ExtensionsFn extensions =
      (ExtensionsFn)wglGetProcAddress("wglGetExtensionsStringARB");
  SwapIntervalFn swap_interval =
      (SwapIntervalFn)wglGetProcAddress("wglSwapIntervalEXT");
  if (!extensions || !strstr(extensions(hdc), "WGL_EXT_swap_control") ||
      !swap_interval || !swap_interval(1)) {
    Fail("no WGL_EXT_swap_control");
  }
  DEVMODEA mode = {.dmSize = sizeof(mode)};
  struct timespec start;
  struct timespec end;
  if (!EnumDisplaySettingsA(NULL, ENUM_CURRENT_SETTINGS, &mode) ||
      !timespec_get(&start, TIME_UTC)) {
    Fail("no clock");
  }
  for (int i = 0; i < kSwaps; i++) {
    SwapBuffers(hdc);
  }
  timespec_get(&end, TIME_UTC);
  double seconds = (double)(end.tv_sec - start.tv_sec) +
                   (double)(end.tv_nsec - start.tv_nsec) * 1e-9;
  double expected = (double)kSwaps / (double)mode.dmDisplayFrequency;
  if (seconds < 0.8 * expected) {
    fprintf(stderr, "%d swaps took %.3f s, expected %.3f s\n", kSwaps, seconds,
            expected);
    return EXIT_FAILURE;
  }
  swap_interval(0);

  glClearColor(1.0f, 0.5f, 0.25f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  if (!SwapBuffers(hdc)) {
    Fail("SwapBuffers failed");
  }
  GdiFlush();

  COLORREF pixel = GetPixel(hdc, 0, 0);
  if (pixel != RGB(255, 128, 64)) {
    fprintf(stderr, "pixel is %06lx, expected %06lx\n", pixel,
            RGB(255, 128, 64));
    return EXIT_FAILURE;
  }

  wglMakeCurrent(NULL, NULL);
  wglDeleteContext(context);
  ReleaseDC(window, hdc);
  DestroyWindow(window);
  puts("ok");
  return EXIT_SUCCESS;
}
