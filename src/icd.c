#include "icd.h"

#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

_Static_assert(sizeof(void*) == 8, "softgpu builds for x64 only");
_Static_assert(kGlEntryCount == 336, "GL 1.1 has 336 dispatch entries");

enum { kMaxContexts = 64, kPixelFormatCount = 1 };

typedef struct Context {
  int width;
  int height;
  uint32_t* pixels;
  uint32_t clear_color;
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

static void APIENTRY ClearColor(GLclampf red, GLclampf green, GLclampf blue,
                                GLclampf alpha) {
  current->clear_color = PackChannel(alpha) << 24 | PackChannel(red) << 16 |
                         PackChannel(green) << 8 | PackChannel(blue);
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
            [kGlClear] = (PROC)Clear,
            [kGlClearColor] = (PROC)ClearColor,
            [kGlGetString] = (PROC)GetString,
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
      .dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
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
