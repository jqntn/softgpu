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
  kChannelBits = 8,
  kDepthBits = 24,
  kStencilBits = 8,
  kStencilMax = (1 << kStencilBits) - 1,
  kSubpixels = 16,
  kGuardBand = 1 << 19,
  kModelviewDepth = 32,
  kProjectionDepth = 2,
  kMaxTextures = 4096,
  kMaxLevels = 12,
  kMaxTextureSize = 1 << (kMaxLevels - 1),
  kClipPlanes = 6,
  kMaxClipVertices = 3 + kClipPlanes
};

enum { kRed, kGreen, kBlue, kAlpha, kS, kT, kFog, kVaryings };

static const float kIdentity[16] = {1, 0, 0, 0, 0, 1, 0, 0,
                                    0, 0, 1, 0, 0, 0, 0, 1};
static const float kRadiansPerDegree = 0.0174532925f;
static const float kDepthUnit = 1.0f / 16777216.0f;
static const float kIntegralFloat = 8388608.0f;

typedef struct Vertex {
  float position[4];
  float varying[kVaryings];
} Vertex;

typedef struct Corner {
  float x;
  float y;
  float z;
  float q;
  float varying[kVaryings];
} Corner;

typedef struct Level {
  int width;
  int height;
  uint8_t (*texels)[4];
} Level;

typedef struct Texture {
  bool named;
  GLenum format;
  GLenum min_filter;
  GLenum mag_filter;
  Level levels[kMaxLevels];
} Texture;

typedef struct Setup {
  const Corner* corners[3];
  float offset;
  const Texture* texture;
  int levels;
  float derivative[2][3];
} Setup;

typedef struct Context {
  int width;
  int height;
  uint32_t* pixels;
  float* depth;
  uint8_t* stencil;
  uint32_t clear_color;
  GLuint clear_stencil;
  bool attached;
  int viewport[4];
  int scissor[4];
  float depth_range[2];
  float color[4];
  float texcoord[2];
  GLenum matrix_mode;
  int modelview_top;
  int projection_top;
  float modelview[kModelviewDepth][16];
  float projection[kProjectionDepth][16];
  GLenum mode;
  bool drawing;
  int vertex_count;
  Vertex first;
  Vertex recent[4];
  GLenum shade_model;
  bool cull_face;
  GLenum cull_face_mode;
  GLenum front_face;
  bool depth_test;
  bool depth_mask;
  GLenum depth_func;
  bool stencil_test;
  GLenum stencil_func;
  int stencil_ref;
  GLuint stencil_value_mask;
  GLenum stencil_fail;
  GLenum stencil_zfail;
  GLenum stencil_zpass;
  GLuint stencil_writemask;
  bool alpha_test;
  GLenum alpha_func;
  float alpha_ref;
  bool blend;
  GLenum blend_src;
  GLenum blend_dst;
  bool scissor_test;
  bool polygon_offset_fill;
  float polygon_offset[2];
  bool fog;
  GLenum fog_mode;
  float fog_density;
  float fog_start;
  float fog_end;
  float fog_color[4];
  bool texture_2d;
  GLuint texture;
  GLenum texture_env_mode;
  int unpack_alignment;
  int pack_alignment;
} Context;

static Context contexts[kMaxContexts];
static atomic_int used[kMaxContexts];
static _Thread_local Context* current;
static Texture textures[kMaxTextures];
static const Texture kDefaultTexture = {
    .format = GL_RGBA,
    .min_filter = GL_NEAREST_MIPMAP_LINEAR,
    .mag_filter = GL_LINEAR,
};

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

static void Unimplemented(atomic_flag* reported, const char* name) {
  if (!atomic_flag_test_and_set(reported)) {
    fprintf(stderr, "softgpu: unimplemented gl%s\n", name);
  }
}

static float MinFloat(float a, float b) { return a < b ? a : b; }

static float MaxFloat(float a, float b) { return a > b ? a : b; }

static float Clamp01(float value) {
  return MinFloat(MaxFloat(value, 0.0f), 1.0f);
}

static float Floor(float value) {
  if (!(value > -kIntegralFloat && value < kIntegralFloat)) {
    return value;
  }
  float truncated = (float)(int32_t)value;
  return truncated > value ? truncated - 1.0f : truncated;
}

static float Ceil(float value) { return -Floor(-value); }

static int ClampInt(int value, int low, int high) {
  return value < low ? low : (value > high ? high : value);
}

static int MinInt(int a, int b) { return a < b ? a : b; }

static int MaxInt(int a, int b) { return a > b ? a : b; }

static uint32_t PackChannel(GLclampf value) {
  return (uint32_t)(Clamp01(value) * 255.0f + 0.5f);
}

static uint32_t PackColor(const float color[4]) {
  return PackChannel(color[3]) << 24 | PackChannel(color[0]) << 16 |
         PackChannel(color[1]) << 8 | PackChannel(color[2]);
}

static void UnpackColor(uint32_t pixel, uint8_t rgba[4]) {
  rgba[0] = (uint8_t)(pixel >> 16);
  rgba[1] = (uint8_t)(pixel >> 8);
  rgba[2] = (uint8_t)pixel;
  rgba[3] = (uint8_t)(pixel >> 24);
}

static bool Compare(GLenum func, float value, float reference) {
  switch (func) {
    case GL_NEVER:
      return false;
    case GL_LESS:
      return value < reference;
    case GL_EQUAL:
      return value == reference;
    case GL_LEQUAL:
      return value <= reference;
    case GL_GREATER:
      return value > reference;
    case GL_NOTEQUAL:
      return value != reference;
    case GL_GEQUAL:
      return value >= reference;
    default:
      return true;
  }
}

static int CompleteLevels(const Texture* texture) {
  const Level* base = texture->levels;
  if (!base->texels) {
    return 0;
  }
  if (texture->min_filter == GL_NEAREST || texture->min_filter == GL_LINEAR) {
    return 1;
  }
  int count = 1;
  while ((base->width >> (count - 1)) > 1 ||
         (base->height >> (count - 1)) > 1) {
    const Level* level = &texture->levels[count];
    if (!level->texels || level->width != MaxInt(base->width >> count, 1) ||
        level->height != MaxInt(base->height >> count, 1)) {
      return 0;
    }
    count++;
  }
  return count;
}

static void SampleLevel(const Level* level, GLenum filter, float s, float t,
                        float texel[4]) {
  int mask_x = level->width - 1;
  int mask_y = level->height - 1;
  float u = (s - Floor(s)) * (float)level->width;
  float v = (t - Floor(t)) * (float)level->height;
  if (filter == GL_NEAREST) {
    const uint8_t* p =
        level->texels[((int)v & mask_y) * level->width + ((int)u & mask_x)];
    for (int i = 0; i < 4; i++) {
      texel[i] = (float)p[i] / 255.0f;
    }
    return;
  }
  u -= 0.5f;
  v -= 0.5f;
  float left = Floor(u);
  float bottom = Floor(v);
  float a = u - left;
  float b = v - bottom;
  int i0 = (int)left & mask_x;
  int i1 = ((int)left + 1) & mask_x;
  int j0 = ((int)bottom & mask_y) * level->width;
  int j1 = (((int)bottom + 1) & mask_y) * level->width;
  const uint8_t* p00 = level->texels[j0 + i0];
  const uint8_t* p10 = level->texels[j0 + i1];
  const uint8_t* p01 = level->texels[j1 + i0];
  const uint8_t* p11 = level->texels[j1 + i1];
  for (int i = 0; i < 4; i++) {
    float lower = (float)p00[i] + ((float)p10[i] - (float)p00[i]) * a;
    float upper = (float)p01[i] + ((float)p11[i] - (float)p01[i]) * a;
    texel[i] = (lower + (upper - lower) * b) / 255.0f;
  }
}

static void SampleTexture(const Texture* texture, int levels, float s, float t,
                          float lambda, float texel[4]) {
  GLenum minify = texture->min_filter;
  bool nearest =
      minify == GL_NEAREST_MIPMAP_NEAREST || minify == GL_NEAREST_MIPMAP_LINEAR;
  float c = texture->mag_filter == GL_LINEAR && nearest ? 0.5f : 0.0f;
  if (lambda <= c) {
    SampleLevel(texture->levels, texture->mag_filter, s, t, texel);
    return;
  }
  if (minify == GL_NEAREST || minify == GL_LINEAR) {
    SampleLevel(texture->levels, minify, s, t, texel);
    return;
  }
  GLenum filter = nearest ? GL_NEAREST : GL_LINEAR;
  float d = MinFloat(lambda, (float)(levels - 1));
  if (minify == GL_NEAREST_MIPMAP_NEAREST ||
      minify == GL_LINEAR_MIPMAP_NEAREST) {
    int level = d <= 0.5f ? 0 : (int)Ceil(d + 0.5f) - 1;
    SampleLevel(&texture->levels[level], filter, s, t, texel);
    return;
  }
  int level = (int)d;
  float fraction = d - (float)level;
  SampleLevel(&texture->levels[level], filter, s, t, texel);
  if (fraction > 0.0f && level + 1 < levels) {
    float next[4];
    SampleLevel(&texture->levels[level + 1], filter, s, t, next);
    for (int i = 0; i < 4; i++) {
      texel[i] += (next[i] - texel[i]) * fraction;
    }
  }
}

static float Lambda(const Setup* setup, const float varying[kVaryings],
                    float w) {
  const Level* base = setup->texture->levels;
  float rho = 0.0f;
  for (int axis = 0; axis < 2; axis++) {
    const float* d = setup->derivative[axis];
    float du = (d[0] - varying[kS] * d[2]) * w * (float)base->width;
    float dv = (d[1] - varying[kT] * d[2]) * w * (float)base->height;
    rho = MaxFloat(du * du + dv * dv, rho);
  }
  return 0.5f * log2f(rho);
}

static void ApplyTexture(GLenum mode, GLenum format, const float texel[4],
                         float color[4]) {
  bool has_color = format != GL_ALPHA;
  bool has_alpha = format != GL_RGB && format != GL_LUMINANCE;
  for (int i = 0; i < 3; i++) {
    if (mode == GL_DECAL) {
      color[i] = format == GL_RGBA ? color[i] + (texel[i] - color[i]) * texel[3]
                                   : texel[i];
    } else if (has_color) {
      color[i] = mode == GL_REPLACE ? texel[i] : color[i] * texel[i];
    }
  }
  if (mode != GL_DECAL && has_alpha) {
    color[3] = mode == GL_REPLACE ? texel[3] : color[3] * texel[3];
  }
}

static void ApplyFog(const Context* context, float distance, float color[4]) {
  float f;
  switch (context->fog_mode) {
    case GL_LINEAR:
      f = (context->fog_end - distance) /
          (context->fog_end - context->fog_start);
      break;
    case GL_EXP:
      f = expf(-context->fog_density * distance);
      break;
    default: {
      float x = context->fog_density * distance;
      f = expf(-x * x);
    }
  }
  f = Clamp01(f);
  for (int i = 0; i < 3; i++) {
    color[i] = f * color[i] + (1.0f - f) * context->fog_color[i];
  }
}

static float Factor(GLenum factor, const float source[4],
                    const float destination[4], int channel) {
  switch (factor) {
    case GL_ONE:
      return 1.0f;
    case GL_SRC_COLOR:
      return source[channel];
    case GL_ONE_MINUS_SRC_COLOR:
      return 1.0f - source[channel];
    case GL_DST_COLOR:
      return destination[channel];
    case GL_ONE_MINUS_DST_COLOR:
      return 1.0f - destination[channel];
    case GL_SRC_ALPHA:
      return source[3];
    case GL_ONE_MINUS_SRC_ALPHA:
      return 1.0f - source[3];
    case GL_DST_ALPHA:
      return destination[3];
    case GL_ONE_MINUS_DST_ALPHA:
      return 1.0f - destination[3];
    case GL_SRC_ALPHA_SATURATE:
      return channel == 3 ? 1.0f : MinFloat(source[3], 1.0f - destination[3]);
    default:
      return 0.0f;
  }
}

static void Blend(const Context* context, uint32_t pixel, float color[4]) {
  uint8_t rgba[4];
  UnpackColor(pixel, rgba);
  float source[4];
  float destination[4];
  for (int i = 0; i < 4; i++) {
    source[i] = color[i];
    destination[i] = (float)rgba[i] / 255.0f;
  }
  for (int i = 0; i < 4; i++) {
    color[i] =
        source[i] * Factor(context->blend_src, source, destination, i) +
        destination[i] * Factor(context->blend_dst, source, destination, i);
  }
}

static void WriteStencil(const Context* context, uint8_t* stencil,
                         GLuint value) {
  *stencil = (uint8_t)((*stencil & ~context->stencil_writemask) |
                       (value & context->stencil_writemask));
}

static void ApplyStencilOp(const Context* context, GLenum op,
                           uint8_t* stencil) {
  int value = *stencil;
  switch (op) {
    case GL_ZERO:
      value = 0;
      break;
    case GL_REPLACE:
      value = context->stencil_ref;
      break;
    case GL_INCR:
      value = MinInt(value + 1, kStencilMax);
      break;
    case GL_DECR:
      value = MaxInt(value - 1, 0);
      break;
    case GL_INVERT:
      value = ~value;
      break;
    default:
      return;
  }
  WriteStencil(context, stencil, (GLuint)value);
}

static void Shade(Context* context, const Setup* setup, size_t index,
                  const float weight[3]) {
  const Corner* const* v = setup->corners;
  float z = Clamp01(weight[0] * v[0]->z + weight[1] * v[1]->z +
                    weight[2] * v[2]->z + setup->offset);
  bool depth_pass = !context->depth_test ||
                    Compare(context->depth_func, z, context->depth[index]);
  if (!depth_pass && !context->stencil_test) {
    return;
  }
  float w =
      1.0f / (weight[0] * v[0]->q + weight[1] * v[1]->q + weight[2] * v[2]->q);
  float varying[kVaryings];
  for (int i = 0; i < kVaryings; i++) {
    varying[i] = (weight[0] * v[0]->varying[i] + weight[1] * v[1]->varying[i] +
                  weight[2] * v[2]->varying[i]) *
                 w;
  }
  float* color = varying;
  if (setup->texture) {
    float texel[4];
    float lambda = setup->texture->min_filter == setup->texture->mag_filter
                       ? 0.0f
                       : Lambda(setup, varying, w);
    SampleTexture(setup->texture, setup->levels, varying[kS], varying[kT],
                  lambda, texel);
    ApplyTexture(context->texture_env_mode, setup->texture->format, texel,
                 color);
  }
  if (context->fog) {
    ApplyFog(context, varying[kFog], color);
  }
  for (int i = 0; i < 4; i++) {
    color[i] = Clamp01(color[i]);
  }
  if (context->alpha_test &&
      !Compare(context->alpha_func, color[kAlpha], context->alpha_ref)) {
    return;
  }
  if (context->stencil_test) {
    uint8_t* stencil = &context->stencil[index];
    GLuint mask = context->stencil_value_mask;
    if (!Compare(context->stencil_func,
                 (float)((GLuint)context->stencil_ref & mask),
                 (float)(*stencil & mask))) {
      ApplyStencilOp(context, context->stencil_fail, stencil);
      return;
    }
    ApplyStencilOp(context,
                   depth_pass ? context->stencil_zpass : context->stencil_zfail,
                   stencil);
  }
  if (!depth_pass) {
    return;
  }
  if (context->depth_test && context->depth_mask) {
    context->depth[index] = z;
  }
  if (context->blend) {
    Blend(context, context->pixels[index], color);
  }
  context->pixels[index] = PackColor(color);
}

static float ToWindow(float ndc, int origin, int size) {
  float window = (ndc + 1.0f) * 0.5f * (float)size + (float)origin;
  return MaxFloat(MinFloat(window, (float)kGuardBand), -(float)kGuardBand);
}

static void Rasterize(Context* context, const Vertex* const input[3]) {
  int clip_x0 = context->viewport[0];
  int clip_y0 = context->viewport[1];
  int clip_x1 = clip_x0 + context->viewport[2];
  int clip_y1 = clip_y0 + context->viewport[3];
  if (context->scissor_test) {
    clip_x0 = MaxInt(clip_x0, context->scissor[0]);
    clip_y0 = MaxInt(clip_y0, context->scissor[1]);
    clip_x1 = MinInt(clip_x1, context->scissor[0] + context->scissor[2]);
    clip_y1 = MinInt(clip_y1, context->scissor[1] + context->scissor[3]);
  }
  clip_x0 = MaxInt(clip_x0, 0);
  clip_y0 = MaxInt(clip_y0, 0);
  clip_x1 = MinInt(clip_x1, context->width);
  clip_y1 = MinInt(clip_y1, context->height);
  if (clip_x0 >= clip_x1 || clip_y0 >= clip_y1) {
    return;
  }

  Corner corners[3];
  int64_t x[3];
  int64_t y[3];
  float depth_scale =
      0.5f * (context->depth_range[1] - context->depth_range[0]);
  for (int i = 0; i < 3; i++) {
    const float* position = input[i]->position;
    if (!(position[3] > 0.0f)) {
      return;
    }
    Corner* corner = &corners[i];
    corner->q = 1.0f / position[3];
    corner->x = ToWindow(position[0] * corner->q, context->viewport[0],
                         context->viewport[2]);
    corner->y = ToWindow(position[1] * corner->q, context->viewport[1],
                         context->viewport[3]);
    corner->z = context->depth_range[0] +
                (position[2] * corner->q + 1.0f) * depth_scale;
    for (int j = 0; j < kVaryings; j++) {
      corner->varying[j] = input[i]->varying[j] * corner->q;
    }
    x[i] = llroundf(corner->x * kSubpixels);
    y[i] = llroundf(corner->y * kSubpixels);
  }
  int64_t area = (x[1] - x[0]) * (y[2] - y[0]) - (x[2] - x[0]) * (y[1] - y[0]);
  if (area == 0) {
    return;
  }
  GLenum face =
      (area > 0) == (context->front_face == GL_CCW) ? GL_FRONT : GL_BACK;
  if (context->cull_face && (context->cull_face_mode == face ||
                             context->cull_face_mode == GL_FRONT_AND_BACK)) {
    return;
  }
  const Corner* v[3] = {&corners[0], &corners[1], &corners[2]};
  if (area < 0) {
    const Corner* corner = v[1];
    v[1] = v[2];
    v[2] = corner;
    int64_t t = x[1];
    x[1] = x[2];
    x[2] = t;
    t = y[1];
    y[1] = y[2];
    y[2] = t;
    area = -area;
  }

  int min_x =
      ClampInt((int)Floor(MinFloat(v[0]->x, MinFloat(v[1]->x, v[2]->x))),
               clip_x0, clip_x1 - 1);
  int max_x =
      ClampInt((int)Floor(MaxFloat(v[0]->x, MaxFloat(v[1]->x, v[2]->x))),
               clip_x0, clip_x1 - 1);
  int min_y =
      ClampInt((int)Floor(MinFloat(v[0]->y, MinFloat(v[1]->y, v[2]->y))),
               clip_y0, clip_y1 - 1);
  int max_y =
      ClampInt((int)Floor(MaxFloat(v[0]->y, MaxFloat(v[1]->y, v[2]->y))),
               clip_y0, clip_y1 - 1);

  int64_t px = (int64_t)min_x * kSubpixels + kSubpixels / 2;
  int64_t py = (int64_t)min_y * kSubpixels + kSubpixels / 2;
  int64_t row[3];
  int64_t bias[3];
  int64_t step_x[3];
  int64_t step_y[3];
  for (int k = 0; k < 3; k++) {
    int p = (k + 1) % 3;
    int q = (k + 2) % 3;
    int64_t dx = x[q] - x[p];
    int64_t dy = y[q] - y[p];
    bias[k] = dy < 0 || (dy == 0 && dx < 0) ? 0 : 1;
    row[k] = dx * (py - y[p]) - dy * (px - x[p]) - bias[k];
    step_x[k] = -dy * kSubpixels;
    step_y[k] = dx * kSubpixels;
  }

  float inverse_area = 1.0f / (float)area;
  float gradient[2][3];
  for (int k = 0; k < 3; k++) {
    gradient[0][k] = (float)step_x[k] * inverse_area;
    gradient[1][k] = (float)step_y[k] * inverse_area;
  }
  Setup setup = {.corners = {v[0], v[1], v[2]}};
  if (context->polygon_offset_fill) {
    float slope = 0.0f;
    for (int axis = 0; axis < 2; axis++) {
      float dz = 0.0f;
      for (int k = 0; k < 3; k++) {
        dz += gradient[axis][k] * v[k]->z;
      }
      slope = MaxFloat(fabsf(dz), slope);
    }
    setup.offset = context->polygon_offset[0] * slope +
                   context->polygon_offset[1] * kDepthUnit;
  }
  if (context->texture_2d) {
    const Texture* texture = &textures[context->texture];
    setup.levels = CompleteLevels(texture);
    if (setup.levels) {
      setup.texture = texture;
      for (int axis = 0; axis < 2; axis++) {
        for (int k = 0; k < 3; k++) {
          setup.derivative[axis][0] += gradient[axis][k] * v[k]->varying[kS];
          setup.derivative[axis][1] += gradient[axis][k] * v[k]->varying[kT];
          setup.derivative[axis][2] += gradient[axis][k] * v[k]->q;
        }
      }
    }
  }

  for (int pixel_y = min_y; pixel_y <= max_y; pixel_y++) {
    int64_t e[3] = {row[0], row[1], row[2]};
    size_t line = (size_t)pixel_y * (size_t)context->width;
    for (int pixel_x = min_x; pixel_x <= max_x; pixel_x++) {
      if ((e[0] | e[1] | e[2]) >= 0) {
        float weight[3] = {(float)(e[0] + bias[0]) * inverse_area,
                           (float)(e[1] + bias[1]) * inverse_area,
                           (float)(e[2] + bias[2]) * inverse_area};
        Shade(context, &setup, line + (size_t)pixel_x, weight);
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

static float* CurrentMatrix(Context* context) {
  return context->matrix_mode == GL_PROJECTION
             ? context->projection[context->projection_top]
             : context->modelview[context->modelview_top];
}

static void Transform(const float m[16], const float in[4], float out[4]) {
  for (int r = 0; r < 4; r++) {
    out[r] =
        m[r] * in[0] + m[4 + r] * in[1] + m[8 + r] * in[2] + m[12 + r] * in[3];
  }
}

static void MultMatrix(float m[16], const float n[16]) {
  float product[16];
  for (int c = 0; c < 4; c++) {
    Transform(m, &n[c * 4], &product[c * 4]);
  }
  memcpy(m, product, sizeof(product));
}

static float PlaneDistance(const Vertex* vertex, int plane) {
  float w = vertex->position[3];
  float p = vertex->position[plane / 2];
  return plane % 2 ? w - p : w + p;
}

static void Interpolate(const Vertex* p, const Vertex* q, float t,
                        Vertex* out) {
  for (int i = 0; i < 4; i++) {
    out->position[i] = p->position[i] + (q->position[i] - p->position[i]) * t;
  }
  for (int i = 0; i < kVaryings; i++) {
    out->varying[i] = p->varying[i] + (q->varying[i] - p->varying[i]) * t;
  }
}

static int Clip(Vertex polygon[kMaxClipVertices], int count) {
  for (int plane = 0; plane < kClipPlanes && count > 0; plane++) {
    bool inside = true;
    for (int i = 0; i < count; i++) {
      inside = inside && PlaneDistance(&polygon[i], plane) >= 0.0f;
    }
    if (inside) {
      continue;
    }
    Vertex input[kMaxClipVertices];
    memcpy(input, polygon, (size_t)count * sizeof(Vertex));
    int output = 0;
    for (int i = 0; i < count; i++) {
      const Vertex* p = &input[i];
      const Vertex* q = &input[(i + 1) % count];
      float dp = PlaneDistance(p, plane);
      float dq = PlaneDistance(q, plane);
      if (dp >= 0.0f && output < kMaxClipVertices) {
        polygon[output++] = *p;
      }
      if ((dp >= 0.0f) != (dq >= 0.0f) && output < kMaxClipVertices) {
        Interpolate(p, q, dp / (dp - dq), &polygon[output++]);
      }
    }
    count = output;
  }
  return count;
}

static void DrawTriangle(Context* context, const Vertex* a, const Vertex* b,
                         const Vertex* c, const Vertex* provoking) {
  Vertex polygon[kMaxClipVertices] = {*a, *b, *c};
  if (context->shade_model == GL_FLAT) {
    for (int i = 0; i < 3; i++) {
      memcpy(polygon[i].varying, provoking->varying, 4 * sizeof(float));
    }
  }
  int count = Clip(polygon, 3);
  for (int i = 2; i < count; i++) {
    Rasterize(context, (const Vertex* const[3]){&polygon[0], &polygon[i - 1],
                                                &polygon[i]});
  }
}

static void Assemble(Context* context, const Vertex* vertex) {
  int k = context->vertex_count++;
  if (k == 0) {
    context->first = *vertex;
  }
  context->recent[k % 4] = *vertex;
  const Vertex* first = &context->first;
  const Vertex* v0 = &context->recent[(k + 1) % 4];
  const Vertex* v1 = &context->recent[(k + 2) % 4];
  const Vertex* v2 = &context->recent[(k + 3) % 4];
  const Vertex* v3 = &context->recent[k % 4];
  switch (context->mode) {
    case GL_TRIANGLES:
      if (k % 3 == 2) {
        DrawTriangle(context, v1, v2, v3, v3);
      }
      break;
    case GL_TRIANGLE_STRIP:
      if (k >= 2) {
        DrawTriangle(context, k % 2 ? v2 : v1, k % 2 ? v1 : v2, v3, v3);
      }
      break;
    case GL_TRIANGLE_FAN:
      if (k >= 2) {
        DrawTriangle(context, first, v2, v3, v3);
      }
      break;
    case GL_POLYGON:
      if (k >= 2) {
        DrawTriangle(context, first, v2, v3, first);
      }
      break;
    case GL_QUADS:
      if (k % 4 == 3) {
        DrawTriangle(context, v0, v1, v2, v3);
        DrawTriangle(context, v0, v2, v3, v3);
      }
      break;
    case GL_QUAD_STRIP:
      if (k >= 3 && k % 2 == 1) {
        DrawTriangle(context, v0, v1, v3, v3);
        DrawTriangle(context, v0, v3, v2, v3);
      }
      break;
    default:
      break;
  }
}

static void EmitVertex(GLfloat x, GLfloat y, GLfloat z) {
  Context* context = current;
  if (!context->drawing) {
    return;
  }
  float eye[4];
  Transform(context->modelview[context->modelview_top],
            (float[4]){x, y, z, 1.0f}, eye);
  Vertex vertex;
  Transform(context->projection[context->projection_top], eye, vertex.position);
  for (int i = 0; i < 4; i++) {
    vertex.varying[i] = Clamp01(context->color[i]);
  }
  vertex.varying[kS] = context->texcoord[0];
  vertex.varying[kT] = context->texcoord[1];
  vertex.varying[kFog] = fabsf(eye[2]);
  Assemble(context, &vertex);
}

static void APIENTRY Begin(GLenum mode) {
  static atomic_flag reported = ATOMIC_FLAG_INIT;
  if (mode <= GL_LINE_STRIP) {
    Unimplemented(&reported, "Begin with points or lines");
  }
  current->mode = mode;
  current->drawing = true;
  current->vertex_count = 0;
}

static void APIENTRY End(void) { current->drawing = false; }

static void SetColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
  current->color[0] = red;
  current->color[1] = green;
  current->color[2] = blue;
  current->color[3] = alpha;
}

static void APIENTRY Color3f(GLfloat red, GLfloat green, GLfloat blue) {
  SetColor(red, green, blue, 1.0f);
}

static void APIENTRY Color3fv(const GLfloat* v) {
  SetColor(v[0], v[1], v[2], 1.0f);
}

static void APIENTRY Color4f(GLfloat red, GLfloat green, GLfloat blue,
                             GLfloat alpha) {
  SetColor(red, green, blue, alpha);
}

static void APIENTRY Color4fv(const GLfloat* v) {
  SetColor(v[0], v[1], v[2], v[3]);
}

static void APIENTRY Color4ubv(const GLubyte* v) {
  SetColor((float)v[0] / 255.0f, (float)v[1] / 255.0f, (float)v[2] / 255.0f,
           (float)v[3] / 255.0f);
}

static void APIENTRY TexCoord2f(GLfloat s, GLfloat t) {
  current->texcoord[0] = s;
  current->texcoord[1] = t;
}

static void APIENTRY Vertex2f(GLfloat x, GLfloat y) { EmitVertex(x, y, 0.0f); }

static void APIENTRY Vertex3f(GLfloat x, GLfloat y, GLfloat z) {
  EmitVertex(x, y, z);
}

static void APIENTRY Vertex3fv(const GLfloat* v) {
  EmitVertex(v[0], v[1], v[2]);
}

static void APIENTRY MatrixMode(GLenum mode) {
  static atomic_flag reported = ATOMIC_FLAG_INIT;
  if (mode == GL_MODELVIEW || mode == GL_PROJECTION) {
    current->matrix_mode = mode;
  } else {
    Unimplemented(&reported, "MatrixMode with GL_TEXTURE");
  }
}

static void APIENTRY LoadIdentity(void) {
  memcpy(CurrentMatrix(current), kIdentity, sizeof(kIdentity));
}

static void APIENTRY PushMatrix(void) {
  Context* context = current;
  bool projection = context->matrix_mode == GL_PROJECTION;
  int* top = projection ? &context->projection_top : &context->modelview_top;
  if (*top + 1 < (projection ? kProjectionDepth : kModelviewDepth)) {
    const float* matrix = CurrentMatrix(context);
    (*top)++;
    memcpy(CurrentMatrix(context), matrix, sizeof(kIdentity));
  }
}

static void APIENTRY PopMatrix(void) {
  Context* context = current;
  int* top = context->matrix_mode == GL_PROJECTION ? &context->projection_top
                                                   : &context->modelview_top;
  if (*top > 0) {
    (*top)--;
  }
}

static void APIENTRY Translatef(GLfloat x, GLfloat y, GLfloat z) {
  MultMatrix(CurrentMatrix(current),
             (const float[16]){1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, x, y, z, 1});
}

static void APIENTRY Scalef(GLfloat x, GLfloat y, GLfloat z) {
  MultMatrix(CurrentMatrix(current),
             (const float[16]){x, 0, 0, 0, 0, y, 0, 0, 0, 0, z, 0, 0, 0, 0, 1});
}

static void APIENTRY Rotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z) {
  float length = sqrtf(x * x + y * y + z * z);
  if (length == 0.0f) {
    return;
  }
  x /= length;
  y /= length;
  z /= length;
  float c = cosf(angle * kRadiansPerDegree);
  float s = sinf(angle * kRadiansPerDegree);
  float t = 1.0f - c;
  MultMatrix(
      CurrentMatrix(current),
      (const float[16]){x * x * t + c, y * x * t + z * s, x * z * t - y * s, 0,
                        x * y * t - z * s, y * y * t + c, y * z * t + x * s, 0,
                        x * z * t + y * s, y * z * t - x * s, z * z * t + c, 0,
                        0, 0, 0, 1});
}

static void APIENTRY Ortho(GLdouble left, GLdouble right, GLdouble bottom,
                           GLdouble top, GLdouble z_near, GLdouble z_far) {
  if (left == right || bottom == top || z_near == z_far) {
    return;
  }
  double width = right - left;
  double height = top - bottom;
  double depth = z_far - z_near;
  MultMatrix(CurrentMatrix(current),
             (const float[16]){
                 (float)(2 / width), 0, 0, 0, 0, (float)(2 / height), 0, 0, 0,
                 0, (float)(-2 / depth), 0, (float)(-(right + left) / width),
                 (float)(-(top + bottom) / height),
                 (float)(-(z_far + z_near) / depth), 1});
}

static void APIENTRY Frustum(GLdouble left, GLdouble right, GLdouble bottom,
                             GLdouble top, GLdouble z_near, GLdouble z_far) {
  if (z_near <= 0 || z_far <= 0 || left == right || bottom == top ||
      z_near == z_far) {
    return;
  }
  double width = right - left;
  double height = top - bottom;
  double depth = z_far - z_near;
  MultMatrix(CurrentMatrix(current),
             (const float[16]){(float)(2 * z_near / width), 0, 0, 0, 0,
                               (float)(2 * z_near / height), 0, 0,
                               (float)((right + left) / width),
                               (float)((top + bottom) / height),
                               (float)(-(z_far + z_near) / depth), -1, 0, 0,
                               (float)(-2 * z_far * z_near / depth), 0});
}

static void APIENTRY Viewport(GLint x, GLint y, GLsizei width, GLsizei height) {
  if (width >= 0 && height >= 0) {
    current->viewport[0] = x;
    current->viewport[1] = y;
    current->viewport[2] = width;
    current->viewport[3] = height;
  }
}

static void APIENTRY DepthRange(GLclampd z_near, GLclampd z_far) {
  current->depth_range[0] = Clamp01((float)z_near);
  current->depth_range[1] = Clamp01((float)z_far);
}

static void SetCapability(GLenum cap, bool value) {
  static atomic_flag reported = ATOMIC_FLAG_INIT;
  Context* context = current;
  switch (cap) {
    case GL_ALPHA_TEST:
      context->alpha_test = value;
      break;
    case GL_BLEND:
      context->blend = value;
      break;
    case GL_CULL_FACE:
      context->cull_face = value;
      break;
    case GL_DEPTH_TEST:
      context->depth_test = value;
      break;
    case GL_FOG:
      context->fog = value;
      break;
    case GL_POLYGON_OFFSET_FILL:
      context->polygon_offset_fill = value;
      break;
    case GL_SCISSOR_TEST:
      context->scissor_test = value;
      break;
    case GL_STENCIL_TEST:
      context->stencil_test = value;
      break;
    case GL_TEXTURE_2D:
      context->texture_2d = value;
      break;
    default:
      Unimplemented(&reported, "Enable or glDisable with this capability");
  }
}

static void APIENTRY Enable(GLenum cap) { SetCapability(cap, true); }

static void APIENTRY Disable(GLenum cap) { SetCapability(cap, false); }

static void APIENTRY CullFace(GLenum mode) {
  if (mode == GL_FRONT || mode == GL_BACK || mode == GL_FRONT_AND_BACK) {
    current->cull_face_mode = mode;
  }
}

static void APIENTRY FrontFace(GLenum mode) {
  if (mode == GL_CW || mode == GL_CCW) {
    current->front_face = mode;
  }
}

static void APIENTRY ShadeModel(GLenum mode) {
  if (mode == GL_FLAT || mode == GL_SMOOTH) {
    current->shade_model = mode;
  }
}

static void APIENTRY PolygonMode(GLenum face, GLenum mode) {
  static atomic_flag reported = ATOMIC_FLAG_INIT;
  if (mode != GL_FILL) {
    Unimplemented(&reported, "PolygonMode other than GL_FILL");
  }
}

static void APIENTRY PolygonOffset(GLfloat factor, GLfloat units) {
  current->polygon_offset[0] = factor;
  current->polygon_offset[1] = units;
}

static void APIENTRY Hint(GLenum target, GLenum mode) {}

static void APIENTRY PixelStorei(GLenum pname, GLint param) {
  static atomic_flag reported = ATOMIC_FLAG_INIT;
  bool alignment = param == 1 || param == 2 || param == 4 || param == 8;
  if (pname == GL_UNPACK_ALIGNMENT) {
    if (alignment) {
      current->unpack_alignment = param;
    }
  } else if (pname == GL_PACK_ALIGNMENT) {
    if (alignment) {
      current->pack_alignment = param;
    }
  } else {
    Unimplemented(&reported, "PixelStorei other than alignments");
  }
}

static size_t RowSize(int width, int components, int alignment) {
  size_t size = (size_t)width * (size_t)components;
  return (size + (size_t)alignment - 1) / (size_t)alignment * (size_t)alignment;
}

static int Components(GLenum format) {
  switch (format) {
    case GL_RGBA:
      return 4;
    case GL_RGB:
      return 3;
    case GL_LUMINANCE_ALPHA:
      return 2;
    case GL_LUMINANCE:
    case GL_ALPHA:
      return 1;
    default:
      return 0;
  }
}

static GLenum BaseFormat(GLint internal_format) {
  switch (internal_format) {
    case 1:
    case GL_LUMINANCE:
    case GL_LUMINANCE4:
    case GL_LUMINANCE8:
    case GL_LUMINANCE12:
    case GL_LUMINANCE16:
      return GL_LUMINANCE;
    case 2:
    case GL_LUMINANCE_ALPHA:
    case GL_LUMINANCE4_ALPHA4:
    case GL_LUMINANCE6_ALPHA2:
    case GL_LUMINANCE8_ALPHA8:
    case GL_LUMINANCE12_ALPHA4:
    case GL_LUMINANCE12_ALPHA12:
    case GL_LUMINANCE16_ALPHA16:
      return GL_LUMINANCE_ALPHA;
    case 3:
    case GL_RGB:
    case GL_R3_G3_B2:
    case GL_RGB4:
    case GL_RGB5:
    case GL_RGB8:
    case GL_RGB10:
    case GL_RGB12:
    case GL_RGB16:
      return GL_RGB;
    case 4:
    case GL_RGBA:
    case GL_RGBA2:
    case GL_RGBA4:
    case GL_RGB5_A1:
    case GL_RGBA8:
    case GL_RGB10_A2:
    case GL_RGBA12:
    case GL_RGBA16:
      return GL_RGBA;
    case GL_ALPHA:
    case GL_ALPHA4:
    case GL_ALPHA8:
    case GL_ALPHA12:
    case GL_ALPHA16:
      return GL_ALPHA;
    case GL_INTENSITY:
    case GL_INTENSITY4:
    case GL_INTENSITY8:
    case GL_INTENSITY12:
    case GL_INTENSITY16:
      return GL_INTENSITY;
    default:
      return 0;
  }
}

static void ToRgba(GLenum format, const GLubyte* source, uint8_t rgba[4]) {
  switch (format) {
    case GL_RGBA:
      memcpy(rgba, source, 4);
      break;
    case GL_RGB:
      memcpy(rgba, (uint8_t[4]){source[0], source[1], source[2], 255}, 4);
      break;
    case GL_LUMINANCE_ALPHA:
      memcpy(rgba, (uint8_t[4]){source[0], source[0], source[0], source[1]}, 4);
      break;
    case GL_LUMINANCE:
      memcpy(rgba, (uint8_t[4]){source[0], source[0], source[0], 255}, 4);
      break;
    default:
      memcpy(rgba, (uint8_t[4]){0, 0, 0, source[0]}, 4);
  }
}

static void StoreTexel(GLenum format, const uint8_t rgba[4], uint8_t texel[4]) {
  uint8_t r = rgba[0];
  switch (format) {
    case GL_RGB:
      memcpy(texel, (uint8_t[4]){r, rgba[1], rgba[2], 255}, 4);
      break;
    case GL_LUMINANCE:
      memcpy(texel, (uint8_t[4]){r, r, r, 255}, 4);
      break;
    case GL_LUMINANCE_ALPHA:
      memcpy(texel, (uint8_t[4]){r, r, r, rgba[3]}, 4);
      break;
    case GL_INTENSITY:
      memcpy(texel, (uint8_t[4]){r, r, r, r}, 4);
      break;
    case GL_ALPHA:
      memcpy(texel, (uint8_t[4]){0, 0, 0, rgba[3]}, 4);
      break;
    default:
      memcpy(texel, rgba, 4);
  }
}

static void Upload(const Texture* texture, Level* level, int x0, int y0,
                   int width, int height, GLenum format,
                   const GLubyte* pixels) {
  int components = Components(format);
  size_t row = RowSize(width, components, current->unpack_alignment);
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      uint8_t rgba[4];
      ToRgba(format, pixels + (size_t)y * row + (size_t)(x * components), rgba);
      StoreTexel(texture->format, rgba,
                 level->texels[(size_t)(y0 + y) * (size_t)level->width +
                               (size_t)(x0 + x)]);
    }
  }
}

static Level* Region(GLint level, GLint x, GLint y, GLsizei width,
                     GLsizei height) {
  if (level < 0 || level >= kMaxLevels) {
    return NULL;
  }
  Level* region = &textures[current->texture].levels[level];
  if (x < 0 || y < 0 || width < 0 || height < 0 || x + width > region->width ||
      y + height > region->height) {
    return NULL;
  }
  return region;
}

static void APIENTRY GenTextures(GLsizei n, GLuint* names) {
  GLuint name = 1;
  for (GLsizei i = 0; i < n; i++) {
    while (name < kMaxTextures && textures[name].named) {
      name++;
    }
    if (name == kMaxTextures) {
      Fail("out of texture names");
    }
    textures[name].named = true;
    names[i] = name;
  }
}

static void APIENTRY DeleteTextures(GLsizei n, const GLuint* names) {
  for (GLsizei i = 0; i < n; i++) {
    GLuint name = names[i];
    if (name == 0 || name >= kMaxTextures) {
      continue;
    }
    for (int level = 0; level < kMaxLevels; level++) {
      free(textures[name].levels[level].texels);
    }
    textures[name] = kDefaultTexture;
    if (current->texture == name) {
      current->texture = 0;
    }
  }
}

static void APIENTRY BindTexture(GLenum target, GLuint texture) {
  static atomic_flag reported = ATOMIC_FLAG_INIT;
  if (target != GL_TEXTURE_2D || texture >= kMaxTextures) {
    Unimplemented(&reported, "BindTexture other than 2D names below 4096");
    return;
  }
  textures[texture].named = true;
  current->texture = texture;
}

static void SetTextureParameter(GLenum target, GLenum pname, GLint param) {
  static atomic_flag reported = ATOMIC_FLAG_INIT;
  Texture* texture = &textures[current->texture];
  bool simple = param == GL_NEAREST || param == GL_LINEAR;
  bool mipmap =
      param >= GL_NEAREST_MIPMAP_NEAREST && param <= GL_LINEAR_MIPMAP_LINEAR;
  bool wrap = pname == GL_TEXTURE_WRAP_S || pname == GL_TEXTURE_WRAP_T;
  if (target == GL_TEXTURE_2D && pname == GL_TEXTURE_MIN_FILTER) {
    if (simple || mipmap) {
      texture->min_filter = (GLenum)param;
    }
  } else if (target == GL_TEXTURE_2D && pname == GL_TEXTURE_MAG_FILTER) {
    if (simple) {
      texture->mag_filter = (GLenum)param;
    }
  } else if (target != GL_TEXTURE_2D || !wrap || param != GL_REPEAT) {
    Unimplemented(&reported, "TexParameter other than filters and GL_REPEAT");
  }
}

static void APIENTRY TexParameterf(GLenum target, GLenum pname, GLfloat param) {
  SetTextureParameter(target, pname, (GLint)param);
}

static void APIENTRY TexParameteri(GLenum target, GLenum pname, GLint param) {
  SetTextureParameter(target, pname, param);
}

static void SetTextureEnvironment(GLenum target, GLenum pname, GLint param) {
  static atomic_flag reported = ATOMIC_FLAG_INIT;
  if (target == GL_TEXTURE_ENV && pname == GL_TEXTURE_ENV_MODE &&
      (param == GL_MODULATE || param == GL_REPLACE || param == GL_DECAL)) {
    current->texture_env_mode = (GLenum)param;
  } else {
    Unimplemented(&reported,
                  "TexEnv other than GL_MODULATE, GL_REPLACE or GL_DECAL");
  }
}

static void APIENTRY TexEnvf(GLenum target, GLenum pname, GLfloat param) {
  SetTextureEnvironment(target, pname, (GLint)param);
}

static void APIENTRY TexEnvi(GLenum target, GLenum pname, GLint param) {
  SetTextureEnvironment(target, pname, param);
}

static void APIENTRY TexImage2D(GLenum target, GLint level,
                                GLint internal_format, GLsizei width,
                                GLsizei height, GLint border, GLenum format,
                                GLenum type, const GLvoid* pixels) {
  static atomic_flag reported = ATOMIC_FLAG_INIT;
  GLenum base = BaseFormat(internal_format);
  if (target != GL_TEXTURE_2D || border != 0 || type != GL_UNSIGNED_BYTE ||
      !Components(format) || !base) {
    Unimplemented(&reported, "TexImage2D other than unsigned bytes");
    return;
  }
  if (level < 0 || level >= kMaxLevels || width < 0 || height < 0 ||
      width > kMaxTextureSize || height > kMaxTextureSize ||
      (width & (width - 1)) != 0 || (height & (height - 1)) != 0) {
    return;
  }
  Texture* texture = &textures[current->texture];
  Level* destination = &texture->levels[level];
  free(destination->texels);
  size_t size = (size_t)width * (size_t)height * 4;
  *destination = (Level){
      .width = width,
      .height = height,
      .texels = size ? Allocate(size) : NULL,
  };
  texture->format = base;
  if (pixels) {
    Upload(texture, destination, 0, 0, width, height, format, pixels);
  } else if (size) {
    memset(destination->texels, 0, size);
  }
}

static void APIENTRY TexSubImage2D(GLenum target, GLint level, GLint xoffset,
                                   GLint yoffset, GLsizei width, GLsizei height,
                                   GLenum format, GLenum type,
                                   const GLvoid* pixels) {
  static atomic_flag reported = ATOMIC_FLAG_INIT;
  if (target != GL_TEXTURE_2D || type != GL_UNSIGNED_BYTE ||
      !Components(format)) {
    Unimplemented(&reported, "TexSubImage2D other than unsigned bytes");
    return;
  }
  Level* region = Region(level, xoffset, yoffset, width, height);
  if (region && pixels) {
    Upload(&textures[current->texture], region, xoffset, yoffset, width, height,
           format, pixels);
  }
}

static void APIENTRY CopyTexSubImage2D(GLenum target, GLint level,
                                       GLint xoffset, GLint yoffset, GLint x,
                                       GLint y, GLsizei width, GLsizei height) {
  static atomic_flag reported = ATOMIC_FLAG_INIT;
  if (target != GL_TEXTURE_2D) {
    Unimplemented(&reported, "CopyTexSubImage2D other than 2D");
    return;
  }
  Context* context = current;
  Level* region = Region(level, xoffset, yoffset, width, height);
  if (!region) {
    return;
  }
  GLenum format = textures[context->texture].format;
  for (int j = 0; j < height; j++) {
    int source_y = y + j;
    for (int i = 0; i < width; i++) {
      int source_x = x + i;
      if (source_x < 0 || source_y < 0 || source_x >= context->width ||
          source_y >= context->height) {
        continue;
      }
      uint8_t rgba[4];
      UnpackColor(context->pixels[(size_t)source_y * (size_t)context->width +
                                  (size_t)source_x],
                  rgba);
      StoreTexel(format, rgba,
                 region->texels[(size_t)(yoffset + j) * (size_t)region->width +
                                (size_t)(xoffset + i)]);
    }
  }
}

static void SetFog(GLenum pname, const GLfloat* params) {
  static atomic_flag reported = ATOMIC_FLAG_INIT;
  Context* context = current;
  switch (pname) {
    case GL_FOG_MODE: {
      GLenum mode = (GLenum)params[0];
      if (mode == GL_LINEAR || mode == GL_EXP || mode == GL_EXP2) {
        context->fog_mode = mode;
      }
      break;
    }
    case GL_FOG_DENSITY:
      if (params[0] >= 0.0f) {
        context->fog_density = params[0];
      }
      break;
    case GL_FOG_START:
      context->fog_start = params[0];
      break;
    case GL_FOG_END:
      context->fog_end = params[0];
      break;
    case GL_FOG_COLOR:
      for (int i = 0; i < 4; i++) {
        context->fog_color[i] = Clamp01(params[i]);
      }
      break;
    default:
      Unimplemented(&reported, "Fog with GL_FOG_INDEX");
  }
}

static void APIENTRY Fogf(GLenum pname, GLfloat param) {
  if (pname != GL_FOG_COLOR) {
    SetFog(pname, &param);
  }
}

static void APIENTRY Fogi(GLenum pname, GLint param) {
  if (pname != GL_FOG_COLOR) {
    SetFog(pname, (GLfloat[1]){(GLfloat)param});
  }
}

static void APIENTRY Fogfv(GLenum pname, const GLfloat* params) {
  SetFog(pname, params);
}

static void APIENTRY Scissor(GLint x, GLint y, GLsizei width, GLsizei height) {
  if (width >= 0 && height >= 0) {
    current->scissor[0] = x;
    current->scissor[1] = y;
    current->scissor[2] = width;
    current->scissor[3] = height;
  }
}

static void APIENTRY AlphaFunc(GLenum func, GLclampf ref) {
  if (func >= GL_NEVER && func <= GL_ALWAYS) {
    current->alpha_func = func;
    current->alpha_ref = Clamp01(ref);
  }
}

static void APIENTRY StencilFunc(GLenum func, GLint ref, GLuint mask) {
  if (func >= GL_NEVER && func <= GL_ALWAYS) {
    current->stencil_func = func;
    current->stencil_ref = ClampInt(ref, 0, kStencilMax);
    current->stencil_value_mask = mask;
  }
}

static bool IsStencilOp(GLenum op) {
  switch (op) {
    case GL_KEEP:
    case GL_ZERO:
    case GL_REPLACE:
    case GL_INCR:
    case GL_DECR:
    case GL_INVERT:
      return true;
    default:
      return false;
  }
}

static void APIENTRY StencilOp(GLenum fail, GLenum zfail, GLenum zpass) {
  if (IsStencilOp(fail) && IsStencilOp(zfail) && IsStencilOp(zpass)) {
    current->stencil_fail = fail;
    current->stencil_zfail = zfail;
    current->stencil_zpass = zpass;
  }
}

static void APIENTRY StencilMask(GLuint mask) {
  current->stencil_writemask = mask;
}

static void APIENTRY DepthFunc(GLenum func) {
  if (func >= GL_NEVER && func <= GL_ALWAYS) {
    current->depth_func = func;
  }
}

static void APIENTRY DepthMask(GLboolean flag) {
  current->depth_mask = flag != GL_FALSE;
}

static bool IsBlendFactor(GLenum factor) {
  switch (factor) {
    case GL_ZERO:
    case GL_ONE:
    case GL_SRC_COLOR:
    case GL_ONE_MINUS_SRC_COLOR:
    case GL_DST_COLOR:
    case GL_ONE_MINUS_DST_COLOR:
    case GL_SRC_ALPHA:
    case GL_ONE_MINUS_SRC_ALPHA:
    case GL_DST_ALPHA:
    case GL_ONE_MINUS_DST_ALPHA:
    case GL_SRC_ALPHA_SATURATE:
      return true;
    default:
      return false;
  }
}

static void APIENTRY BlendFunc(GLenum sfactor, GLenum dfactor) {
  if (IsBlendFactor(sfactor) && IsBlendFactor(dfactor)) {
    current->blend_src = sfactor;
    current->blend_dst = dfactor;
  }
}

static void APIENTRY ClearColor(GLclampf red, GLclampf green, GLclampf blue,
                                GLclampf alpha) {
  current->clear_color = PackColor((float[4]){red, green, blue, alpha});
}

static void APIENTRY ClearStencil(GLint s) {
  current->clear_stencil = (GLuint)s & kStencilMax;
}

static void APIENTRY Clear(GLbitfield mask) {
  Context* context = current;
  int x0 = 0;
  int y0 = 0;
  int x1 = context->width;
  int y1 = context->height;
  if (context->scissor_test) {
    x0 = MaxInt(x0, context->scissor[0]);
    y0 = MaxInt(y0, context->scissor[1]);
    x1 = MinInt(x1, context->scissor[0] + context->scissor[2]);
    y1 = MinInt(y1, context->scissor[1] + context->scissor[3]);
  }
  bool depth = (mask & GL_DEPTH_BUFFER_BIT) && context->depth_mask;
  bool stencil = mask & GL_STENCIL_BUFFER_BIT;
  for (int y = y0; y < y1; y++) {
    size_t row = (size_t)y * (size_t)context->width;
    for (int x = x0; x < x1; x++) {
      if (mask & GL_COLOR_BUFFER_BIT) {
        context->pixels[row + x] = context->clear_color;
      }
      if (depth) {
        context->depth[row + x] = 1.0f;
      }
      if (stencil) {
        WriteStencil(context, &context->stencil[row + x],
                     context->clear_stencil);
      }
    }
  }
}

static void APIENTRY ReadPixels(GLint x, GLint y, GLsizei width, GLsizei height,
                                GLenum format, GLenum type, GLvoid* pixels) {
  static atomic_flag reported = ATOMIC_FLAG_INIT;
  int components = format == GL_RGBA ? 4 : (format == GL_RGB ? 3 : 0);
  if (!components || type != GL_UNSIGNED_BYTE) {
    Unimplemented(&reported, "ReadPixels other than RGB or RGBA bytes");
    return;
  }
  Context* context = current;
  size_t row = RowSize(width, components, context->pack_alignment);
  for (int j = 0; j < height; j++) {
    int source_y = y + j;
    for (int i = 0; i < width; i++) {
      int source_x = x + i;
      if (source_x < 0 || source_y < 0 || source_x >= context->width ||
          source_y >= context->height) {
        continue;
      }
      uint8_t rgba[4];
      UnpackColor(context->pixels[(size_t)source_y * (size_t)context->width +
                                  (size_t)source_x],
                  rgba);
      memcpy((GLubyte*)pixels + (size_t)j * row + (size_t)(i * components),
             rgba, (size_t)components);
    }
  }
}

static void APIENTRY Finish(void) {}

static void APIENTRY Flush(void) {}

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

static void APIENTRY GetIntegerv(GLenum pname, GLint* params) {
  static atomic_flag reported = ATOMIC_FLAG_INIT;
  switch (pname) {
    case GL_MAX_TEXTURE_SIZE:
      *params = kMaxTextureSize;
      break;
    case GL_RED_BITS:
    case GL_GREEN_BITS:
    case GL_BLUE_BITS:
    case GL_ALPHA_BITS:
      *params = kChannelBits;
      break;
    case GL_DEPTH_BITS:
      *params = kDepthBits;
      break;
    case GL_STENCIL_BITS:
      *params = kStencilBits;
      break;
    default:
      Unimplemented(&reported, "GetIntegerv with this parameter");
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
            [kGlAlphaFunc] = (PROC)AlphaFunc,
            [kGlBegin] = (PROC)Begin,
            [kGlBindTexture] = (PROC)BindTexture,
            [kGlBlendFunc] = (PROC)BlendFunc,
            [kGlClear] = (PROC)Clear,
            [kGlClearColor] = (PROC)ClearColor,
            [kGlClearStencil] = (PROC)ClearStencil,
            [kGlColor3f] = (PROC)Color3f,
            [kGlColor3fv] = (PROC)Color3fv,
            [kGlColor4f] = (PROC)Color4f,
            [kGlColor4fv] = (PROC)Color4fv,
            [kGlColor4ubv] = (PROC)Color4ubv,
            [kGlCopyTexSubImage2D] = (PROC)CopyTexSubImage2D,
            [kGlCullFace] = (PROC)CullFace,
            [kGlDeleteTextures] = (PROC)DeleteTextures,
            [kGlDepthFunc] = (PROC)DepthFunc,
            [kGlDepthMask] = (PROC)DepthMask,
            [kGlDepthRange] = (PROC)DepthRange,
            [kGlDisable] = (PROC)Disable,
            [kGlEnable] = (PROC)Enable,
            [kGlEnd] = (PROC)End,
            [kGlFinish] = (PROC)Finish,
            [kGlFlush] = (PROC)Flush,
            [kGlFogf] = (PROC)Fogf,
            [kGlFogfv] = (PROC)Fogfv,
            [kGlFogi] = (PROC)Fogi,
            [kGlFrontFace] = (PROC)FrontFace,
            [kGlFrustum] = (PROC)Frustum,
            [kGlGenTextures] = (PROC)GenTextures,
            [kGlGetIntegerv] = (PROC)GetIntegerv,
            [kGlGetString] = (PROC)GetString,
            [kGlHint] = (PROC)Hint,
            [kGlLoadIdentity] = (PROC)LoadIdentity,
            [kGlMatrixMode] = (PROC)MatrixMode,
            [kGlOrtho] = (PROC)Ortho,
            [kGlPixelStorei] = (PROC)PixelStorei,
            [kGlPolygonMode] = (PROC)PolygonMode,
            [kGlPolygonOffset] = (PROC)PolygonOffset,
            [kGlPopMatrix] = (PROC)PopMatrix,
            [kGlPushMatrix] = (PROC)PushMatrix,
            [kGlReadPixels] = (PROC)ReadPixels,
            [kGlRotatef] = (PROC)Rotatef,
            [kGlScalef] = (PROC)Scalef,
            [kGlScissor] = (PROC)Scissor,
            [kGlShadeModel] = (PROC)ShadeModel,
            [kGlStencilFunc] = (PROC)StencilFunc,
            [kGlStencilMask] = (PROC)StencilMask,
            [kGlStencilOp] = (PROC)StencilOp,
            [kGlTexCoord2f] = (PROC)TexCoord2f,
            [kGlTexEnvf] = (PROC)TexEnvf,
            [kGlTexEnvi] = (PROC)TexEnvi,
            [kGlTexImage2D] = (PROC)TexImage2D,
            [kGlTexParameterf] = (PROC)TexParameterf,
            [kGlTexParameteri] = (PROC)TexParameteri,
            [kGlTexSubImage2D] = (PROC)TexSubImage2D,
            [kGlTranslatef] = (PROC)Translatef,
            [kGlVertex2f] = (PROC)Vertex2f,
            [kGlVertex3f] = (PROC)Vertex3f,
            [kGlVertex3fv] = (PROC)Vertex3fv,
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
    free(context->depth);
    free(context->stencil);
    context->width = rect.right;
    context->height = rect.bottom;
    size_t count = (size_t)context->width * (size_t)context->height;
    context->pixels = count ? Allocate(count * sizeof(uint32_t)) : NULL;
    context->depth = count ? Allocate(count * sizeof(float)) : NULL;
    context->stencil = count ? Allocate(count) : NULL;
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
    for (int i = 0; i < kMaxTextures; i++) {
      textures[i] = kDefaultTexture;
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
      .cRedBits = kChannelBits,
      .cRedShift = 16,
      .cGreenBits = kChannelBits,
      .cGreenShift = 8,
      .cBlueBits = kChannelBits,
      .cAlphaBits = kChannelBits,
      .cAlphaShift = 24,
      .cDepthBits = kDepthBits,
      .cStencilBits = kStencilBits,
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
      Context* context = &contexts[i];
      *context = (Context){
          .depth_range = {0.0f, 1.0f},
          .color = {1.0f, 1.0f, 1.0f, 1.0f},
          .matrix_mode = GL_MODELVIEW,
          .shade_model = GL_SMOOTH,
          .cull_face_mode = GL_BACK,
          .front_face = GL_CCW,
          .depth_mask = true,
          .depth_func = GL_LESS,
          .stencil_func = GL_ALWAYS,
          .stencil_value_mask = ~0u,
          .stencil_fail = GL_KEEP,
          .stencil_zfail = GL_KEEP,
          .stencil_zpass = GL_KEEP,
          .stencil_writemask = ~0u,
          .alpha_func = GL_ALWAYS,
          .blend_src = GL_ONE,
          .blend_dst = GL_ZERO,
          .fog_mode = GL_EXP,
          .fog_density = 1.0f,
          .fog_end = 1.0f,
          .texture_env_mode = GL_MODULATE,
          .unpack_alignment = 4,
          .pack_alignment = 4,
      };
      memcpy(context->modelview[0], kIdentity, sizeof(kIdentity));
      memcpy(context->projection[0], kIdentity, sizeof(kIdentity));
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
  free(context->depth);
  free(context->stencil);
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
    context->scissor[2] = context->width;
    context->scissor[3] = context->height;
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
