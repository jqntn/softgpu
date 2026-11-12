#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "icd.h"

#define SOFTGPU_FIELD(ret, name, params) ret(APIENTRY* name) params;
typedef struct Gl {
  SOFTGPU_GL11(SOFTGPU_FIELD)
} Gl;
#undef SOFTGPU_FIELD

typedef const char*(WINAPI* ExtensionsArbFn)(HDC);
typedef const char*(WINAPI* ExtensionsExtFn)(void);
typedef BOOL(WINAPI* SwapIntervalFn)(int);
typedef int(WINAPI* GetSwapIntervalFn)(void);

_Static_assert(sizeof(Gl) == kGlEntryCount * sizeof(PROC),
               "one function pointer per dispatch entry");

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
static const GLfloat kSliver[] = {-0.9f, -1.9f, 1.1f, -1.9f, -0.9f, 2.1f};
static const GLubyte kTexels[] = {255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 0};
static const COLORREF kBlack = RGB(0, 0, 0);
static const COLORREF kWhite = RGB(255, 255, 255);

static Gl gl;

static void Fail(const char* message) {
  fprintf(stderr, "%s\n", message);
  exit(EXIT_FAILURE);
}

static void Expect(bool condition, const char* message) {
  if (!condition) {
    Fail(message);
  }
}

static void ExpectError(GLenum expected, const char* message) {
  GLenum error = gl.GetError();
  if (error != expected) {
    fprintf(stderr, "%s: error %04x, expected %04x\n", message, error,
            expected);
    exit(EXIT_FAILURE);
  }
}

static void ExpectPixel(HDC hdc, int x, int y, COLORREF expected) {
  COLORREF pixel = GetPixel(hdc, x, y);
  if (pixel != expected) {
    fprintf(stderr, "pixel (%d, %d) is %06lx, expected %06lx\n", x, y, pixel,
            expected);
    exit(EXIT_FAILURE);
  }
}

static COLORREF ReadColor(int x, int y) {
  GLubyte rgba[4];
  gl.ReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
  return RGB(rgba[0], rgba[1], rgba[2]);
}

static void ExpectColor(int x, int y, COLORREF expected) {
  COLORREF pixel = ReadColor(x, y);
  if (pixel != expected) {
    fprintf(stderr, "color (%d, %d) is %06lx, expected %06lx\n", x, y, pixel,
            expected);
    exit(EXIT_FAILURE);
  }
}

static void ExpectNear(int x, int y, int red, int tolerance) {
  int value = GetRValue(ReadColor(x, y));
  if (abs(value - red) > tolerance) {
    fprintf(stderr, "red at (%d, %d) is %d, expected %d\n", x, y, value, red);
    exit(EXIT_FAILURE);
  }
}

static GLint Integer(GLenum pname) {
  GLint value;
  gl.GetIntegerv(pname, &value);
  return value;
}

static void Triangle(GLfloat red, GLfloat green, GLfloat blue,
                     const GLfloat vertices[6]) {
  gl.Begin(GL_TRIANGLES);
  gl.Color3f(red, green, blue);
  for (int i = 0; i < 6; i += 2) {
    gl.Vertex2f(vertices[i], vertices[i + 1]);
  }
  gl.End();
}

static void Quad(GLfloat x0, GLfloat y0, GLfloat x1, GLfloat y1, GLfloat z) {
  static const GLfloat kCorners[] = {0, 0, 1, 0, 1, 1, 0, 1};
  gl.Begin(GL_QUADS);
  for (int i = 0; i < 8; i += 2) {
    gl.TexCoord2f(kCorners[i] - 1.0f, kCorners[i + 1] - 1.0f);
    gl.Vertex3f(kCorners[i] ? x1 : x0, kCorners[i + 1] ? y1 : y0, z);
  }
  gl.End();
}

static void PixelSpace(int width, int height) {
  gl.MatrixMode(GL_PROJECTION);
  gl.LoadIdentity();
  gl.Ortho(0, width, 0, height, -1, 1);
  gl.MatrixMode(GL_MODELVIEW);
  gl.LoadIdentity();
  gl.ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  gl.Clear(GL_COLOR_BUFFER_BIT);
  gl.Color3f(1.0f, 1.0f, 1.0f);
}

static PROC Proc(const char* name) {
  PROC proc = DrvGetProcAddress(name);
  if (!proc) {
    Fail(name);
  }
  return proc;
}

static void TestSwapInterval(HDC hdc) {
  const char* arb = ((ExtensionsArbFn)Proc("wglGetExtensionsStringARB"))(hdc);
  const char* ext = ((ExtensionsExtFn)Proc("wglGetExtensionsStringEXT"))();
  const char* extensions = (const char*)gl.GetString(GL_EXTENSIONS);
  if (!arb || !strstr(arb, "WGL_EXT_swap_control") || strcmp(arb, ext) != 0 ||
      !strstr(extensions, "WGL_EXT_swap_control")) {
    Fail("no WGL_EXT_swap_control");
  }
  SwapIntervalFn set = (SwapIntervalFn)Proc("wglSwapIntervalEXT");
  GetSwapIntervalFn get = (GetSwapIntervalFn)Proc("wglGetSwapIntervalEXT");
  if (get() != 1) {
    Fail("swap interval does not default to 1");
  }
  if (set(-1) || GetLastError() != ERROR_INVALID_DATA || get() != 1) {
    Fail("negative swap interval accepted");
  }
  if (!set(2) || get() != 2 || !set(0) || get() != 0) {
    Fail("swap interval not stored");
  }
}

static void TestPipeline(int width, int height) {
  int left = width / 4;
  int right = width * 3 / 4;
  int bottom = height / 4;
  int top = height * 3 / 4;
  gl.Disable(GL_CULL_FACE);
  gl.ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  gl.Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  gl.Enable(GL_DEPTH_TEST);
  gl.Color4f(0.0f, 1.0f, 0.0f, 1.0f);
  Quad(-1.0f, -1.0f, 0.0f, 1.0f, -0.5f);
  gl.Color4f(1.0f, 0.0f, 0.0f, 1.0f);
  Quad(-1.0f, -1.0f, 1.0f, 1.0f, 0.5f);
  ExpectColor(left, height / 2, RGB(0, 255, 0));
  ExpectColor(right, height / 2, RGB(255, 0, 0));
  gl.Disable(GL_DEPTH_TEST);

  gl.PixelStorei(GL_UNPACK_ALIGNMENT, 1);
  gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 2, 2, 0, GL_RGB, GL_UNSIGNED_BYTE,
                kTexels);
  gl.PixelStorei(GL_UNPACK_ALIGNMENT, 4);
  gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  gl.TexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
  gl.Enable(GL_TEXTURE_2D);
  Quad(-1.0f, -1.0f, 1.0f, 1.0f, 0.0f);
  gl.Disable(GL_TEXTURE_2D);
  ExpectColor(left, bottom, RGB(255, 0, 0));
  ExpectColor(right, bottom, RGB(0, 255, 0));
  ExpectColor(left, top, RGB(0, 0, 255));
  ExpectColor(right, top, RGB(255, 255, 0));

  gl.Enable(GL_BLEND);
  gl.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  gl.Color4f(1.0f, 1.0f, 1.0f, 0.5f);
  Quad(-1.0f, -1.0f, 0.0f, 0.0f, 0.0f);
  gl.Disable(GL_BLEND);
  ExpectColor(left, bottom, RGB(255, 128, 128));

  gl.Clear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  gl.Enable(GL_DEPTH_TEST);
  gl.Enable(GL_STENCIL_TEST);
  gl.StencilFunc(GL_ALWAYS, 1, ~0u);
  gl.StencilOp(GL_KEEP, GL_INCR, GL_REPLACE);
  Quad(-1.0f, -1.0f, 0.0f, 1.0f, 0.0f);
  gl.Color4f(0.0f, 1.0f, 0.0f, 1.0f);
  Quad(-1.0f, -1.0f, 1.0f, 1.0f, 0.5f);
  gl.Disable(GL_DEPTH_TEST);
  gl.StencilFunc(GL_EQUAL, 2, ~0u);
  gl.StencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
  gl.Color4f(1.0f, 0.0f, 0.0f, 1.0f);
  Quad(-1.0f, -1.0f, 1.0f, 1.0f, 0.0f);
  gl.Disable(GL_STENCIL_TEST);
  ExpectColor(left, height / 2, RGB(255, 0, 0));
  ExpectColor(right, height / 2, RGB(0, 255, 0));

  gl.Clear(GL_DEPTH_BUFFER_BIT);
  gl.Enable(GL_DEPTH_TEST);
  gl.Color4f(0.0f, 1.0f, 0.0f, 1.0f);
  Quad(-1.0f, -1.0f, 1.0f, 1.0f, 0.998f);
  gl.Color4f(1.0f, 0.0f, 0.0f, 1.0f);
  gl.Begin(GL_TRIANGLES);
  for (int i = 0; i < 6; i += 2) {
    gl.Vertex3f(
        (2.0f * (float)(width / 2) + 1.0f + 2.0f * kSliver[i]) / (float)width -
            1.0f,
        (2.0f * (float)(height / 2) + 1.0f + 2.0f * kSliver[i + 1]) /
                (float)height -
            1.0f,
        0.9984f);
  }
  gl.End();
  gl.Disable(GL_DEPTH_TEST);
  ExpectColor(width / 2, height / 2, RGB(0, 255, 0));

  gl.MatrixMode(GL_PROJECTION);
  gl.Frustum(-1.0, 1.0, -1.0, 1.0, 1.0, 10.0);
  gl.Color4f(1.0f, 0.0f, 1.0f, 1.0f);
  Quad(-10.0f, -10.0f, 10.0f, 10.0f, -2.0f);
  gl.Color4f(0.0f, 1.0f, 1.0f, 1.0f);
  Quad(-10.0f, -10.0f, 10.0f, 10.0f, 2.0f);
  ExpectColor(width / 2, height / 2, RGB(255, 0, 255));
  gl.LoadIdentity();
  gl.MatrixMode(GL_MODELVIEW);
}

static void TestErrors(void) {
  ExpectError(GL_NO_ERROR, "initial error");
  gl.Enable(0xFFFF);
  ExpectError(GL_INVALID_ENUM, "bad capability");
  ExpectError(GL_NO_ERROR, "error not cleared");
  gl.Viewport(0, 0, -1, 1);
  ExpectError(GL_INVALID_VALUE, "negative viewport");
  gl.PopMatrix();
  ExpectError(GL_STACK_UNDERFLOW, "empty matrix stack");
  gl.Begin(GL_POINTS);
  gl.Enable(GL_BLEND);
  gl.End();
  ExpectError(GL_INVALID_OPERATION, "enable inside begin");
  gl.Begin(GL_POINTS);
  gl.LightModelf(GL_LIGHT_MODEL_AMBIENT, 0.0f);
  gl.VertexPointer(1, GL_FLOAT, 0, NULL);
  gl.End();
  ExpectError(GL_INVALID_OPERATION, "begin checked before arguments");
  gl.LightModelf(GL_LIGHT_MODEL_AMBIENT, 0.0f);
  ExpectError(GL_INVALID_ENUM, "scalar form of a vector parameter");
  gl.End();
  ExpectError(GL_INVALID_OPERATION, "end without begin");
  gl.BlendFunc(GL_SRC_COLOR, GL_ZERO);
  ExpectError(GL_INVALID_ENUM, "source color as a source factor");
  gl.DrawBuffer(GL_FRONT_RIGHT);
  ExpectError(GL_INVALID_OPERATION, "missing draw buffer");
}

static void TestQueries(void) {
  Expect(Integer(GL_MAX_LIGHTS) == 8, "eight lights");
  Expect(Integer(GL_MAX_LIST_NESTING) >= 64, "list nesting");
  gl.Color4f(1.0f, 0.0f, 0.5f, 1.0f);
  GLfloat color[4];
  gl.GetFloatv(GL_CURRENT_COLOR, color);
  Expect(color[2] == 0.5f, "current color as float");
  GLint integers[4];
  gl.GetIntegerv(GL_CURRENT_COLOR, integers);
  Expect(integers[0] == 2147483647 && integers[1] == 0,
         "current color as integer");
  Expect(gl.IsEnabled(GL_DITHER) == GL_TRUE, "dither on by default");
  Expect(strcmp((const char*)gl.GetString(GL_VERSION), "1.1") == 0, "1.1");
  GLboolean mask[4];
  gl.GetBooleanv(GL_COLOR_WRITEMASK, mask);
  Expect(mask[0] && mask[3], "color writemask");
  ExpectError(GL_NO_ERROR, "queries");
}

static void TestAttribStacks(void) {
  gl.Enable(GL_DEPTH_TEST);
  gl.DepthFunc(GL_LEQUAL);
  gl.PushAttrib(GL_ENABLE_BIT | GL_DEPTH_BUFFER_BIT);
  gl.Disable(GL_DEPTH_TEST);
  gl.DepthFunc(GL_GREATER);
  gl.PopAttrib();
  Expect(gl.IsEnabled(GL_DEPTH_TEST) && Integer(GL_DEPTH_FUNC) == GL_LEQUAL,
         "attributes restored");
  gl.Disable(GL_DEPTH_TEST);
  gl.DepthFunc(GL_LESS);
  GLint alignment = Integer(GL_UNPACK_ALIGNMENT);
  gl.PushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
  gl.PixelStorei(GL_UNPACK_ALIGNMENT, alignment == 1 ? 8 : 1);
  gl.PopClientAttrib();
  Expect(Integer(GL_UNPACK_ALIGNMENT) == alignment, "client attributes");
  gl.PopAttrib();
  ExpectError(GL_STACK_UNDERFLOW, "empty attribute stack");
}

static void TestDisplayLists(int width, int height) {
  PixelSpace(width, height);
  GLuint list = gl.GenLists(2);
  Expect(list != 0 && gl.IsList(list) && gl.IsList(list + 1), "gen lists");
  gl.NewList(list, GL_COMPILE);
  gl.Color3f(0.0f, 1.0f, 0.0f);
  gl.Rectf(0.0f, 0.0f, 10.0f, 10.0f);
  gl.EndList();
  ExpectColor(5, 5, kBlack);
  gl.Color3f(1.0f, 0.0f, 0.0f);
  gl.CallList(list);
  ExpectColor(5, 5, RGB(0, 255, 0));
  gl.NewList(list + 1, GL_COMPILE_AND_EXECUTE);
  gl.Color3f(0.0f, 0.0f, 1.0f);
  gl.Rectf(20.0f, 0.0f, 30.0f, 10.0f);
  gl.EndList();
  ExpectColor(25, 5, RGB(0, 0, 255));
  gl.Clear(GL_COLOR_BUFFER_BIT);
  static const GLubyte kOffsets[] = {0, 1};
  gl.ListBase(list);
  gl.CallLists(2, GL_UNSIGNED_BYTE, kOffsets);
  gl.ListBase(0);
  ExpectColor(5, 5, RGB(0, 255, 0));
  ExpectColor(25, 5, RGB(0, 0, 255));
  static const GLfloat kSquare[] = {0, 0, 10, 0, 10, 10, 0, 10};
  gl.VertexPointer(2, GL_FLOAT, 0, kSquare);
  gl.EnableClientState(GL_VERTEX_ARRAY);
  gl.NewList(list, GL_COMPILE);
  gl.DrawArrays(GL_QUADS, 0, 4);
  gl.EndList();
  gl.DisableClientState(GL_VERTEX_ARRAY);
  gl.Clear(GL_COLOR_BUFFER_BIT);
  gl.Begin(GL_QUADS);
  gl.CallList(list);
  gl.End();
  ExpectError(GL_INVALID_OPERATION, "listed arrays inside begin");
  ExpectColor(5, 5, kBlack);
  gl.Color3f(1.0f, 1.0f, 1.0f);
  gl.CallList(list);
  ExpectColor(5, 5, kWhite);
  gl.DeleteLists(list, 2);
  Expect(!gl.IsList(list) && !gl.IsList(list + 1), "deleted lists");
  ExpectError(GL_NO_ERROR, "display lists");
}

static void TestVertexArrays(int width, int height) {
  static const GLfloat kVertices[] = {40, 0, 50, 0, 50, 10, 40, 10};
  static const GLubyte kColors[] = {255, 255, 0, 255, 255, 255, 0, 255,
                                    255, 255, 0, 255, 255, 255, 0, 255};
  static const GLushort kIndices[] = {0, 1, 2};
  PixelSpace(width, height);
  gl.VertexPointer(2, GL_FLOAT, 0, kVertices);
  gl.ColorPointer(4, GL_UNSIGNED_BYTE, 0, kColors);
  gl.EnableClientState(GL_VERTEX_ARRAY);
  gl.Enable(GL_COLOR_ARRAY);
  gl.DrawArrays(GL_QUADS, 0, 4);
  ExpectColor(45, 5, RGB(255, 255, 0));
  gl.Clear(GL_COLOR_BUFFER_BIT);
  gl.DrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, kIndices);
  ExpectColor(48, 2, RGB(255, 255, 0));
  ExpectColor(42, 8, kBlack);
  gl.DisableClientState(GL_VERTEX_ARRAY);
  gl.Disable(GL_COLOR_ARRAY);
  Expect(!gl.IsEnabled(GL_COLOR_ARRAY), "array enable through glDisable");
  gl.ShadeModel(GL_FLAT);
  gl.Begin(GL_QUAD_STRIP);
  gl.Color3f(1.0f, 0.0f, 0.0f);
  gl.Vertex2f(60.0f, 0.0f);
  gl.Vertex2f(70.0f, 0.0f);
  gl.Vertex2f(60.0f, 10.0f);
  gl.Color3f(0.0f, 0.0f, 1.0f);
  gl.Vertex2f(70.0f, 10.0f);
  gl.End();
  gl.ShadeModel(GL_SMOOTH);
  ExpectColor(65, 5, RGB(0, 0, 255));
  ExpectError(GL_NO_ERROR, "vertex arrays");
}

static void TestLighting(int width, int height) {
  PixelSpace(width, height);
  gl.Enable(GL_LIGHTING);
  gl.Enable(GL_LIGHT0);
  gl.Normal3f(0.0f, 0.0f, 1.0f);
  gl.Rectf(60.0f, 0.0f, 70.0f, 10.0f);
  ExpectColor(65, 5, RGB(214, 214, 214));
  gl.Enable(GL_COLOR_MATERIAL);
  gl.Color3f(1.0f, 0.0f, 0.0f);
  gl.Rectf(60.0f, 0.0f, 70.0f, 10.0f);
  ExpectColor(65, 5, RGB(255, 0, 0));
  static const GLfloat kGreen[] = {0.0f, 1.0f, 0.0f, 1.0f};
  gl.Materialfv(GL_FRONT, GL_DIFFUSE, kGreen);
  GLfloat diffuse[4];
  gl.GetMaterialfv(GL_FRONT, GL_DIFFUSE, diffuse);
  Expect(diffuse[0] == 1.0f && diffuse[1] == 0.0f, "tracked material");
  gl.Disable(GL_COLOR_MATERIAL);
  gl.Disable(GL_LIGHTING);
  gl.Color3f(1.0f, 1.0f, 1.0f);
  ExpectError(GL_NO_ERROR, "lighting");
}

static void TestTexgen(int width, int height) {
  static const GLubyte kRedGreen[] = {255, 0, 0, 0, 255, 0};
  static const GLfloat kPlane[] = {1.0f / 20.0f, 0.0f, 0.0f, -4.0f};
  PixelSpace(width, height);
  gl.PixelStorei(GL_UNPACK_ALIGNMENT, 1);
  gl.TexImage1D(GL_TEXTURE_1D, 0, GL_RGB, 2, 0, GL_RGB, GL_UNSIGNED_BYTE,
                kRedGreen);
  gl.PixelStorei(GL_UNPACK_ALIGNMENT, 4);
  gl.TexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  gl.TexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  gl.TexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
  gl.TexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
  gl.TexGenfv(GL_S, GL_OBJECT_PLANE, kPlane);
  gl.Enable(GL_TEXTURE_GEN_S);
  gl.Enable(GL_TEXTURE_1D);
  gl.Rectf(80.0f, 0.0f, 100.0f, 10.0f);
  gl.Disable(GL_TEXTURE_1D);
  gl.Disable(GL_TEXTURE_GEN_S);
  ExpectColor(85, 5, RGB(255, 0, 0));
  ExpectColor(95, 5, RGB(0, 255, 0));
  ExpectError(GL_NO_ERROR, "texgen");
}

static void TestLinesAndPoints(int width, int height) {
  PixelSpace(width, height);
  gl.Begin(GL_LINES);
  gl.Vertex2f(10.0f, 30.5f);
  gl.Vertex2f(20.0f, 30.5f);
  gl.End();
  ExpectColor(10, 30, kWhite);
  ExpectColor(19, 30, kWhite);
  ExpectColor(20, 30, kBlack);
  ExpectColor(15, 31, kBlack);
  gl.PointSize(3.0f);
  gl.Begin(GL_POINTS);
  gl.Vertex2f(30.5f, 30.5f);
  gl.End();
  gl.PointSize(1.0f);
  ExpectColor(29, 29, kWhite);
  ExpectColor(31, 31, kWhite);
  ExpectColor(32, 30, kBlack);
  gl.PolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  gl.Rectf(40.0f, 20.0f, 60.0f, 40.0f);
  gl.PolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  ExpectColor(50, 20, kWhite);
  ExpectColor(50, 30, kBlack);
  gl.Enable(GL_LINE_STIPPLE);
  gl.LineStipple(1, 0x5555);
  gl.Begin(GL_LINES);
  gl.Vertex2f(10.0f, 50.5f);
  gl.Vertex2f(20.0f, 50.5f);
  gl.End();
  gl.Disable(GL_LINE_STIPPLE);
  ExpectColor(10, 50, kWhite);
  ExpectColor(11, 50, kBlack);
  ExpectError(GL_NO_ERROR, "lines and points");
}

static void TestRasterRules(int width, int height) {
  PixelSpace(width, height);
  gl.Enable(GL_BLEND);
  gl.BlendFunc(GL_ONE, GL_ONE);
  gl.Color3f(0.25f, 0.0f, 0.0f);
  gl.Begin(GL_TRIANGLES);
  gl.Vertex2f(0.0f, 0.0f);
  gl.Vertex2f(16.0f, 0.0f);
  gl.Vertex2f(16.0f, 16.0f);
  gl.Vertex2f(0.0f, 0.0f);
  gl.Vertex2f(16.0f, 16.0f);
  gl.Vertex2f(0.0f, 16.0f);
  gl.End();
  gl.Disable(GL_BLEND);
  ExpectColor(5, 5, RGB(64, 0, 0));
  ExpectColor(10, 3, RGB(64, 0, 0));
  ExpectColor(3, 10, RGB(64, 0, 0));

  gl.Color3f(1.0f, 1.0f, 1.0f);
  gl.Begin(GL_LINES);
  gl.Vertex2f(20.52f, 40.5f);
  gl.Vertex2f(30.0f, 40.5f);
  gl.End();
  ExpectColor(20, 40, kWhite);

  gl.Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  gl.Enable(GL_DEPTH_TEST);
  gl.DepthFunc(GL_LEQUAL);
  gl.Translatef(0.375f, 0.375f, 0.0f);
  gl.PolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  gl.Rectf(1.0f, 1.0f, 158.0f, 158.0f);
  gl.PolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  gl.Enable(GL_POLYGON_OFFSET_FILL);
  gl.PolygonOffset(1.0f, 1.0f);
  gl.Color3f(0.0f, 0.0f, 1.0f);
  gl.Rectf(0.0f, 0.0f, 160.0f, 160.0f);
  gl.Disable(GL_POLYGON_OFFSET_FILL);
  gl.Disable(GL_DEPTH_TEST);
  gl.DepthFunc(GL_LESS);
  gl.LoadIdentity();
  gl.Color3f(1.0f, 1.0f, 1.0f);
  for (int y = 2; y < 158; y++) {
    ExpectColor(1, y, kWhite);
  }
  ExpectColor(80, 80, RGB(0, 0, 255));
  ExpectError(GL_NO_ERROR, "raster rules");
}

static void TestFeedbackAndSelection(void) {
  GLfloat feedback[16];
  gl.FeedbackBuffer(16, GL_3D, feedback);
  gl.RenderMode(GL_FEEDBACK);
  gl.Begin(GL_POINTS);
  gl.Vertex2f(10.5f, 20.5f);
  gl.End();
  gl.PassThrough(7.0f);
  Expect(gl.RenderMode(GL_RENDER) == 6, "feedback count");
  Expect(feedback[0] == GL_POINT_TOKEN && feedback[1] == 10.5f &&
             feedback[2] == 20.5f && feedback[3] == 0.5f,
         "point feedback");
  Expect(feedback[4] == GL_PASS_THROUGH_TOKEN && feedback[5] == 7.0f,
         "pass-through feedback");
  GLuint selection[16];
  gl.SelectBuffer(16, selection);
  gl.RenderMode(GL_SELECT);
  gl.InitNames();
  gl.PushName(7);
  gl.Rectf(0.0f, 0.0f, 10.0f, 10.0f);
  gl.LoadName(8);
  gl.Rectf(-100.0f, -100.0f, -90.0f, -90.0f);
  Expect(gl.RenderMode(GL_RENDER) == 1, "one hit");
  Expect(selection[0] == 1 && selection[3] == 7, "hit record");
  ExpectError(GL_NO_ERROR, "feedback and selection");
}

static void TestPixels(int width, int height) {
  static const GLubyte kImage[] = {255, 0, 0,   255, 0,   255, 0,   255,
                                   0,   0, 255, 255, 255, 255, 255, 255};
  PixelSpace(width, height);
  gl.RasterPos2i(100, 100);
  gl.DrawPixels(2, 2, GL_RGBA, GL_UNSIGNED_BYTE, kImage);
  ExpectColor(100, 100, RGB(255, 0, 0));
  ExpectColor(101, 100, RGB(0, 255, 0));
  ExpectColor(100, 101, RGB(0, 0, 255));
  ExpectColor(101, 101, kWhite);
  GLubyte luminance[2];
  gl.PixelStorei(GL_PACK_ALIGNMENT, 1);
  gl.ReadPixels(100, 101, 2, 1, GL_LUMINANCE, GL_UNSIGNED_BYTE, luminance);
  gl.PixelStorei(GL_PACK_ALIGNMENT, 4);
  Expect(luminance[0] == 255 && luminance[1] == 255, "luminance read");
  GLfloat rgba[4];
  gl.ReadPixels(101, 100, 1, 1, GL_RGBA, GL_FLOAT, rgba);
  Expect(rgba[0] == 0.0f && rgba[1] == 1.0f, "float read");
  gl.PixelZoom(2.0f, 2.0f);
  gl.RasterPos2i(110, 100);
  gl.DrawPixels(2, 2, GL_RGBA, GL_UNSIGNED_BYTE, kImage);
  gl.PixelZoom(1.0f, 1.0f);
  ExpectColor(111, 101, RGB(255, 0, 0));
  ExpectColor(112, 100, RGB(0, 255, 0));
  gl.RasterPos2i(120, 100);
  gl.CopyPixels(100, 100, 2, 2, GL_COLOR);
  ExpectColor(120, 100, RGB(255, 0, 0));
  ExpectColor(121, 101, kWhite);
  gl.PixelTransferf(GL_RED_SCALE, 0.0f);
  gl.RasterPos2i(130, 100);
  gl.DrawPixels(2, 2, GL_RGBA, GL_UNSIGNED_BYTE, kImage);
  gl.PixelTransferf(GL_RED_SCALE, 1.0f);
  ExpectColor(130, 100, kBlack);
  static const GLubyte kBits[] = {0xA0};
  gl.RasterPos2i(140, 100);
  gl.Bitmap(4, 1, 0.0f, 0.0f, 4.0f, 0.0f, kBits);
  ExpectColor(140, 100, kWhite);
  ExpectColor(141, 100, kBlack);
  ExpectColor(142, 100, kWhite);
  GLfloat position[4];
  gl.GetFloatv(GL_CURRENT_RASTER_POSITION, position);
  Expect(position[0] == 144.0f, "bitmap moves the raster position");
  ExpectError(GL_NO_ERROR, "pixels");
}

static void TestAccum(int width, int height) {
  PixelSpace(width, height);
  gl.ClearColor(0.5f, 0.25f, 0.0f, 1.0f);
  gl.Clear(GL_COLOR_BUFFER_BIT | GL_ACCUM_BUFFER_BIT);
  gl.Accum(GL_LOAD, 1.0f);
  gl.ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  gl.Clear(GL_COLOR_BUFFER_BIT);
  gl.Accum(GL_RETURN, 1.0f);
  ExpectColor(5, 5, RGB(128, 64, 0));
  ExpectError(GL_NO_ERROR, "accumulation");
}

static void TestEvaluators(int width, int height) {
  static const GLfloat kPoints[] = {150.0f, 100.5f, 0.0f, 160.0f, 100.5f, 0.0f};
  PixelSpace(width, height);
  gl.Map1f(GL_MAP1_VERTEX_3, 0.0f, 1.0f, 3, 2, kPoints);
  gl.Enable(GL_MAP1_VERTEX_3);
  gl.MapGrid1f(10, 0.0f, 1.0f);
  gl.EvalMesh1(GL_LINE, 0, 10);
  gl.Disable(GL_MAP1_VERTEX_3);
  ExpectColor(155, 100, kWhite);
  ExpectColor(155, 101, kBlack);
  GLint order;
  gl.GetMapiv(GL_MAP1_VERTEX_3, GL_ORDER, &order);
  Expect(order == 2, "map order");
  ExpectError(GL_NO_ERROR, "evaluators");
}

static void TestFragmentOps(int width, int height) {
  PixelSpace(width, height);
  gl.ColorMask(GL_TRUE, GL_FALSE, GL_TRUE, GL_TRUE);
  gl.Rectf(0.0f, 0.0f, 10.0f, 10.0f);
  gl.ColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  ExpectColor(5, 5, RGB(255, 0, 255));
  gl.Enable(GL_COLOR_LOGIC_OP);
  gl.LogicOp(GL_INVERT);
  gl.Rectf(0.0f, 0.0f, 10.0f, 10.0f);
  gl.Disable(GL_COLOR_LOGIC_OP);
  ExpectColor(5, 5, RGB(0, 255, 0));
  gl.Enable(GL_BLEND);
  gl.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  gl.Enable(GL_POLYGON_SMOOTH);
  gl.Clear(GL_COLOR_BUFFER_BIT);
  gl.Rectf(0.5f, 0.0f, 10.5f, 10.0f);
  gl.Disable(GL_POLYGON_SMOOTH);
  gl.Disable(GL_BLEND);
  ExpectNear(0, 5, 128, 16);
  ExpectColor(5, 5, kWhite);
  static const GLdouble kPlane[] = {-1.0, 0.0, 0.0, 5.0};
  gl.Clear(GL_COLOR_BUFFER_BIT);
  gl.ClipPlane(GL_CLIP_PLANE0, kPlane);
  gl.Enable(GL_CLIP_PLANE0);
  gl.Rectf(0.0f, 0.0f, 10.0f, 10.0f);
  gl.Disable(GL_CLIP_PLANE0);
  ExpectColor(2, 5, kWhite);
  ExpectColor(8, 5, kBlack);
  static const GLfloat kBlue[] = {0.0f, 0.0f, 1.0f, 1.0f};
  gl.Enable(GL_FOG);
  gl.Fogi(GL_FOG_MODE, GL_LINEAR);
  gl.Fogf(GL_FOG_START, 0.0f);
  gl.Fogf(GL_FOG_END, 1.0f);
  gl.Fogfv(GL_FOG_COLOR, kBlue);
  gl.Begin(GL_QUADS);
  gl.Vertex3f(20.0f, 0.0f, 0.5f);
  gl.Vertex3f(30.0f, 0.0f, 0.5f);
  gl.Vertex3f(30.0f, 10.0f, 0.5f);
  gl.Vertex3f(20.0f, 10.0f, 0.5f);
  gl.End();
  gl.Begin(GL_QUADS);
  gl.Vertex3f(40.0f, 0.0f, -0.9f);
  gl.Vertex3f(60.0f, 0.0f, 0.9f);
  gl.Vertex3f(60.0f, 10.0f, 0.9f);
  gl.Vertex3f(40.0f, 10.0f, -0.9f);
  gl.End();
  gl.Disable(GL_FOG);
  ExpectNear(25, 5, 128, 1);
  ExpectNear(50, 5, 255, 16);
  gl.Disable(GL_DITHER);
  gl.ClearColor(0.5f, 0.0f, 0.0f, 1.0f);
  gl.Clear(GL_COLOR_BUFFER_BIT);
  gl.Enable(GL_DITHER);
  ExpectColor(5, 5, RGB(128, 0, 0));
  gl.ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  ExpectError(GL_NO_ERROR, "fragment operations");
}

static void TestBuffers(int width, int height) {
  PixelSpace(width, height);
  gl.DrawBuffer(GL_FRONT);
  gl.Rectf(0.0f, 0.0f, 10.0f, 10.0f);
  gl.DrawBuffer(GL_BACK);
  ExpectColor(5, 5, kBlack);
  gl.ReadBuffer(GL_FRONT);
  ExpectColor(5, 5, kWhite);
  gl.ReadBuffer(GL_BACK);
  ExpectError(GL_NO_ERROR, "front and back buffers");
}

static void TestWindows(HDC hdc, ULONG context) {
  gl.ClearColor(0.0f, 0.0f, 1.0f, 1.0f);
  gl.Clear(GL_COLOR_BUFFER_BIT);
  HWND window = CreateWindowA("softgpu", "softgpu", WS_OVERLAPPEDWINDOW, 0, 0,
                              64, 64, NULL, NULL, NULL, NULL);
  HDC other_hdc = GetDC(window);
  Expect(DrvSetPixelFormat(other_hdc, 1) &&
             DrvSetContext(other_hdc, context, NULL) != NULL,
         "second window");
  gl.ClearColor(1.0f, 0.0f, 0.0f, 1.0f);
  gl.Clear(GL_COLOR_BUFFER_BIT);
  ULONG other = DrvCreateContext(hdc);
  Expect(DrvSetContext(hdc, other, NULL) != NULL, "second context");
  ExpectColor(5, 5, RGB(0, 0, 255));
  DrvDeleteContext(other);
  Expect(DrvSetContext(other_hdc, context, NULL) != NULL, "back to window");
  ExpectColor(5, 5, RGB(255, 0, 0));
  Expect(DrvSetContext(hdc, context, NULL) != NULL, "first window");
  gl.ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  ReleaseDC(window, other_hdc);
  DestroyWindow(window);
  ExpectError(GL_NO_ERROR, "windows");
}

static void TestTextures(void) {
  static const GLubyte kBordered[] = {0, 0, 255, 0,   0, 255, 0, 0, 255,
                                      0, 0, 255, 255, 0, 0,   0, 0, 255,
                                      0, 0, 255, 0,   0, 255, 0, 0, 255};
  GLuint texture;
  gl.GenTextures(1, &texture);
  Expect(!gl.IsTexture(texture), "unbound name is not a texture");
  gl.BindTexture(GL_TEXTURE_2D, texture);
  Expect(gl.IsTexture(texture), "bound name is a texture");
  gl.PixelStorei(GL_UNPACK_ALIGNMENT, 1);
  gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 3, 3, 1, GL_RGB, GL_UNSIGNED_BYTE,
                kBordered);
  gl.PixelStorei(GL_UNPACK_ALIGNMENT, 4);
  GLint value;
  gl.GetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_BORDER, &value);
  Expect(value == 1, "texture border");
  GLubyte texels[9];
  gl.PixelStorei(GL_PACK_ALIGNMENT, 1);
  gl.GetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_UNSIGNED_BYTE, texels);
  gl.PixelStorei(GL_PACK_ALIGNMENT, 4);
  Expect(texels[0] == 0 && texels[4] == 255, "texture image with border");
  gl.TexImage2D(GL_PROXY_TEXTURE_2D, 0, GL_RGBA, 4096, 4096, 0, GL_RGBA,
                GL_UNSIGNED_BYTE, NULL);
  gl.GetTexLevelParameteriv(GL_PROXY_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &value);
  Expect(value == 0, "oversized proxy");
  gl.BindTexture(GL_TEXTURE_1D, texture);
  ExpectError(GL_INVALID_OPERATION, "2D texture bound to 1D");
  gl.DeleteTextures(1, &texture);
  Expect(Integer(GL_TEXTURE_BINDING_2D) == 0, "deleted texture unbound");
  ExpectError(GL_NO_ERROR, "textures");
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
  if (((SwapIntervalFn)Proc("wglSwapIntervalEXT"))(1) ||
      GetLastError() != ERROR_DC_NOT_FOUND) {
    Fail("swap interval set without a context");
  }

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
  memcpy(&gl, table->entries, sizeof(gl));
  TestSwapInterval(hdc);

  gl.ClearColor(1.0f, 0.5f, 0.25f, 1.0f);
  gl.Clear(GL_COLOR_BUFFER_BIT);
  Triangle(0.0f, 0.0f, 1.0f, kLowerLeftCw);
  gl.Enable(GL_CULL_FACE);
  Triangle(0.0f, 1.0f, 0.0f, kCenterCcw);
  Triangle(1.0f, 0.0f, 0.0f, kUpperRightCw);
  gl.CullFace(GL_FRONT);
  Triangle(1.0f, 0.0f, 0.0f, kUpperRightCcw);
  gl.FrontFace(GL_CW);
  Triangle(1.0f, 0.0f, 0.0f, kUpperRightCw);
  gl.CullFace(GL_FRONT_AND_BACK);
  Triangle(1.0f, 0.0f, 0.0f, kUpperRightCcw);
  gl.FrontFace(GL_CCW);
  gl.CullFace(GL_BACK);
  if (!DrvSwapBuffers(hdc)) {
    Fail("swap failed");
  }
  GdiFlush();

  RECT rect;
  GetClientRect(window, &rect);
  ExpectPixel(hdc, 2, rect.bottom - 3, RGB(0, 0, 255));
  ExpectPixel(hdc, rect.right - 3, 2, RGB(255, 128, 64));
  ExpectPixel(hdc, rect.right / 2 + 8, rect.bottom / 2 - 8, RGB(0, 255, 0));
  TestPipeline(rect.right, rect.bottom);
  TestErrors();
  TestQueries();
  TestAttribStacks();
  TestDisplayLists(rect.right, rect.bottom);
  TestVertexArrays(rect.right, rect.bottom);
  TestLighting(rect.right, rect.bottom);
  TestTexgen(rect.right, rect.bottom);
  TestLinesAndPoints(rect.right, rect.bottom);
  TestRasterRules(rect.right, rect.bottom);
  TestFeedbackAndSelection();
  TestPixels(rect.right, rect.bottom);
  TestAccum(rect.right, rect.bottom);
  TestEvaluators(rect.right, rect.bottom);
  TestFragmentOps(rect.right, rect.bottom);
  TestBuffers(rect.right, rect.bottom);
  TestWindows(hdc, context);
  TestTextures();

  DrvReleaseContext(context);
  DrvDeleteContext(context);
  ReleaseDC(window, hdc);
  DestroyWindow(window);
  puts("ok");
  return EXIT_SUCCESS;
}
