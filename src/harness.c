#include <stdio.h>
#include <stdlib.h>

#include "icd.h"

typedef void(APIENTRY* ClearColorFn)(GLclampf, GLclampf, GLclampf, GLclampf);
typedef void(APIENTRY* ClearFn)(GLbitfield);
typedef GLboolean(APIENTRY* IsEnabledFn)(GLenum);

static const char* const kExports[] = {
    "DrvValidateVersion",
    "DrvSetCallbackProcs",
    "DrvDescribePixelFormat",
    "DrvSetPixelFormat",
    "DrvCreateContext",
    "DrvCreateLayerContext",
    "DrvDeleteContext",
    "DrvCopyContext",
    "DrvShareLists",
    "DrvSetContext",
    "DrvReleaseContext",
    "DrvGetProcAddress",
    "DrvSwapBuffers",
    "DrvSwapLayerBuffers",
    "DrvDescribeLayerPlane",
    "DrvSetLayerPaletteEntries",
    "DrvGetLayerPaletteEntries",
    "DrvRealizeLayerPalette",
};

static void Fail(const char* message) {
  fprintf(stderr, "%s\n", message);
  exit(EXIT_FAILURE);
}

int main(void) {
  HMODULE icd = GetModuleHandleA("softgpu.dll");
  for (size_t i = 0; i < sizeof(kExports) / sizeof(kExports[0]); i++) {
    if (!GetProcAddress(icd, kExports[i])) {
      Fail(kExports[i]);
    }
  }

  RegisterClassA(&(WNDCLASSA){
      .lpfnWndProc = DefWindowProcA,
      .hInstance = GetModuleHandleA(NULL),
      .lpszClassName = "softgpu",
  });
  HWND window = CreateWindowA("softgpu", "softgpu",
                              WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT,
                              CW_USEDEFAULT, 640, 480, NULL, NULL, NULL, NULL);
  if (!window) {
    Fail("CreateWindowA failed");
  }
  HDC hdc = GetDC(window);

  PIXELFORMATDESCRIPTOR descriptor;
  if (!DrvValidateVersion(1) ||
      DrvDescribePixelFormat(hdc, 1, sizeof(descriptor), &descriptor) < 1 ||
      !DrvSetPixelFormat(hdc, 1)) {
    Fail("no pixel format");
  }
  ULONG context = DrvCreateContext(hdc);
  ProcTable* table = context ? DrvSetContext(hdc, context, NULL) : NULL;
  if (!table || table->count != kGlEntryCount) {
    Fail("no context");
  }

  ((ClearColorFn)table->entries[kGlClearColor])(1.0f, 0.5f, 0.25f, 1.0f);
  ((ClearFn)table->entries[kGlClear])(GL_COLOR_BUFFER_BIT);
  if (((IsEnabledFn)table->entries[kGlIsEnabled])(GL_DEPTH_TEST) != GL_FALSE) {
    Fail("stub returned nonzero");
  }
  if (!DrvSwapBuffers(hdc)) {
    Fail("swap failed");
  }
  GdiFlush();

  COLORREF pixel = GetPixel(hdc, 0, 0);
  if (pixel != RGB(255, 128, 64)) {
    fprintf(stderr, "pixel is %06lx, expected %06lx\n", pixel,
            RGB(255, 128, 64));
    return EXIT_FAILURE;
  }

  DrvReleaseContext(context);
  DrvDeleteContext(context);
  ReleaseDC(window, hdc);
  DestroyWindow(window);
  puts("ok");
  return EXIT_SUCCESS;
}
