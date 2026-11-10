#include "icd.h"

#include <math.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(kGlEntryCount == 336, "GL 1.1 has 336 dispatch entries");

enum {
  kMaxContexts = 64,
  kPixelFormatCount = 1,
  kSubpixels = 16,
  kGuardBand = 1 << 19
};

typedef struct Vertex {
  float x;
  float y;
  float color[4];
} Vertex;

typedef struct Context {
  int width;
  int height;
  uint32_t* pixels;
  uint32_t clear_color;
  bool attached;
  int viewport[4];
  float color[4];
  GLenum mode;
  bool drawing;
  int vertex_count;
  Vertex vertices[3];
} Context;

static Context contexts[kMaxContexts];
static atomic_int used[kMaxContexts];
static _Thread_local Context* current;

static void Fail(const char* message) {
  fprintf(stderr, "softgpu: %s\n", message);
  exit(EXIT_FAILURE);
}

static void* Allocate(size_t size) {
  void* memory = malloc(size);
  if (!memory) {
    Fail("out of memory");
  }
  return memory;
}

static uint32_t PackChannel(GLclampf value) {
  GLclampf clamped = value > 0.0f ? (value < 1.0f ? value : 1.0f) : 0.0f;
  return (uint32_t)(clamped * 255.0f + 0.5f);
}

static uint32_t PackColor(const float color[4]) {
  return PackChannel(color[3]) << 24 | PackChannel(color[0]) << 16 |
         PackChannel(color[1]) << 8 | PackChannel(color[2]);
}

static int ClampInt(int value, int low, int high) {
  return value < low ? low : (value > high ? high : value);
}

static void APIENTRY ClearColor(GLclampf red, GLclampf green, GLclampf blue,
                                GLclampf alpha) {
  current->clear_color = PackColor((float[4]){red, green, blue, alpha});
}

static void APIENTRY Clear(GLbitfield mask) {
  if (mask & GL_COLOR_BUFFER_BIT) {
    size_t count = (size_t)current->width * (size_t)current->height;
    for (size_t i = 0; i < count; i++) {
      current->pixels[i] = current->clear_color;
    }
  }
}

static const GLubyte* APIENTRY GetString(GLenum name) {
  switch (name) {
    case GL_VENDOR:
    case GL_RENDERER:
      return (const GLubyte*)"softgpu";
    case GL_VERSION:
      return (const GLubyte*)"1.1";
    case GL_EXTENSIONS:
      return (const GLubyte*)"";
    default:
      return NULL;
  }
}

static void Unimplemented(atomic_flag* reported, const char* name) {
  if (!atomic_flag_test_and_set(reported)) {
    fprintf(stderr, "softgpu: unimplemented gl%s\n", name);
  }
}

static float ToWindow(float ndc, int origin, int size) {
  float window = (ndc + 1.0f) * 0.5f * (float)size + (float)origin;
  return fmaxf(-(float)kGuardBand, fminf(window, (float)kGuardBand));
}

static void DrawTriangle(Context* context) {
  int clip_x0 = context->viewport[0] > 0 ? context->viewport[0] : 0;
  int clip_y0 = context->viewport[1] > 0 ? context->viewport[1] : 0;
  int clip_x1 = context->viewport[0] + context->viewport[2];
  int clip_y1 = context->viewport[1] + context->viewport[3];
  clip_x1 = clip_x1 < context->width ? clip_x1 : context->width;
  clip_y1 = clip_y1 < context->height ? clip_y1 : context->height;
  if (clip_x0 >= clip_x1 || clip_y0 >= clip_y1) {
    return;
  }

  const Vertex* v[3] = {&context->vertices[0], &context->vertices[1],
                        &context->vertices[2]};
  int64_t x[3];
  int64_t y[3];
  for (int i = 0; i < 3; i++) {
    x[i] = llroundf(v[i]->x * kSubpixels);
    y[i] = llroundf(v[i]->y * kSubpixels);
  }
  int64_t area = (x[1] - x[0]) * (y[2] - y[0]) - (x[2] - x[0]) * (y[1] - y[0]);
  if (area == 0) {
    return;
  }
  if (area < 0) {
    const Vertex* vertex = v[1];
    v[1] = v[2];
    v[2] = vertex;
    int64_t t = x[1];
    x[1] = x[2];
    x[2] = t;
    t = y[1];
    y[1] = y[2];
    y[2] = t;
    area = -area;
  }

  int min_x = ClampInt((int)floorf(fminf(v[0]->x, fminf(v[1]->x, v[2]->x))),
                       clip_x0, clip_x1 - 1);
  int max_x = ClampInt((int)floorf(fmaxf(v[0]->x, fmaxf(v[1]->x, v[2]->x))),
                       clip_x0, clip_x1 - 1);
  int min_y = ClampInt((int)floorf(fminf(v[0]->y, fminf(v[1]->y, v[2]->y))),
                       clip_y0, clip_y1 - 1);
  int max_y = ClampInt((int)floorf(fmaxf(v[0]->y, fmaxf(v[1]->y, v[2]->y))),
                       clip_y0, clip_y1 - 1);

  int64_t px = (int64_t)min_x * kSubpixels + kSubpixels / 2;
  int64_t py = (int64_t)min_y * kSubpixels + kSubpixels / 2;
  int64_t row[3];
  int64_t step_x[3];
  int64_t step_y[3];
  for (int k = 0; k < 3; k++) {
    int p = (k + 1) % 3;
    int q = (k + 2) % 3;
    int64_t dx = x[q] - x[p];
    int64_t dy = y[q] - y[p];
    bool top_left = dy < 0 || (dy == 0 && dx < 0);
    row[k] = dx * (py - y[p]) - dy * (px - x[p]) - (top_left ? 0 : 1);
    step_x[k] = -dy * kSubpixels;
    step_y[k] = dx * kSubpixels;
  }

  float inverse_area = 1.0f / (float)area;
  for (int pixel_y = min_y; pixel_y <= max_y; pixel_y++) {
    int64_t e[3] = {row[0], row[1], row[2]};
    uint32_t* line = context->pixels + (size_t)pixel_y * (size_t)context->width;
    for (int pixel_x = min_x; pixel_x <= max_x; pixel_x++) {
      if ((e[0] | e[1] | e[2]) >= 0) {
        float color[4];
        for (int c = 0; c < 4; c++) {
          color[c] =
              ((float)e[0] * v[0]->color[c] + (float)e[1] * v[1]->color[c] +
               (float)e[2] * v[2]->color[c]) *
              inverse_area;
        }
        line[pixel_x] = PackColor(color);
      }
      for (int k = 0; k < 3; k++) {
        e[k] += step_x[k];
      }
    }
    for (int k = 0; k < 3; k++) {
      row[k] += step_y[k];
    }
  }
}

static void APIENTRY Viewport(GLint x, GLint y, GLsizei width, GLsizei height) {
  if (width >= 0 && height >= 0) {
    current->viewport[0] = x;
    current->viewport[1] = y;
    current->viewport[2] = width;
    current->viewport[3] = height;
  }
}

static void APIENTRY Begin(GLenum mode) {
  static atomic_flag reported = ATOMIC_FLAG_INIT;
  if (mode != GL_TRIANGLES) {
    Unimplemented(&reported, "Begin with a mode other than GL_TRIANGLES");
  }
  current->mode = mode;
  current->drawing = true;
  current->vertex_count = 0;
}

static void APIENTRY End(void) { current->drawing = false; }

static void APIENTRY Color3f(GLfloat red, GLfloat green, GLfloat blue) {
  current->color[0] = red;
  current->color[1] = green;
  current->color[2] = blue;
  current->color[3] = 1.0f;
}

static void APIENTRY Vertex2f(GLfloat x, GLfloat y) {
  Context* context = current;
  if (!context->drawing) {
    return;
  }
  Vertex* vertex = &context->vertices[context->vertex_count++];
  vertex->x = ToWindow(x, context->viewport[0], context->viewport[2]);
  vertex->y = ToWindow(y, context->viewport[1], context->viewport[3]);
  memcpy(vertex->color, context->color, sizeof(vertex->color));
  if (context->vertex_count == 3) {
    context->vertex_count = 0;
    if (context->mode == GL_TRIANGLES) {
      DrawTriangle(context);
    }
  }
}

static void APIENTRY Flush(void) {}

#define SOFTGPU_STUB(ret, name, params, value)      \
  static ret APIENTRY Stub##name params {           \
    static atomic_flag reported = ATOMIC_FLAG_INIT; \
    Unimplemented(&reported, #name);                \
    return value;                                   \
  }
SOFTGPU_GL11(SOFTGPU_STUB)
#undef SOFTGPU_STUB

#define SOFTGPU_STUB_PROC(ret, name, params, value) (PROC) Stub##name,
static const PROC kStubs[] = {SOFTGPU_GL11(SOFTGPU_STUB_PROC)};
#undef SOFTGPU_STUB_PROC

static ProcTable table = {
    .count = kGlEntryCount,
    .entries =
        {
            [kGlBegin] = (PROC)Begin,
            [kGlClear] = (PROC)Clear,
            [kGlClearColor] = (PROC)ClearColor,
            [kGlColor3f] = (PROC)Color3f,
            [kGlEnd] = (PROC)End,
            [kGlFlush] = (PROC)Flush,
            [kGlGetString] = (PROC)GetString,
            [kGlVertex2f] = (PROC)Vertex2f,
            [kGlViewport] = (PROC)Viewport,
        },
};

static Context* Lookup(ULONG handle) {
  if (handle == 0 || handle > kMaxContexts || !atomic_load(&used[handle - 1])) {
    return NULL;
  }
  return &contexts[handle - 1];
}

static BOOL Resize(Context* context, HDC hdc) {
  HWND window = WindowFromDC(hdc);
  RECT rect;
  if (!window || !GetClientRect(window, &rect)) {
    return FALSE;
  }
  if (rect.right != context->width || rect.bottom != context->height) {
    free(context->pixels);
    context->width = rect.right;
    context->height = rect.bottom;
    size_t count = (size_t)context->width * (size_t)context->height;
    context->pixels = count ? Allocate(count * sizeof(uint32_t)) : NULL;
  }
  return TRUE;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
  if (reason == DLL_PROCESS_ATTACH) {
    for (int i = 0; i < kGlEntryCount; i++) {
      if (!table.entries[i]) {
        table.entries[i] = kStubs[i];
      }
    }
  }
  return TRUE;
}

BOOL APIENTRY DrvValidateVersion(ULONG version) { return TRUE; }

void APIENTRY DrvSetCallbackProcs(INT count, PROC* procs) {}

LONG APIENTRY DrvDescribePixelFormat(HDC hdc, INT format, ULONG size,
                                     PIXELFORMATDESCRIPTOR* descriptor) {
  if (!descriptor) {
    return kPixelFormatCount;
  }
  if (format < 1 || format > kPixelFormatCount || size < sizeof(*descriptor)) {
    return 0;
  }
  *descriptor = (PIXELFORMATDESCRIPTOR){
      .nSize = sizeof(*descriptor),
      .nVersion = 1,
      .dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER |
                 PFD_SUPPORT_COMPOSITION,
      .iPixelType = PFD_TYPE_RGBA,
      .cColorBits = 32,
      .cRedBits = 8,
      .cRedShift = 16,
      .cGreenBits = 8,
      .cGreenShift = 8,
      .cBlueBits = 8,
      .cAlphaBits = 8,
      .cAlphaShift = 24,
      .iLayerType = PFD_MAIN_PLANE,
  };
  return kPixelFormatCount;
}

BOOL APIENTRY DrvSetPixelFormat(HDC hdc, LONG format) {
  return format >= 1 && format <= kPixelFormatCount;
}

ULONG APIENTRY DrvCreateContext(HDC hdc) {
  for (ULONG i = 0; i < kMaxContexts; i++) {
    int expected = 0;
    if (atomic_compare_exchange_strong(&used[i], &expected, 1)) {
      contexts[i] = (Context){.color = {1.0f, 1.0f, 1.0f, 1.0f}};
      return i + 1;
    }
  }
  return 0;
}

ULONG APIENTRY DrvCreateLayerContext(HDC hdc, INT layer) {
  return layer == 0 ? DrvCreateContext(hdc) : 0;
}

BOOL APIENTRY DrvDeleteContext(ULONG handle) {
  Context* context = Lookup(handle);
  if (!context) {
    return FALSE;
  }
  if (current == context) {
    current = NULL;
  }
  free(context->pixels);
  *context = (Context){0};
  atomic_store(&used[handle - 1], 0);
  return TRUE;
}

BOOL APIENTRY DrvCopyContext(ULONG source, ULONG destination, UINT mask) {
  return FALSE;
}

BOOL APIENTRY DrvShareLists(ULONG first, ULONG second) { return TRUE; }

ProcTable* APIENTRY DrvSetContext(HDC hdc, ULONG handle, PROC set_proc_table) {
  Context* context = Lookup(handle);
  if (!context || !Resize(context, hdc)) {
    return NULL;
  }
  if (!context->attached) {
    context->attached = true;
    context->viewport[2] = context->width;
    context->viewport[3] = context->height;
  }
  current = context;
  return &table;
}

BOOL APIENTRY DrvReleaseContext(ULONG handle) {
  if (current && current == Lookup(handle)) {
    current = NULL;
  }
  return TRUE;
}

PROC APIENTRY DrvGetProcAddress(LPCSTR name) { return NULL; }

BOOL APIENTRY DrvSwapBuffers(HDC hdc) {
  Context* context = current;
  if (!context) {
    return FALSE;
  }
  if (context->pixels) {
    BITMAPINFO info = {
        .bmiHeader =
            {
                .biSize = sizeof(info.bmiHeader),
                .biWidth = context->width,
                .biHeight = context->height,
                .biPlanes = 1,
                .biBitCount = 32,
                .biCompression = BI_RGB,
            },
    };
    StretchDIBits(hdc, 0, 0, context->width, context->height, 0, 0,
                  context->width, context->height, context->pixels, &info,
                  DIB_RGB_COLORS, SRCCOPY);
  }
  return Resize(context, hdc);
}

BOOL APIENTRY DrvSwapLayerBuffers(HDC hdc, UINT planes) {
  return (planes & WGL_SWAP_MAIN_PLANE) ? DrvSwapBuffers(hdc) : FALSE;
}

BOOL APIENTRY DrvDescribeLayerPlane(HDC hdc, INT format, INT layer, UINT size,
                                    LAYERPLANEDESCRIPTOR* plane) {
  return FALSE;
}

INT APIENTRY DrvSetLayerPaletteEntries(HDC hdc, INT layer, INT start, INT count,
                                       const COLORREF* colors) {
  return 0;
}

INT APIENTRY DrvGetLayerPaletteEntries(HDC hdc, INT layer, INT start, INT count,
                                       COLORREF* colors) {
  return 0;
}

BOOL APIENTRY DrvRealizeLayerPalette(HDC hdc, INT layer, BOOL realize) {
  return FALSE;
}
