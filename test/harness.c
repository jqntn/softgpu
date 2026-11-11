#include <stdio.h>
#include <stdlib.h>

#include "icd.h"

typedef void(APIENTRY* ClearColorFn)(GLclampf, GLclampf, GLclampf, GLclampf);
typedef void(APIENTRY* ClearFn)(GLbitfield);
typedef GLboolean(APIENTRY* IsEnabledFn)(GLenum);
typedef void(APIENTRY* EnumFn)(GLenum);
typedef void(APIENTRY* Color3fFn)(GLfloat, GLfloat, GLfloat);
typedef void(APIENTRY* Vertex2fFn)(GLfloat, GLfloat);
typedef void(APIENTRY* Vertex3fFn)(GLfloat, GLfloat, GLfloat);
typedef void(APIENTRY* Color4fFn)(GLfloat, GLfloat, GLfloat, GLfloat);
typedef void(APIENTRY* EndFn)(void);
typedef void(APIENTRY* BlendFuncFn)(GLenum, GLenum);
typedef void(APIENTRY* ParameterFn)(GLenum, GLenum, GLint);
typedef void(APIENTRY* PixelStoreiFn)(GLenum, GLint);
typedef void(APIENTRY* FrustumFn)(GLdouble, GLdouble, GLdouble, GLdouble,
                                  GLdouble, GLdouble);
typedef void(APIENTRY* TexImage2DFn)(GLenum, GLint, GLint, GLsizei, GLsizei,
                                     GLint, GLenum, GLenum, const GLvoid*);
typedef void(APIENTRY* ReadPixelsFn)(GLint, GLint, GLsizei, GLsizei, GLenum,
                                     GLenum, GLvoid*);

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
static const GLubyte kTexels[] = {255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 0};

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

static void Quad(const ProcTable* table, GLfloat x0, GLfloat y0, GLfloat x1,
                 GLfloat y1, GLfloat z) {
  static const GLfloat kCorners[] = {0, 0, 1, 0, 1, 1, 0, 1};
  ((EnumFn)table->entries[kGlBegin])(GL_QUADS);
  for (int i = 0; i < 8; i += 2) {
    ((Vertex2fFn)table->entries[kGlTexCoord2f])(kCorners[i], kCorners[i + 1]);
    ((Vertex3fFn)table->entries[kGlVertex3f])(kCorners[i] ? x1 : x0,
                                              kCorners[i + 1] ? y1 : y0, z);
  }
  ((EndFn)table->entries[kGlEnd])();
}

static void ExpectColor(const ProcTable* table, int x, int y,
                        COLORREF expected) {
  GLubyte rgba[4];
  ((ReadPixelsFn)table->entries[kGlReadPixels])(x, y, 1, 1, GL_RGBA,
                                                GL_UNSIGNED_BYTE, rgba);
  COLORREF pixel = RGB(rgba[0], rgba[1], rgba[2]);
  if (pixel != expected) {
    fprintf(stderr, "color (%d, %d) is %06lx, expected %06lx\n", x, y, pixel,
            expected);
    exit(EXIT_FAILURE);
  }
}

static void TestPipeline(const ProcTable* table, int width, int height) {
  int left = width / 4;
  int right = width * 3 / 4;
  int bottom = height / 4;
  int top = height * 3 / 4;
  ((EnumFn)table->entries[kGlDisable])(GL_CULL_FACE);
  ((ClearColorFn)table->entries[kGlClearColor])(0.0f, 0.0f, 0.0f, 1.0f);
  ((ClearFn)table->entries[kGlClear])(GL_COLOR_BUFFER_BIT |
                                      GL_DEPTH_BUFFER_BIT);

  ((EnumFn)table->entries[kGlEnable])(GL_DEPTH_TEST);
  ((Color4fFn)table->entries[kGlColor4f])(0.0f, 1.0f, 0.0f, 1.0f);
  Quad(table, -1.0f, -1.0f, 0.0f, 1.0f, -0.5f);
  ((Color4fFn)table->entries[kGlColor4f])(1.0f, 0.0f, 0.0f, 1.0f);
  Quad(table, -1.0f, -1.0f, 1.0f, 1.0f, 0.5f);
  ExpectColor(table, left, height / 2, RGB(0, 255, 0));
  ExpectColor(table, right, height / 2, RGB(255, 0, 0));
  ((EnumFn)table->entries[kGlDisable])(GL_DEPTH_TEST);

  ((PixelStoreiFn)table->entries[kGlPixelStorei])(GL_UNPACK_ALIGNMENT, 1);
  ((TexImage2DFn)table->entries[kGlTexImage2D])(
      GL_TEXTURE_2D, 0, GL_RGB, 2, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, kTexels);
  ((ParameterFn)table->entries[kGlTexParameteri])(
      GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  ((ParameterFn)table->entries[kGlTexParameteri])(
      GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  ((ParameterFn)table->entries[kGlTexEnvi])(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE,
                                            GL_REPLACE);
  ((EnumFn)table->entries[kGlEnable])(GL_TEXTURE_2D);
  Quad(table, -1.0f, -1.0f, 1.0f, 1.0f, 0.0f);
  ((EnumFn)table->entries[kGlDisable])(GL_TEXTURE_2D);
  ExpectColor(table, left, bottom, RGB(255, 0, 0));
  ExpectColor(table, right, bottom, RGB(0, 255, 0));
  ExpectColor(table, left, top, RGB(0, 0, 255));
  ExpectColor(table, right, top, RGB(255, 255, 0));

  ((EnumFn)table->entries[kGlEnable])(GL_BLEND);
  ((BlendFuncFn)table->entries[kGlBlendFunc])(GL_SRC_ALPHA,
                                              GL_ONE_MINUS_SRC_ALPHA);
  ((Color4fFn)table->entries[kGlColor4f])(1.0f, 1.0f, 1.0f, 0.5f);
  Quad(table, -1.0f, -1.0f, 0.0f, 0.0f, 0.0f);
  ((EnumFn)table->entries[kGlDisable])(GL_BLEND);
  ExpectColor(table, left, bottom, RGB(255, 128, 128));

  ((EnumFn)table->entries[kGlMatrixMode])(GL_PROJECTION);
  ((FrustumFn)table->entries[kGlFrustum])(-1.0, 1.0, -1.0, 1.0, 1.0, 10.0);
  ((Color4fFn)table->entries[kGlColor4f])(1.0f, 0.0f, 1.0f, 1.0f);
  Quad(table, -10.0f, -10.0f, 10.0f, 10.0f, -2.0f);
  ((Color4fFn)table->entries[kGlColor4f])(0.0f, 1.0f, 1.0f, 1.0f);
  Quad(table, -10.0f, -10.0f, 10.0f, 10.0f, 2.0f);
  ExpectColor(table, width / 2, height / 2, RGB(255, 0, 255));
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
  TestPipeline(table, rect.right, rect.bottom);

  DrvReleaseContext(context);
  DrvDeleteContext(context);
  ReleaseDC(window, hdc);
  DestroyWindow(window);
  puts("ok");
  return EXIT_SUCCESS;
}
