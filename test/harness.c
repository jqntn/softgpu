#include <stdio.h>
#include <stdlib.h>

#include "icd.h"

typedef void(APIENTRY* ClearColorFn)(GLclampf, GLclampf, GLclampf, GLclampf);
typedef void(APIENTRY* ClearFn)(GLbitfield);
typedef GLboolean(APIENTRY* IsEnabledFn)(GLenum);
typedef void(APIENTRY* EnumFn)(GLenum);
typedef void(APIENTRY* Color3fFn)(GLfloat, GLfloat, GLfloat);
typedef void(APIENTRY* Vertex2fFn)(GLfloat, GLfloat);
typedef void(APIENTRY* EndFn)(void);

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

static const GLfloat kLowerLeftCw[] = {-1.0f, -1.0f, -1.0f, 1.0f, 1.0f, -1.0f};
static const GLfloat kUpperRightCw[] = {-1.0f, 1.0f, 1.0f, 1.0f, 1.0f, -1.0f};
static const GLfloat kUpperRightCcw[] = {-1.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f};
static const GLfloat kCenterCcw[] = {0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f};

static void Fail(const char* message) {
  fprintf(stderr, "%s\n", message);
  exit(EXIT_FAILURE);
}

static void ExpectPixel(HDC hdc, int x, int y, COLORREF expected) {
  COLORREF pixel = GetPixel(hdc, x, y);
  if (pixel != expected) {
    fprintf(stderr, "pixel (%d, %d) is %06lx, expected %06lx\n", x, y, pixel,
            expected);
    exit(EXIT_FAILURE);
  }
}

static void Triangle(const ProcTable* table, GLfloat red, GLfloat green,
                     GLfloat blue, const GLfloat vertices[6]) {
  ((EnumFn)table->entries[kGlBegin])(GL_TRIANGLES);
  ((Color3fFn)table->entries[kGlColor3f])(red, green, blue);
  for (int i = 0; i < 6; i += 2) {
    ((Vertex2fFn)table->entries[kGlVertex2f])(vertices[i], vertices[i + 1]);
  }
  ((EndFn)table->entries[kGlEnd])();
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
  Triangle(table, 0.0f, 0.0f, 1.0f, kLowerLeftCw);
  ((EnumFn)table->entries[kGlEnable])(GL_CULL_FACE);
  Triangle(table, 0.0f, 1.0f, 0.0f, kCenterCcw);
  Triangle(table, 1.0f, 0.0f, 0.0f, kUpperRightCw);
  ((EnumFn)table->entries[kGlCullFace])(GL_FRONT);
  Triangle(table, 1.0f, 0.0f, 0.0f, kUpperRightCcw);
  ((EnumFn)table->entries[kGlFrontFace])(GL_CW);
  Triangle(table, 1.0f, 0.0f, 0.0f, kUpperRightCw);
  ((EnumFn)table->entries[kGlCullFace])(GL_FRONT_AND_BACK);
  Triangle(table, 1.0f, 0.0f, 0.0f, kUpperRightCcw);
  if (!DrvSwapBuffers(hdc)) {
    Fail("swap failed");
  }
  GdiFlush();

  RECT rect;
  GetClientRect(window, &rect);
  ExpectPixel(hdc, 2, rect.bottom - 3, RGB(0, 0, 255));
  ExpectPixel(hdc, rect.right - 3, 2, RGB(255, 128, 64));
  ExpectPixel(hdc, rect.right / 2 + 8, rect.bottom / 2 - 8, RGB(0, 255, 0));

  DrvReleaseContext(context);
  DrvDeleteContext(context);
  ReleaseDC(window, hdc);
  DestroyWindow(window);
  puts("ok");
  return EXIT_SUCCESS;
}
