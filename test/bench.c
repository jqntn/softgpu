#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "icd.h"

#define SOFTGPU_FIELD(ret, name, params) ret(APIENTRY* name) params;
typedef struct Gl {
  SOFTGPU_GL11(SOFTGPU_FIELD)
} Gl;
#undef SOFTGPU_FIELD

typedef struct Test {
  const char* name;
  void (*draw)(void);
} Test;

enum {
  kWidth = 640,
  kHeight = 480,
  kTextureSize = 64,
  kWarmupRuns = 1,
  kRuns = 7,
  kFrames = 10,
};

static Gl gl;

static void Fail(const char* message) {
  fprintf(stderr, "%s\n", message);
  exit(EXIT_FAILURE);
}

static ULONG_PTR PerformanceCores(void) {
  DWORD length = 0;
  GetLogicalProcessorInformationEx(RelationProcessorCore, NULL, &length);
  char* buffer = malloc(length);
  if (!buffer ||
      !GetLogicalProcessorInformationEx(
          RelationProcessorCore,
          (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)buffer, &length)) {
    Fail("GetLogicalProcessorInformationEx failed");
  }
  BYTE best = 0;
  ULONG_PTR mask = 0;
  for (DWORD offset = 0; offset < length;) {
    const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX* info =
        (const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*)(buffer + offset);
    if (info->Processor.EfficiencyClass > best) {
      best = info->Processor.EfficiencyClass;
      mask = 0;
    }
    if (info->Processor.EfficiencyClass == best) {
      mask |= info->Processor.GroupMask[0].Mask;
    }
    offset += info->Size;
  }
  free(buffer);
  return mask;
}

static void UsePerformanceCores(void) {
  if (!SetProcessAffinityMask(GetCurrentProcess(), PerformanceCores())) {
    Fail("cannot keep the benchmark on the performance cores");
  }
}

static void Quads(int count) {
  gl.Begin(GL_QUADS);
  for (int i = 0; i < count; i++) {
    float z = -1.5f - 0.01f * (float)i;
    gl.TexCoord2f(0.0f, 0.0f);
    gl.Vertex3f(-2.0f, -2.0f, z);
    gl.TexCoord2f(8.0f, 0.0f);
    gl.Vertex3f(2.0f, -2.0f, z - 3.0f);
    gl.TexCoord2f(8.0f, 8.0f);
    gl.Vertex3f(2.0f, 2.0f, z - 3.0f);
    gl.TexCoord2f(0.0f, 8.0f);
    gl.Vertex3f(-2.0f, 2.0f, z);
  }
  gl.End();
}

static void Fill(void) { Quads(4); }

static void Blend(void) {
  Quads(1);
  gl.Enable(GL_BLEND);
  gl.BlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
  Quads(1);
  gl.Disable(GL_BLEND);
}

static void Small(void) {
  gl.Begin(GL_TRIANGLES);
  for (int i = 0; i < 20000; i++) {
    float x = (float)(i % 40) / 20.0f - 1.0f;
    float y = (float)(i / 40 % 40) / 20.0f - 1.0f;
    gl.TexCoord2f(0.0f, 0.0f);
    gl.Vertex3f(x, y, -2.0f);
    gl.TexCoord2f(1.0f, 0.0f);
    gl.Vertex3f(x + 0.06f, y, -2.0f);
    gl.TexCoord2f(0.0f, 1.0f);
    gl.Vertex3f(x, y + 0.06f, -2.1f);
  }
  gl.End();
}

static const Test kTests[] = {
    {"fill", Fill},
    {"blend", Blend},
    {"small", Small},
};

static void SetupScene(void) {
  static GLubyte texels[kTextureSize * kTextureSize * 3];
  for (int i = 0; i < kTextureSize * kTextureSize * 3; i++) {
    texels[i] = (GLubyte)(i * 37 + (i >> 7) * 11);
  }
  for (int level = 0, size = kTextureSize; size >= 1; level++, size /= 2) {
    gl.TexImage2D(GL_TEXTURE_2D, level, GL_RGB, size, size, 0, GL_RGB,
                  GL_UNSIGNED_BYTE, texels);
  }
  gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                   GL_LINEAR_MIPMAP_LINEAR);
  gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  gl.Enable(GL_TEXTURE_2D);
  gl.Enable(GL_DEPTH_TEST);
  gl.DepthFunc(GL_LEQUAL);
  gl.MatrixMode(GL_PROJECTION);
  gl.Frustum(-1.0, 1.0, -0.75, 0.75, 1.0, 100.0);
  gl.MatrixMode(GL_MODELVIEW);
}

static double Seconds(void) {
  LARGE_INTEGER frequency;
  LARGE_INTEGER counter;
  QueryPerformanceFrequency(&frequency);
  QueryPerformanceCounter(&counter);
  return (double)counter.QuadPart / (double)frequency.QuadPart;
}

int main(void) {
  UsePerformanceCores();
  RegisterClassA(&(WNDCLASSA){
      .lpfnWndProc = DefWindowProcA,
      .hInstance = GetModuleHandleA(NULL),
      .lpszClassName = "bench",
  });
  RECT rect = {0, 0, kWidth, kHeight};
  AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
  HWND window = CreateWindowA("bench", "bench", WS_OVERLAPPEDWINDOW, 0, 0,
                              rect.right - rect.left, rect.bottom - rect.top,
                              NULL, NULL, NULL, NULL);
  if (!window) {
    Fail("CreateWindowA failed");
  }
  HDC hdc = GetDC(window);
  ULONG context = DrvSetPixelFormat(hdc, 1) ? DrvCreateContext(hdc) : 0;
  ProcTable* table = context ? DrvSetContext(hdc, context, NULL) : NULL;
  if (!table) {
    Fail("no context");
  }
  memcpy(&gl, table->entries, sizeof(gl));
  SetupScene();
  for (size_t t = 0; t < sizeof(kTests) / sizeof(kTests[0]); t++) {
    double best = INFINITY;
    for (int run = 0; run < kWarmupRuns + kRuns; run++) {
      double start = Seconds();
      for (int frame = 0; frame < kFrames; frame++) {
        gl.Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        kTests[t].draw();
      }
      gl.Finish();
      double elapsed = (Seconds() - start) * 1000.0 / kFrames;
      if (run >= kWarmupRuns && elapsed < best) {
        best = elapsed;
      }
    }
    printf("%-6s %7.2f ms\n", kTests[t].name, best);
  }
  return 0;
}
