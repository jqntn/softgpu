#include "icd.h"

#include <dwmapi.h>
#include <math.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER
#define SOFTGPU_INLINE __forceinline
#else
#define SOFTGPU_INLINE inline __attribute__((always_inline))
#endif

_Static_assert(kGlEntryCount == 336, "GL 1.1 has 336 dispatch entries");

enum {
  kMaxContexts = 64,
  kMaxFramebuffers = 64,
  kPixelFormatCount = 4,
  kSingleBufferedFormat = 3,
  kChannelBits = 8,
  kDepthBits = 24,
  kStencilBits = 8,
  kAccumBits = 16,
  kStencilMax = (1 << kStencilBits) - 1,
  kAccumMax = (1 << (kAccumBits - 1)) - 1,
  kSubpixelBits = 4,
  kSubpixels = 1 << kSubpixelBits,
  kGuardBand = 1 << 19,
  kMaxViewport = 16384,
  kModelviewDepth = 32,
  kProjectionDepth = 2,
  kTextureDepth = 2,
  kMaxLevels = 12,
  kMaxTextureSize = 1 << (kMaxLevels - 1),
  kTextureUnits = 2,
  kMaxLights = 8,
  kMaxClipPlanes = 6,
  kClipPlanes = 6 + kMaxClipPlanes,
  kMaxEvalOrder = 8,
  kMaps = 9,
  kMaxPixelMap = 256,
  kPixelMaps = 10,
  kMaxNameStack = 64,
  kMaxListNesting = 64,
  kMaxAttribDepth = 16,
  kMaxPointSize = 64,
  kStippleSize = 32,
  kSamples = 4,
  kMaxArgs = 9,
  kMaxSwapInterval = 8
};

enum {
  kRed,
  kGreen,
  kBlue,
  kAlpha,
  kFog,
  kTexcoords,
  kVaryings = kTexcoords + 3 * kTextureUnits
};

enum { kS, kT, kQ };

enum { kFront, kBack };

enum { kTexture1D, kTexture2D };

enum {
  kModelview,
  kProjection,
  kTextureMatrix,
  kMatrixStacks = kTextureMatrix + kTextureUnits
};

enum {
  kVertexArray,
  kNormalArray,
  kColorArray,
  kIndexArray,
  kEdgeFlagArray,
  kTexCoordArray,
  kArrays = kTexCoordArray + kTextureUnits
};

typedef enum Op {
  kOpBegin,
  kOpEnd,
  kOpArrays,
  kOpVertex,
  kOpColor,
  kOpIndex,
  kOpNormal,
  kOpTexCoord,
  kOpEdgeFlag,
  kOpRect,
  kOpRasterPos,
  kOpMaterial,
  kOpLight,
  kOpLightModel,
  kOpColorMaterial,
  kOpShadeModel,
  kOpFog,
  kOpClipPlane,
  kOpMatrixMode,
  kOpLoadIdentity,
  kOpLoadMatrix,
  kOpMultMatrix,
  kOpRotate,
  kOpScale,
  kOpTranslate,
  kOpFrustum,
  kOpOrtho,
  kOpPushMatrix,
  kOpPopMatrix,
  kOpViewport,
  kOpDepthRange,
  kOpEnable,
  kOpDisable,
  kOpCullFace,
  kOpFrontFace,
  kOpPolygonMode,
  kOpPolygonOffset,
  kOpPolygonStipple,
  kOpLineWidth,
  kOpLineStipple,
  kOpPointSize,
  kOpHint,
  kOpTexParameter,
  kOpTexEnv,
  kOpTexGen,
  kOpTexImage1D,
  kOpTexImage2D,
  kOpTexSubImage1D,
  kOpTexSubImage2D,
  kOpCopyTexImage1D,
  kOpCopyTexImage2D,
  kOpCopyTexSubImage1D,
  kOpCopyTexSubImage2D,
  kOpBindTexture,
  kOpPrioritizeTextures,
  kOpActiveTexture,
  kOpScissor,
  kOpAlphaFunc,
  kOpStencilFunc,
  kOpStencilOp,
  kOpStencilMask,
  kOpDepthFunc,
  kOpDepthMask,
  kOpBlendFunc,
  kOpLogicOp,
  kOpColorMask,
  kOpIndexMask,
  kOpDrawBuffer,
  kOpReadBuffer,
  kOpClearColor,
  kOpClearIndex,
  kOpClearDepth,
  kOpClearStencil,
  kOpClearAccum,
  kOpClear,
  kOpAccum,
  kOpPixelZoom,
  kOpPixelTransfer,
  kOpPixelMap,
  kOpBitmap,
  kOpDrawPixels,
  kOpCopyPixels,
  kOpMap1,
  kOpMap2,
  kOpMapGrid1,
  kOpMapGrid2,
  kOpEvalCoord1,
  kOpEvalCoord2,
  kOpEvalPoint1,
  kOpEvalPoint2,
  kOpEvalMesh1,
  kOpEvalMesh2,
  kOpInitNames,
  kOpLoadName,
  kOpPushName,
  kOpPopName,
  kOpPassThrough,
  kOpCallList,
  kOpCallLists,
  kOpListBase,
  kOpPushAttrib,
  kOpPopAttrib
} Op;

static const float kIdentity[16] = {1, 0, 0, 0, 0, 1, 0, 0,
                                    0, 0, 1, 0, 0, 0, 0, 1};
static const float kRadiansPerDegree = 0.0174532925f;
static const float kDepthUnit = 1.0f / 8388608.0f;
static const float kIntegralFloat = 8388608.0f;
static const float kPointGranularity = 0.125f;
static const double kUintMax = 4294967295.0;
static const char kWglExtensions[] =
    "WGL_ARB_extensions_string WGL_EXT_extensions_string WGL_EXT_swap_control";
static const int kMapSizes[kMaps] = {4, 1, 3, 1, 2, 3, 4, 3, 4};
static const GLenum kCapabilities[] = {
    GL_ALPHA_TEST,
    GL_AUTO_NORMAL,
    GL_BLEND,
    GL_CLIP_PLANE0,
    GL_CLIP_PLANE1,
    GL_CLIP_PLANE2,
    GL_CLIP_PLANE3,
    GL_CLIP_PLANE4,
    GL_CLIP_PLANE5,
    GL_COLOR_LOGIC_OP,
    GL_COLOR_MATERIAL,
    GL_CULL_FACE,
    GL_DEPTH_TEST,
    GL_DITHER,
    GL_FOG,
    GL_INDEX_LOGIC_OP,
    GL_LIGHT0,
    GL_LIGHT1,
    GL_LIGHT2,
    GL_LIGHT3,
    GL_LIGHT4,
    GL_LIGHT5,
    GL_LIGHT6,
    GL_LIGHT7,
    GL_LIGHTING,
    GL_LINE_SMOOTH,
    GL_LINE_STIPPLE,
    GL_MAP1_COLOR_4,
    GL_MAP1_INDEX,
    GL_MAP1_NORMAL,
    GL_MAP1_TEXTURE_COORD_1,
    GL_MAP1_TEXTURE_COORD_2,
    GL_MAP1_TEXTURE_COORD_3,
    GL_MAP1_TEXTURE_COORD_4,
    GL_MAP1_VERTEX_3,
    GL_MAP1_VERTEX_4,
    GL_MAP2_COLOR_4,
    GL_MAP2_INDEX,
    GL_MAP2_NORMAL,
    GL_MAP2_TEXTURE_COORD_1,
    GL_MAP2_TEXTURE_COORD_2,
    GL_MAP2_TEXTURE_COORD_3,
    GL_MAP2_TEXTURE_COORD_4,
    GL_MAP2_VERTEX_3,
    GL_MAP2_VERTEX_4,
    GL_NORMALIZE,
    GL_POINT_SMOOTH,
    GL_POLYGON_OFFSET_FILL,
    GL_POLYGON_OFFSET_LINE,
    GL_POLYGON_OFFSET_POINT,
    GL_POLYGON_SMOOTH,
    GL_POLYGON_STIPPLE,
    GL_SCISSOR_TEST,
    GL_STENCIL_TEST,
};

typedef union Arg {
  GLint i;
  GLuint u;
  GLfloat f;
  GLdouble d;
} Arg;

typedef struct Command {
  Op op;
  Arg args[kMaxArgs];
  void* data;
} Command;

typedef struct List {
  Command* commands;
  int count;
  int capacity;
} List;

typedef struct Name {
  GLuint name;
  void* object;
} Name;

typedef struct Names {
  Name* entries;
  int count;
  int capacity;
} Names;

typedef struct Level {
  int width;
  int height;
  int border;
  GLint internal_format;
  GLenum format;
  uint8_t (*texels)[4];
} Level;

typedef struct TextureParameters {
  GLenum min_filter;
  GLenum mag_filter;
  GLenum wrap_s;
  GLenum wrap_t;
  float border_color[4];
  float priority;
} TextureParameters;

typedef struct Texture {
  GLuint name;
  GLenum target;
  TextureParameters parameters;
  Level levels[kMaxLevels];
} Texture;

typedef struct Framebuffer {
  HWND window;
  int width;
  int height;
  uint32_t* front;
  uint32_t* back;
  float* depth;
  uint8_t* stencil;
  int16_t* accum;
  bool front_dirty;
  bool single_buffered;
  bool accumulation;
  int swap_interval;
} Framebuffer;

typedef struct Shared {
  Names textures;
  Names lists;
  int references;
} Shared;

typedef struct Vertex {
  float clip[4];
  float eye[4];
  float color[2][4];
  float texcoord[kTextureUnits][4];
  float fog;
  bool edge;
} Vertex;

typedef struct Corner {
  float x;
  float y;
  float z;
  float q;
  float varying[kVaryings];
} Corner;

typedef struct RasterPosition {
  float window[4];
  float distance;
  bool valid;
  float color[4];
  float index;
  float texcoord[kTextureUnits][4];
} RasterPosition;

typedef struct CurrentState {
  float color[4];
  float index;
  float normal[3];
  float texcoord[kTextureUnits][4];
  bool edge_flag;
  RasterPosition raster;
} CurrentState;

typedef struct PointState {
  float size;
  bool smooth;
} PointState;

typedef struct LineState {
  float width;
  bool smooth;
  bool stipple;
  GLushort pattern;
  GLint repeat;
} LineState;

typedef struct PolygonState {
  bool cull;
  GLenum cull_mode;
  GLenum front_face;
  GLenum mode[2];
  bool smooth;
  bool stipple;
  bool offset_point;
  bool offset_line;
  bool offset_fill;
  float offset_factor;
  float offset_units;
} PolygonState;

typedef struct PixelState {
  bool map_color;
  bool map_stencil;
  GLint index_shift;
  GLint index_offset;
  float scale[4];
  float bias[4];
  float depth_scale;
  float depth_bias;
  float zoom[2];
  GLenum read_buffer;
} PixelState;

typedef struct Material {
  float ambient[4];
  float diffuse[4];
  float specular[4];
  float emission[4];
  float shininess;
  float indexes[3];
} Material;

typedef struct Light {
  bool enabled;
  float ambient[4];
  float diffuse[4];
  float specular[4];
  float position[4];
  float direction[3];
  float exponent;
  float cutoff;
  float attenuation[3];
} Light;

typedef struct LightingState {
  bool enabled;
  bool color_material;
  GLenum color_material_face;
  GLenum color_material_mode;
  Material material[2];
  Light lights[kMaxLights];
  float model_ambient[4];
  bool local_viewer;
  bool two_side;
  GLenum shade_model;
} LightingState;

typedef struct FogState {
  bool enabled;
  GLenum mode;
  float density;
  float start;
  float end;
  float index;
  float color[4];
} FogState;

typedef struct DepthState {
  bool test;
  GLenum func;
  float clear;
  bool mask;
} DepthState;

typedef struct StencilState {
  bool test;
  GLenum func;
  GLint ref;
  GLuint value_mask;
  GLenum fail;
  GLenum zfail;
  GLenum zpass;
  GLint clear;
  GLuint writemask;
} StencilState;

typedef struct ViewportState {
  GLint box[4];
  float depth_range[2];
} ViewportState;

typedef struct TransformState {
  GLenum matrix_mode;
  float planes[kMaxClipPlanes][4];
  bool clip[kMaxClipPlanes];
  bool normalize;
} TransformState;

typedef struct ColorState {
  bool alpha_test;
  GLenum alpha_func;
  float alpha_ref;
  bool blend;
  GLenum blend_src;
  GLenum blend_dst;
  bool dither;
  bool index_logic_op;
  bool color_logic_op;
  GLenum logic_op;
  GLenum draw_buffer;
  GLuint index_writemask;
  bool mask[4];
  float clear[4];
  float clear_index;
} ColorState;

typedef struct HintState {
  GLenum perspective;
  GLenum point;
  GLenum line;
  GLenum polygon;
  GLenum fog;
} HintState;

typedef struct EvalState {
  bool map1[kMaps];
  bool map2[kMaps];
  bool auto_normal;
  GLint segments1;
  float domain1[2];
  GLint segments2[2];
  float domain2[4];
} EvalState;

typedef struct TexGen {
  bool enabled;
  GLenum mode;
  float object[4];
  float eye[4];
} TexGen;

typedef struct Combiner {
  GLenum function;
  GLenum source[3];
  GLenum operand[3];
  float scale;
} Combiner;

typedef struct TextureUnit {
  bool enabled[2];
  GLuint binding[2];
  GLenum env_mode;
  float env_color[4];
  Combiner rgb;
  Combiner alpha;
  TexGen gen[4];
} TextureUnit;

typedef struct TextureState {
  int active;
  TextureUnit units[kTextureUnits];
} TextureState;

typedef struct ScissorState {
  bool test;
  GLint box[4];
} ScissorState;

typedef struct State {
  CurrentState current;
  PointState point;
  LineState line;
  PolygonState polygon;
  uint32_t stipple[kStippleSize];
  PixelState pixel;
  LightingState lighting;
  FogState fog;
  DepthState depth;
  float accum_clear[4];
  StencilState stencil;
  ViewportState viewport;
  TransformState transform;
  ColorState color;
  HintState hint;
  EvalState eval;
  GLuint list_base;
  TextureState texture;
  ScissorState scissor;
} State;

typedef struct PixelStore {
  bool swap_bytes;
  bool lsb_first;
  GLint row_length;
  GLint skip_rows;
  GLint skip_pixels;
  GLint alignment;
} PixelStore;

typedef struct Array {
  bool enabled;
  GLint size;
  GLenum type;
  GLsizei stride;
  const GLvoid* pointer;
} Array;

typedef struct ClientState {
  PixelStore pack;
  PixelStore unpack;
  Array arrays[kArrays];
  int active_texture;
} ClientState;

typedef struct AttribEntry {
  GLbitfield mask;
  State state;
  TextureParameters textures[kTextureUnits][2];
} AttribEntry;

typedef struct ClientEntry {
  GLbitfield mask;
  ClientState state;
} ClientEntry;

typedef struct Map {
  int order[2];
  float domain[4];
  float points[kMaxEvalOrder * kMaxEvalOrder * 4];
} Map;

typedef struct PixelMap {
  int size;
  float values[kMaxPixelMap];
} PixelMap;

typedef struct Selection {
  GLuint* buffer;
  GLsizei size;
  int count;
  int hits;
  bool overflow;
  bool hit;
  float min_depth;
  float max_depth;
  GLuint names[kMaxNameStack];
  int depth;
} Selection;

typedef struct Feedback {
  GLfloat* buffer;
  GLsizei size;
  GLenum type;
  int count;
  bool overflow;
} Feedback;

typedef struct Context {
  State state;
  ClientState client;
  GLenum error;
  float matrices[kMatrixStacks][kModelviewDepth][16];
  int matrix_top[kMatrixStacks];
  float normal_matrix[9];
  bool normal_matrix_stale;
  Texture default_textures[2];
  Texture* bound[kTextureUnits][2];
  Level proxies[2][kMaxLevels];
  PixelMap pixel_maps[kPixelMaps];
  Map maps1[kMaps];
  Map maps2[kMaps];
  bool inside;
  GLenum mode;
  int vertex_count;
  Vertex first;
  Vertex recent[4];
  Vertex* clip_buffer;
  Corner* corners;
  int clip_capacity;
  int stipple_counter;
  GLenum render_mode;
  Selection selection;
  Feedback feedback;
  List* list;
  GLuint list_name;
  GLenum list_mode;
  int list_depth;
  AttribEntry attrib_stack[kMaxAttribDepth];
  int attrib_depth;
  ClientEntry client_stack[kMaxAttribDepth];
  int client_depth;
  Shared* shared;
  HDC hdc;
  Framebuffer* framebuffer;
  bool attached;
} Context;

typedef struct Sampler {
  const Texture* texture;
  int levels;
  bool one_dimensional;
  GLenum format;
  uint8_t border[4];
} Sampler;

typedef struct Targets {
  uint32_t* buffers[2];
  int count;
  uint32_t mask;
} Targets;

typedef struct Primitive {
  Sampler samplers[kTextureUnits];
  int varyings;
  Targets targets;
  int rect[4];
  float offset;
  bool stipple;
} Primitive;

typedef struct Setup {
  const Primitive* primitive;
  const Corner* corners[3];
  bool projective[kTextureUnits];
  float derivative[kTextureUnits][2][3];
} Setup;

typedef struct TextureLookup {
  float s;
  float t;
  float lambda;
} TextureLookup;

static Context contexts[kMaxContexts];
static atomic_int used[kMaxContexts];
static Framebuffer framebuffers[kMaxFramebuffers];
static atomic_flag framebuffers_lock = ATOMIC_FLAG_INIT;
static _Thread_local Context* current;
static const PixelStore kPackedStore = {.alignment = 1};
static const TextureParameters kDefaultParameters = {
    .min_filter = GL_NEAREST_MIPMAP_LINEAR,
    .mag_filter = GL_LINEAR,
    .wrap_s = GL_REPEAT,
    .wrap_t = GL_REPEAT,
    .priority = 1.0f,
};

static void Fail(const char* message) {
  fprintf(stderr, "softgpu: %s\n", message);
  exit(EXIT_FAILURE);
}

static void* Allocate(size_t size) {
  void* memory = malloc(size ? size : 1);
  if (!memory) {
    Fail("out of memory");
  }
  return memory;
}

static void* Reallocate(void* memory, size_t size) {
  void* resized = realloc(memory, size ? size : 1);
  if (!resized) {
    Fail("out of memory");
  }
  return resized;
}

static float MinFloat(float a, float b) { return a < b ? a : b; }

static float MaxFloat(float a, float b) { return a > b ? a : b; }

static float Clamp01(float value) {
  return MinFloat(MaxFloat(value, 0.0f), 1.0f);
}

static float ClampFloat(float value, float low, float high) {
  return MinFloat(MaxFloat(value, low), high);
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

static bool IsPowerOfTwo(int value) {
  return value > 0 && (value & (value - 1)) == 0;
}

static bool IsTextureUnit(GLenum texture) {
  return texture >= GL_TEXTURE0_ARB &&
         texture < GL_TEXTURE0_ARB + kTextureUnits;
}

static int TexcoordVarying(int unit) { return kTexcoords + 3 * unit; }

static float Dot3(const float a[3], const float b[3]) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static float Dot4(const float a[4], const float b[4]) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
}

static void Normalize3(float v[3]) {
  float length = sqrtf(Dot3(v, v));
  if (length > 0.0f) {
    for (int i = 0; i < 3; i++) {
      v[i] /= length;
    }
  }
}

static float FromUbyte(GLubyte c) { return (float)c / 255.0f; }

static float FromByte(GLbyte c) { return (2.0f * (float)c + 1.0f) / 255.0f; }

static float FromUshort(GLushort c) { return (float)c / 65535.0f; }

static float FromShort(GLshort c) {
  return (2.0f * (float)c + 1.0f) / 65535.0f;
}

static float FromUint(GLuint c) { return (float)((double)c / kUintMax); }

static float FromInt(GLint c) {
  return (float)((2.0 * (double)c + 1.0) / kUintMax);
}

static GLint ToInt(double value) {
  return (GLint)fmax(fmin(round(value), 2147483647.0), -2147483648.0);
}

static GLint ColorToInt(double value) {
  return (GLint)fmax(fmin((kUintMax * value - 1.0) / 2.0, 2147483647.0),
                     -2147483648.0);
}

static uint32_t PackChannel(GLclampf value) {
  return (uint32_t)(int32_t)(Clamp01(value) * 255.0f + 0.5f);
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

static void PixelColor(uint32_t pixel, float color[4]) {
  uint8_t rgba[4];
  UnpackColor(pixel, rgba);
  for (int i = 0; i < 4; i++) {
    color[i] = (float)rgba[i] / 255.0f;
  }
}

static bool IsCompare(GLenum func) {
  return func >= GL_NEVER && func <= GL_ALWAYS;
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

static void SetError(Context* context, GLenum error) {
  if (context->error == GL_NO_ERROR) {
    context->error = error;
  }
}

static bool InsideBeginEnd(Context* context) {
  if (context->inside) {
    SetError(context, GL_INVALID_OPERATION);
  }
  return context->inside;
}

static TextureUnit* ActiveUnit(Context* context) {
  return &context->state.texture.units[context->state.texture.active];
}

static Texture* BoundTexture(const Context* context, int target) {
  return context->bound[context->state.texture.active][target];
}

static int NamePosition(const Names* names, GLuint name) {
  int low = 0;
  int high = names->count;
  while (low < high) {
    int middle = (low + high) / 2;
    if (names->entries[middle].name < name) {
      low = middle + 1;
    } else {
      high = middle;
    }
  }
  return low;
}

static Name* FindName(const Names* names, GLuint name) {
  int position = NamePosition(names, name);
  return position < names->count && names->entries[position].name == name
             ? &names->entries[position]
             : NULL;
}

static void AddName(Names* names, GLuint name, void* object) {
  int position = NamePosition(names, name);
  if (position < names->count && names->entries[position].name == name) {
    names->entries[position].object = object;
    return;
  }
  if (names->count == names->capacity) {
    names->capacity = MaxInt(16, names->capacity * 2);
    names->entries =
        Reallocate(names->entries, (size_t)names->capacity * sizeof(Name));
  }
  memmove(&names->entries[position + 1], &names->entries[position],
          (size_t)(names->count - position) * sizeof(Name));
  names->entries[position] = (Name){name, object};
  names->count++;
}

static void RemoveName(Names* names, GLuint name) {
  int position = NamePosition(names, name);
  if (position < names->count && names->entries[position].name == name) {
    memmove(&names->entries[position], &names->entries[position + 1],
            (size_t)(names->count - position - 1) * sizeof(Name));
    names->count--;
  }
}

static GLuint UnusedNames(const Names* names, GLuint count) {
  GLuint candidate = 1;
  for (int i = 0; i < names->count; i++) {
    GLuint name = names->entries[i].name;
    if (name - candidate >= count) {
      return candidate;
    }
    if (name == UINT32_MAX) {
      return 0;
    }
    candidate = name + 1;
  }
  return UINT32_MAX - candidate + 1 >= count ? candidate : 0;
}

static void FreeList(List* list) {
  for (int i = 0; i < list->count; i++) {
    free(list->commands[i].data);
  }
  free(list->commands);
  free(list);
}

static bool Compiling(const Context* context) {
  return context->list && context->list_depth == 0;
}

static bool RecordData(Context* context, Op op, const Arg* args, int count,
                       void* data) {
  if (!Compiling(context)) {
    free(data);
    return false;
  }
  List* list = context->list;
  if (list->count == list->capacity) {
    list->capacity = MaxInt(64, list->capacity * 2);
    list->commands =
        Reallocate(list->commands, (size_t)list->capacity * sizeof(Command));
  }
  Command* command = &list->commands[list->count++];
  *command = (Command){.op = op, .data = data};
  if (count > 0) {
    memcpy(command->args, args, (size_t)count * sizeof(Arg));
  }
  return context->list_mode == GL_COMPILE;
}

static bool Record(Context* context, Op op, const Arg* args, int count) {
  return RecordData(context, op, args, count, NULL);
}

static void* Duplicate(const void* source, size_t size) {
  void* copy = Allocate(size);
  memcpy(copy, source, size);
  return copy;
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

static int LevelWidth(const Level* level) {
  return level->width - 2 * level->border;
}

static int LevelHeight(const Level* level, bool one_dimensional) {
  return one_dimensional ? level->height : level->height - 2 * level->border;
}

static int CompleteLevels(const Texture* texture, bool one_dimensional) {
  const Level* base = texture->levels;
  if (!base->texels) {
    return 0;
  }
  GLenum min_filter = texture->parameters.min_filter;
  if (min_filter == GL_NEAREST || min_filter == GL_LINEAR) {
    return 1;
  }
  int width = LevelWidth(base);
  int height = LevelHeight(base, one_dimensional);
  int count = 1;
  while ((width >> (count - 1)) > 1 || (height >> (count - 1)) > 1) {
    const Level* level = &texture->levels[count];
    int border = base->border;
    int expected_height =
        one_dimensional ? 1 : MaxInt(height >> count, 1) + 2 * border;
    if (!level->texels || level->border != border ||
        level->internal_format != base->internal_format ||
        level->width != MaxInt(width >> count, 1) + 2 * border ||
        level->height != expected_height) {
      return 0;
    }
    count++;
  }
  return count;
}

static int TexelIndex(int i, int size, int border, GLenum wrap) {
  if (wrap == GL_REPEAT) {
    return (i & (size - 1)) + border;
  }
  return i < -border || i >= size + border ? -1 : i + border;
}

static const uint8_t* Texel(const Sampler* sampler, const Level* level, int x,
                            int y) {
  return x < 0 || y < 0
             ? sampler->border
             : level->texels[(size_t)y * (size_t)level->width + (size_t)x];
}

static SOFTGPU_INLINE void SampleLevel(const Sampler* sampler,
                                       const Level* level, GLenum filter,
                                       float s, float t, float texel[4]) {
  const TextureParameters* parameters = &sampler->texture->parameters;
  int width = LevelWidth(level);
  int height = LevelHeight(level, sampler->one_dimensional);
  int border_y = sampler->one_dimensional ? 0 : level->border;
  GLenum wrap_t = sampler->one_dimensional ? GL_REPEAT : parameters->wrap_t;
  s = parameters->wrap_s == GL_REPEAT ? s - Floor(s) : Clamp01(s);
  if (sampler->one_dimensional) {
    t = 0.5f;
  } else {
    t = wrap_t == GL_REPEAT ? t - Floor(t) : Clamp01(t);
  }
  float u = s * (float)width;
  float v = t * (float)height;
  if (filter == GL_NEAREST) {
    const uint8_t* p =
        Texel(sampler, level,
              TexelIndex(MinInt((int)u, width - 1), width, level->border,
                         parameters->wrap_s),
              TexelIndex(MinInt((int)v, height - 1), height, border_y, wrap_t));
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
  int x0 = TexelIndex((int)left, width, level->border, parameters->wrap_s);
  int x1 = TexelIndex((int)left + 1, width, level->border, parameters->wrap_s);
  int y0 = TexelIndex((int)bottom, height, border_y, wrap_t);
  int y1 = TexelIndex((int)bottom + 1, height, border_y, wrap_t);
  const uint8_t* p00 = Texel(sampler, level, x0, y0);
  const uint8_t* p10 = Texel(sampler, level, x1, y0);
  const uint8_t* p01 = Texel(sampler, level, x0, y1);
  const uint8_t* p11 = Texel(sampler, level, x1, y1);
  for (int i = 0; i < 4; i++) {
    float lower = (float)p00[i] + ((float)p10[i] - (float)p00[i]) * a;
    float upper = (float)p01[i] + ((float)p11[i] - (float)p01[i]) * a;
    texel[i] = (lower + (upper - lower) * b) / 255.0f;
  }
}

static SOFTGPU_INLINE void SampleTexture(const Sampler* sampler, float s,
                                         float t, float lambda,
                                         float texel[4]) {
  const Texture* texture = sampler->texture;
  GLenum minify = texture->parameters.min_filter;
  GLenum magnify = texture->parameters.mag_filter;
  bool nearest =
      minify == GL_NEAREST_MIPMAP_NEAREST || minify == GL_NEAREST_MIPMAP_LINEAR;
  float c = magnify == GL_LINEAR && nearest ? 0.5f : 0.0f;
  if (lambda <= c) {
    SampleLevel(sampler, texture->levels, magnify, s, t, texel);
    return;
  }
  if (minify == GL_NEAREST || minify == GL_LINEAR) {
    SampleLevel(sampler, texture->levels, minify, s, t, texel);
    return;
  }
  GLenum filter = nearest ? GL_NEAREST : GL_LINEAR;
  float d = MinFloat(lambda, (float)(sampler->levels - 1));
  if (minify == GL_NEAREST_MIPMAP_NEAREST ||
      minify == GL_LINEAR_MIPMAP_NEAREST) {
    int level = d <= 0.5f ? 0 : (int)Ceil(d + 0.5f) - 1;
    SampleLevel(sampler, &texture->levels[level], filter, s, t, texel);
    return;
  }
  int level = (int)d;
  float fraction = d - (float)level;
  SampleLevel(sampler, &texture->levels[level], filter, s, t, texel);
  if (fraction > 0.0f && level + 1 < sampler->levels) {
    float next[4];
    SampleLevel(sampler, &texture->levels[level + 1], filter, s, t, next);
    for (int i = 0; i < 4; i++) {
      texel[i] += (next[i] - texel[i]) * fraction;
    }
  }
}

static float LevelOfDetail(float rho) {
  return rho <= 1.0f ? 0.0f : 0.5f * log2f(rho);
}

static float TextureScale(const Sampler* sampler, int axis) {
  const Level* base = sampler->texture->levels;
  if (axis == 0) {
    return (float)LevelWidth(base);
  }
  return sampler->one_dimensional ? 0.0f : (float)LevelHeight(base, false);
}

static float Lambda(const Setup* setup, int unit, float s, float t,
                    float inverse_q) {
  const Sampler* sampler = &setup->primitive->samplers[unit];
  float width = TextureScale(sampler, 0);
  float height = TextureScale(sampler, 1);
  float rho = 0.0f;
  for (int axis = 0; axis < 2; axis++) {
    const float* d = setup->derivative[unit][axis];
    float du = (d[0] - s * d[2]) * inverse_q * width;
    float dv = (d[1] - t * d[2]) * inverse_q * height;
    rho = MaxFloat(du * du + dv * dv, rho);
  }
  return LevelOfDetail(rho);
}

static float CombineArgument(GLenum source, GLenum operand, int channel,
                             const TextureUnit* unit, const float texel[4],
                             const float primary[4], const float previous[4]) {
  const float* color = source == GL_TEXTURE             ? texel
                       : source == GL_CONSTANT_ARB      ? unit->env_color
                       : source == GL_PRIMARY_COLOR_ARB ? primary
                                                        : previous;
  bool alpha = operand == GL_SRC_ALPHA || operand == GL_ONE_MINUS_SRC_ALPHA;
  float value = color[alpha ? kAlpha : channel];
  bool inverted =
      operand == GL_ONE_MINUS_SRC_COLOR || operand == GL_ONE_MINUS_SRC_ALPHA;
  return inverted ? 1.0f - value : value;
}

static float CombineFunction(GLenum function, const float a[3]) {
  switch (function) {
    case GL_REPLACE:
      return a[0];
    case GL_MODULATE:
      return a[0] * a[1];
    case GL_ADD:
      return a[0] + a[1];
    case GL_ADD_SIGNED_ARB:
      return a[0] + a[1] - 0.5f;
    case GL_INTERPOLATE_ARB:
      return a[0] * a[2] + a[1] * (1.0f - a[2]);
    default:
      return a[0] - a[1];
  }
}

static int CombineArgumentCount(GLenum function) {
  switch (function) {
    case GL_REPLACE:
      return 1;
    case GL_INTERPOLATE_ARB:
      return 3;
    default:
      return 2;
  }
}

static void ApplyCombine(const TextureUnit* unit, const float texel[4],
                         const float primary[4], float color[4]) {
  float previous[4];
  memcpy(previous, color, sizeof(previous));
  for (int channel = 0; channel < 4; channel++) {
    const Combiner* combiner = channel == kAlpha ? &unit->alpha : &unit->rgb;
    float a[3] = {0};
    for (int i = 0; i < CombineArgumentCount(combiner->function); i++) {
      a[i] = CombineArgument(combiner->source[i], combiner->operand[i], channel,
                             unit, texel, primary, previous);
    }
    color[channel] =
        Clamp01(CombineFunction(combiner->function, a) * combiner->scale);
  }
}

static void ApplyTexture(const TextureUnit* state, GLenum format,
                         const float texel[4], const float primary[4],
                         float color[4]) {
  bool has_color = format != GL_ALPHA;
  bool has_alpha = format == GL_ALPHA || format == GL_LUMINANCE_ALPHA ||
                   format == GL_INTENSITY || format == GL_RGBA;
  switch (state->env_mode) {
    case GL_COMBINE_ARB:
      ApplyCombine(state, texel, primary, color);
      break;
    case GL_REPLACE:
      for (int i = 0; i < 3 && has_color; i++) {
        color[i] = texel[i];
      }
      if (has_alpha) {
        color[3] = texel[3];
      }
      break;
    case GL_MODULATE:
      for (int i = 0; i < 3 && has_color; i++) {
        color[i] *= texel[i];
      }
      if (has_alpha) {
        color[3] *= texel[3];
      }
      break;
    case GL_DECAL:
      for (int i = 0; i < 3 && has_color; i++) {
        color[i] = format == GL_RGBA
                       ? color[i] + (texel[i] - color[i]) * texel[3]
                       : texel[i];
      }
      break;
    case GL_ADD:
      for (int i = 0; i < 3 && has_color; i++) {
        color[i] = MinFloat(color[i] + texel[i], 1.0f);
      }
      if (format == GL_INTENSITY) {
        color[3] = MinFloat(color[3] + texel[3], 1.0f);
      } else if (has_alpha) {
        color[3] *= texel[3];
      }
      break;
    default:
      for (int i = 0; i < 3 && has_color; i++) {
        color[i] += (state->env_color[i] - color[i]) * texel[i];
      }
      if (format == GL_INTENSITY) {
        color[3] += (state->env_color[3] - color[3]) * texel[3];
      } else if (has_alpha) {
        color[3] *= texel[3];
      }
  }
}

static void ApplyFog(const Context* context, float eye_z, float color[4]) {
  const FogState* fog = &context->state.fog;
  float distance = fabsf(eye_z);
  float f;
  switch (fog->mode) {
    case GL_LINEAR:
      f = (fog->end - distance) / (fog->end - fog->start);
      break;
    case GL_EXP:
      f = expf(-fog->density * distance);
      break;
    default: {
      float x = fog->density * distance;
      f = expf(-x * x);
    }
  }
  f = Clamp01(f);
  for (int i = 0; i < 3; i++) {
    color[i] = f * color[i] + (1.0f - f) * fog->color[i];
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

static uint32_t Blend(const Context* context, uint32_t pixel,
                      const float color[4]) {
  float destination[4];
  PixelColor(pixel, destination);
  float blended[4];
  for (int i = 0; i < 4; i++) {
    blended[i] = color[i] * Factor(context->state.color.blend_src, color,
                                   destination, i) +
                 destination[i] * Factor(context->state.color.blend_dst, color,
                                         destination, i);
  }
  return PackColor(blended);
}

static uint32_t ApplyLogicOp(GLenum op, uint32_t s, uint32_t d) {
  switch (op) {
    case GL_CLEAR:
      return 0;
    case GL_AND:
      return s & d;
    case GL_AND_REVERSE:
      return s & ~d;
    case GL_AND_INVERTED:
      return ~s & d;
    case GL_NOOP:
      return d;
    case GL_XOR:
      return s ^ d;
    case GL_OR:
      return s | d;
    case GL_NOR:
      return ~(s | d);
    case GL_EQUIV:
      return ~(s ^ d);
    case GL_INVERT:
      return ~d;
    case GL_OR_REVERSE:
      return s | ~d;
    case GL_COPY_INVERTED:
      return ~s;
    case GL_OR_INVERTED:
      return ~s | d;
    case GL_NAND:
      return ~(s & d);
    case GL_SET:
      return UINT32_MAX;
    default:
      return s;
  }
}

static SOFTGPU_INLINE void WriteColor(const Context* context,
                                      const Targets* targets, size_t index,
                                      const float color[4]) {
  const ColorState* state = &context->state.color;
  uint32_t source = PackColor(color);
  for (int i = 0; i < targets->count; i++) {
    uint32_t* pixel = &targets->buffers[i][index];
    uint32_t value = source;
    if (state->color_logic_op) {
      value = ApplyLogicOp(state->logic_op, source, *pixel);
    } else if (state->blend) {
      value = Blend(context, *pixel, color);
    }
    *pixel = targets->mask == UINT32_MAX
                 ? value
                 : (*pixel & ~targets->mask) | (value & targets->mask);
  }
}

static void WriteStencil(const Context* context, uint8_t* stencil,
                         GLuint value) {
  GLuint mask = context->state.stencil.writemask;
  *stencil = (uint8_t)((*stencil & ~mask) | (value & mask));
}

static void ApplyStencilOp(const Context* context, GLenum op,
                           uint8_t* stencil) {
  int value = *stencil;
  switch (op) {
    case GL_ZERO:
      value = 0;
      break;
    case GL_REPLACE:
      value = context->state.stencil.ref;
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

static bool DepthPass(const Context* context, size_t index, float z) {
  return !context->state.depth.test ||
         Compare(context->state.depth.func, z,
                 context->framebuffer->depth[index]);
}

static SOFTGPU_INLINE void WriteFragment(Context* context,
                                         const Primitive* primitive,
                                         size_t index, float z, bool depth_pass,
                                         const float color[4]) {
  const State* state = &context->state;
  if (state->color.alpha_test &&
      !Compare(state->color.alpha_func, color[kAlpha],
               state->color.alpha_ref)) {
    return;
  }
  if (state->stencil.test) {
    uint8_t* stencil = &context->framebuffer->stencil[index];
    GLuint mask = state->stencil.value_mask;
    if (!Compare(state->stencil.func,
                 (float)((GLuint)state->stencil.ref & mask),
                 (float)(*stencil & mask))) {
      ApplyStencilOp(context, state->stencil.fail, stencil);
      return;
    }
    ApplyStencilOp(context,
                   depth_pass ? state->stencil.zpass : state->stencil.zfail,
                   stencil);
  }
  if (!depth_pass) {
    return;
  }
  if (state->depth.test && state->depth.mask) {
    context->framebuffer->depth[index] = z;
  }
  WriteColor(context, &primitive->targets, index, color);
}

static SOFTGPU_INLINE void ShadeColor(
    const Context* context, const Primitive* primitive,
    const float values[kVaryings], const TextureLookup lookups[kTextureUnits],
    float coverage, float color[4]) {
  memcpy(color, values, 4 * sizeof(float));
  for (int unit = 0; unit < kTextureUnits; unit++) {
    const Sampler* sampler = &primitive->samplers[unit];
    if (sampler->texture) {
      const TextureLookup* lookup = &lookups[unit];
      float texel[4];
      SampleTexture(sampler, lookup->s, lookup->t, lookup->lambda, texel);
      ApplyTexture(&context->state.texture.units[unit], sampler->format, texel,
                   values, color);
    }
  }
  if (context->state.fog.enabled) {
    ApplyFog(context, values[kFog], color);
  }
  for (int i = 0; i < 4; i++) {
    color[i] = Clamp01(color[i]);
  }
  color[kAlpha] *= coverage;
}

static void EmitFragment(Context* context, const Primitive* primitive, int x,
                         int y, float z, const float values[kVaryings],
                         const float* lambda, float coverage) {
  const int* rect = primitive->rect;
  if (x < rect[0] || y < rect[1] || x >= rect[2] || y >= rect[3]) {
    return;
  }
  size_t index = (size_t)y * (size_t)context->framebuffer->width + (size_t)x;
  z = Clamp01(z);
  bool depth_pass = DepthPass(context, index, z);
  if (!depth_pass && !context->state.stencil.test) {
    return;
  }
  TextureLookup lookups[kTextureUnits] = {{0}};
  for (int unit = 0; unit < kTextureUnits; unit++) {
    const float* coord = &values[TexcoordVarying(unit)];
    if (primitive->samplers[unit].texture) {
      lookups[unit] =
          (TextureLookup){coord[kS] / coord[kQ], coord[kT] / coord[kQ],
                          lambda ? lambda[unit] : 0.0f};
    }
  }
  float color[4];
  ShadeColor(context, primitive, values, lookups, coverage, color);
  WriteFragment(context, primitive, index, z, depth_pass, color);
}

static void Shade(Context* context, const Setup* setup, size_t index,
                  const float weight[3], float coverage) {
  const Primitive* primitive = setup->primitive;
  const Corner* const* v = setup->corners;
  float z = Clamp01(v[0]->z + weight[1] * (v[1]->z - v[0]->z) +
                    weight[2] * (v[2]->z - v[0]->z) + primitive->offset);
  bool depth_pass = DepthPass(context, index, z);
  if (!depth_pass && !context->state.stencil.test) {
    return;
  }
  float w =
      1.0f / (weight[0] * v[0]->q + weight[1] * v[1]->q + weight[2] * v[2]->q);
  float varying[kVaryings];
  for (int i = 0; i < primitive->varyings; i++) {
    varying[i] = (weight[0] * v[0]->varying[i] + weight[1] * v[1]->varying[i] +
                  weight[2] * v[2]->varying[i]) *
                 w;
  }
  TextureLookup lookups[kTextureUnits] = {{0}};
  for (int unit = 0; unit < kTextureUnits; unit++) {
    const Texture* texture = primitive->samplers[unit].texture;
    if (!texture) {
      continue;
    }
    const float* coord = &varying[TexcoordVarying(unit)];
    float s = coord[kS];
    float t = coord[kT];
    float inverse_q = w;
    if (setup->projective[unit]) {
      s /= coord[kQ];
      t /= coord[kQ];
      inverse_q = w / coord[kQ];
    }
    const TextureParameters* parameters = &texture->parameters;
    float lambda = parameters->min_filter != parameters->mag_filter
                       ? Lambda(setup, unit, s, t, inverse_q)
                       : 0.0f;
    lookups[unit] = (TextureLookup){s, t, lambda};
  }
  float color[4];
  ShadeColor(context, primitive, varying, lookups, coverage, color);
  WriteFragment(context, primitive, index, z, depth_pass, color);
}

static bool StippleBit(const Context* context, int x, int y) {
  return (context->state.stipple[y % kStippleSize] >> (x % kStippleSize)) & 1;
}

static void PrepareSetup(Setup* setup, const Primitive* primitive,
                         const Corner* const v[3], const float gradient[2][3]) {
  *setup = (Setup){.primitive = primitive, .corners = {v[0], v[1], v[2]}};
  for (int unit = 0; unit < kTextureUnits; unit++) {
    if (!primitive->samplers[unit].texture) {
      continue;
    }
    int base = TexcoordVarying(unit);
    for (int k = 0; k < 3; k++) {
      setup->projective[unit] =
          setup->projective[unit] || v[k]->varying[base + kQ] != v[k]->q;
      for (int axis = 0; axis < 2; axis++) {
        for (int c = kS; c <= kQ; c++) {
          setup->derivative[unit][axis][c] +=
              gradient[axis][k] * v[k]->varying[base + c];
        }
      }
    }
  }
}

static void PixelBounds(const Corner* corners, int count, const int rect[4],
                        int bounds[4]) {
  float low_x = corners[0].x;
  float high_x = corners[0].x;
  float low_y = corners[0].y;
  float high_y = corners[0].y;
  for (int i = 1; i < count; i++) {
    low_x = MinFloat(low_x, corners[i].x);
    high_x = MaxFloat(high_x, corners[i].x);
    low_y = MinFloat(low_y, corners[i].y);
    high_y = MaxFloat(high_y, corners[i].y);
  }
  bounds[0] = ClampInt((int)Floor(low_x), rect[0], rect[2] - 1);
  bounds[1] = ClampInt((int)Floor(low_y), rect[1], rect[3] - 1);
  bounds[2] = ClampInt((int)Floor(high_x), rect[0], rect[2] - 1);
  bounds[3] = ClampInt((int)Floor(high_y), rect[1], rect[3] - 1);
}

static void RasterizeTriangle(Context* context, const Primitive* primitive,
                              const Corner* a, const Corner* b,
                              const Corner* c) {
  const int* rect = primitive->rect;
  if (rect[0] >= rect[2] || rect[1] >= rect[3]) {
    return;
  }
  const Corner* v[3] = {a, b, c};
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
    const Corner* corner = v[1];
    v[1] = v[2];
    v[2] = corner;
    int64_t swap = x[1];
    x[1] = x[2];
    x[2] = swap;
    swap = y[1];
    y[1] = y[2];
    y[2] = swap;
    area = -area;
  }
  int bounds[4];
  PixelBounds((const Corner[3]){*v[0], *v[1], *v[2]}, 3, rect, bounds);

  int64_t px = (int64_t)bounds[0] * kSubpixels + kSubpixels / 2;
  int64_t py = (int64_t)bounds[1] * kSubpixels + kSubpixels / 2;
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
  Setup setup;
  PrepareSetup(&setup, primitive, v, gradient);

  for (int pixel_y = bounds[1]; pixel_y <= bounds[3]; pixel_y++) {
    int64_t e[3] = {row[0], row[1], row[2]};
    size_t line = (size_t)pixel_y * (size_t)context->framebuffer->width;
    for (int pixel_x = bounds[0]; pixel_x <= bounds[2]; pixel_x++) {
      if ((e[0] | e[1] | e[2]) >= 0 &&
          (!primitive->stipple || StippleBit(context, pixel_x, pixel_y))) {
        float weight[3] = {(float)(e[0] + bias[0]) * inverse_area,
                           (float)(e[1] + bias[1]) * inverse_area,
                           (float)(e[2] + bias[2]) * inverse_area};
        Shade(context, &setup, line + (size_t)pixel_x, weight, 1.0f);
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

static float EdgeValue(const Corner* p, const Corner* q, float x, float y) {
  return (q->x - p->x) * (y - p->y) - (q->y - p->y) * (x - p->x);
}

static void SmoothPolygon(Context* context, const Primitive* primitive,
                          const Corner* corners, int count) {
  float area = 0.0f;
  for (int i = 0; i < count; i++) {
    area += corners[i].x * corners[(i + 1) % count].y -
            corners[(i + 1) % count].x * corners[i].y;
  }
  if (area == 0.0f) {
    return;
  }
  float sign = area > 0.0f ? 1.0f : -1.0f;
  int triangles = count - 2;
  Setup* setups = Allocate((size_t)triangles * sizeof(Setup));
  float (*planes)[3][3] = Allocate((size_t)triangles * sizeof(*planes));
  for (int t = 0; t < triangles; t++) {
    const Corner* v[3] = {&corners[0], &corners[t + 1], &corners[t + 2]};
    float twice = EdgeValue(v[0], v[1], v[2]->x, v[2]->y);
    float gradient[2][3] = {{0}};
    for (int k = 0; k < 3 && twice != 0.0f; k++) {
      const Corner* p = v[(k + 1) % 3];
      const Corner* q = v[(k + 2) % 3];
      planes[t][k][0] = -(q->y - p->y) / twice;
      planes[t][k][1] = (q->x - p->x) / twice;
      planes[t][k][2] = EdgeValue(p, q, 0.0f, 0.0f) / twice;
      gradient[0][k] = planes[t][k][0];
      gradient[1][k] = planes[t][k][1];
    }
    if (twice == 0.0f) {
      memset(planes[t], 0, sizeof(planes[t]));
      planes[t][0][2] = -1.0f;
    }
    PrepareSetup(&setups[t], primitive, v, gradient);
  }
  int bounds[4];
  PixelBounds(corners, count, primitive->rect, bounds);
  for (int pixel_y = bounds[1]; pixel_y <= bounds[3]; pixel_y++) {
    for (int pixel_x = bounds[0]; pixel_x <= bounds[2]; pixel_x++) {
      int inside = 0;
      for (int sy = 0; sy < kSamples; sy++) {
        for (int sx = 0; sx < kSamples; sx++) {
          float x = (float)pixel_x + ((float)sx + 0.5f) / kSamples;
          float y = (float)pixel_y + ((float)sy + 0.5f) / kSamples;
          bool covered = true;
          for (int i = 0; i < count && covered; i++) {
            covered = sign * EdgeValue(&corners[i], &corners[(i + 1) % count],
                                       x, y) >=
                      0.0f;
          }
          inside += covered;
        }
      }
      if (inside == 0 ||
          (primitive->stipple && !StippleBit(context, pixel_x, pixel_y))) {
        continue;
      }
      float x = (float)pixel_x + 0.5f;
      float y = (float)pixel_y + 0.5f;
      int best = 0;
      float best_weight = -INFINITY;
      float weights[3];
      for (int t = 0; t < triangles; t++) {
        float lowest = INFINITY;
        for (int k = 0; k < 3; k++) {
          lowest = MinFloat(lowest, planes[t][k][0] * x + planes[t][k][1] * y +
                                        planes[t][k][2]);
        }
        if (lowest > best_weight) {
          best_weight = lowest;
          best = t;
        }
      }
      for (int k = 0; k < 3; k++) {
        weights[k] = planes[best][k][0] * x + planes[best][k][1] * y +
                     planes[best][k][2];
      }
      Shade(context, &setups[best],
            (size_t)pixel_y * (size_t)context->framebuffer->width +
                (size_t)pixel_x,
            weights, (float)inside / (float)(kSamples * kSamples));
    }
  }
  free(planes);
  free(setups);
}

static void LineValues(const Corner* a, const Corner* b, float t,
                       float values[kVaryings]) {
  float w = 1.0f / ((1.0f - t) * a->q + t * b->q);
  for (int i = 0; i < kVaryings; i++) {
    values[i] = ((1.0f - t) * a->varying[i] + t * b->varying[i]) * w;
  }
}

static float LineLambda(const Primitive* primitive, int unit, const Corner* a,
                        const Corner* b, float t, const float values[kVaryings],
                        float length) {
  const Sampler* sampler = &primitive->samplers[unit];
  const TextureParameters* parameters = &sampler->texture->parameters;
  if (parameters->min_filter == parameters->mag_filter || length <= 0.0f) {
    return 0.0f;
  }
  int base = TexcoordVarying(unit);
  const float* from = &a->varying[base];
  const float* to = &b->varying[base];
  const float* coord = &values[base];
  float q = (1.0f - t) * from[kQ] + t * to[kQ];
  float dq = to[kQ] - from[kQ];
  float s = coord[kS] / coord[kQ];
  float tt = coord[kT] / coord[kQ];
  float ds = (to[kS] - from[kS] - s * dq) / q;
  float dt = (to[kT] - from[kT] - tt * dq) / q;
  float du = ds * TextureScale(sampler, 0) / length;
  float dv = dt * TextureScale(sampler, 1) / length;
  return LevelOfDetail(du * du + dv * dv);
}

static bool LineStippleBit(const Context* context, int counter) {
  const LineState* line = &context->state.line;
  return !line->stipple ||
         ((line->pattern >> ((counter / line->repeat) % 16)) & 1);
}

static void LineFragment(Context* context, const Primitive* primitive,
                         const Corner* a, const Corner* b, float t, int x,
                         int y, float coverage, float length) {
  float values[kVaryings];
  LineValues(a, b, t, values);
  float lambda[kTextureUnits] = {0};
  for (int unit = 0; unit < kTextureUnits; unit++) {
    if (primitive->samplers[unit].texture) {
      lambda[unit] = LineLambda(primitive, unit, a, b, t, values, length);
    }
  }
  float z = a->z + t * (b->z - a->z) + primitive->offset;
  EmitFragment(context, primitive, x, y, z, values, lambda, coverage);
}

static void SmoothLine(Context* context, const Primitive* primitive,
                       const Corner* a, const Corner* b) {
  float dx = b->x - a->x;
  float dy = b->y - a->y;
  float length = sqrtf(dx * dx + dy * dy);
  float ux = dx / length;
  float uy = dy / length;
  float half =
      0.5f * ClampFloat(context->state.line.width, 1.0f, (float)kMaxPointSize);
  bool x_major = fabsf(dx) >= fabsf(dy);
  float major_a = x_major ? a->x : a->y;
  float major_b = x_major ? b->x : b->y;
  float slope = x_major ? dy / dx : dx / dy;
  float extent = half * length / fabsf(x_major ? dx : dy) + 1.0f;
  int first = (int)Floor(MinFloat(major_a, major_b) - half);
  int last = (int)Floor(MaxFloat(major_a, major_b) + half);
  int counter = context->stipple_counter;
  for (int i = first; i <= last; i++) {
    float center = (float)i + 0.5f;
    float minor = (x_major ? a->y : a->x) + (center - major_a) * slope;
    int low = (int)Floor(minor - extent);
    int high = (int)Floor(minor + extent);
    for (int j = low; j <= high; j++) {
      int x = x_major ? i : j;
      int y = x_major ? j : i;
      int inside = 0;
      for (int sy = 0; sy < kSamples; sy++) {
        for (int sx = 0; sx < kSamples; sx++) {
          float px = (float)x + ((float)sx + 0.5f) / kSamples - a->x;
          float py = (float)y + ((float)sy + 0.5f) / kSamples - a->y;
          float along = px * ux + py * uy;
          float across = px * uy - py * ux;
          inside += along >= 0.0f && along <= length && fabsf(across) <= half &&
                    LineStippleBit(context, counter + (int)along);
        }
      }
      if (inside == 0) {
        continue;
      }
      float along =
          ((float)x + 0.5f - a->x) * ux + ((float)y + 0.5f - a->y) * uy;
      LineFragment(context, primitive, a, b, Clamp01(along / length), x, y,
                   (float)inside / (float)(kSamples * kSamples), length);
    }
  }
  context->stipple_counter = counter + (int)Ceil(length);
}

static void RasterizeLine(Context* context, const Primitive* primitive,
                          const Corner* a, const Corner* b) {
  float dx = b->x - a->x;
  float dy = b->y - a->y;
  if (dx == 0.0f && dy == 0.0f) {
    return;
  }
  if (context->state.line.smooth) {
    SmoothLine(context, primitive, a, b);
    return;
  }
  float length = sqrtf(dx * dx + dy * dy);
  bool x_major = fabsf(dx) >= fabsf(dy);
  int width =
      ClampInt((int)(context->state.line.width + 0.5f), 1, kMaxPointSize);
  float shift = (float)(width - 1) * 0.5f;
  float major_a = x_major ? a->x : a->y;
  float major_b = x_major ? b->x : b->y;
  float minor_a = (x_major ? a->y : a->x) - shift;
  float minor_b = (x_major ? b->y : b->x) - shift;
  float delta = major_b - major_a;
  int first;
  int last;
  int step;
  if (delta > 0.0f) {
    first = (int)Ceil(major_a - 0.5f);
    last = (int)Ceil(major_b - 0.5f) - 1;
    step = 1;
  } else {
    first = (int)Floor(major_a - 0.5f);
    last = (int)Floor(major_b - 0.5f) + 1;
    step = -1;
  }
  if ((last - first) * step < 0) {
    return;
  }
  for (int i = first;; i += step) {
    float t = ((float)i + 0.5f - major_a) / delta;
    int j = (int)Floor(minor_a + t * (minor_b - minor_a));
    if (LineStippleBit(context, context->stipple_counter)) {
      for (int k = 0; k < width; k++) {
        LineFragment(context, primitive, a, b, t, x_major ? i : j + k,
                     x_major ? j + k : i, 1.0f, length);
      }
    }
    context->stipple_counter++;
    if (i == last) {
      break;
    }
  }
}

static void RasterizePoint(Context* context, const Primitive* primitive,
                           const Corner* corner) {
  float values[kVaryings];
  for (int i = 0; i < kVaryings; i++) {
    values[i] = corner->varying[i] / corner->q;
  }
  float z = corner->z + primitive->offset;
  float size = context->state.point.size;
  if (context->state.point.smooth) {
    float radius = 0.5f * ClampFloat(Floor(size / kPointGranularity + 0.5f) *
                                         kPointGranularity,
                                     1.0f, (float)kMaxPointSize);
    int x0 = (int)Floor(corner->x - radius);
    int x1 = (int)Floor(corner->x + radius);
    int y0 = (int)Floor(corner->y - radius);
    int y1 = (int)Floor(corner->y + radius);
    for (int y = y0; y <= y1; y++) {
      for (int x = x0; x <= x1; x++) {
        int inside = 0;
        for (int sy = 0; sy < kSamples; sy++) {
          for (int sx = 0; sx < kSamples; sx++) {
            float px = (float)x + ((float)sx + 0.5f) / kSamples - corner->x;
            float py = (float)y + ((float)sy + 0.5f) / kSamples - corner->y;
            inside += px * px + py * py <= radius * radius;
          }
        }
        if (inside > 0) {
          EmitFragment(context, primitive, x, y, z, values, NULL,
                       (float)inside / (float)(kSamples * kSamples));
        }
      }
    }
    return;
  }
  int width = ClampInt((int)(size + 0.5f), 1, kMaxPointSize);
  float center_x =
      width % 2 ? Floor(corner->x) + 0.5f : Floor(corner->x + 0.5f);
  float center_y =
      width % 2 ? Floor(corner->y) + 0.5f : Floor(corner->y + 0.5f);
  int x0 = (int)Floor(center_x - (float)width * 0.5f);
  int y0 = (int)Floor(center_y - (float)width * 0.5f);
  for (int y = y0; y < y0 + width; y++) {
    for (int x = x0; x < x0 + width; x++) {
      EmitFragment(context, primitive, x, y, z, values, NULL, 1.0f);
    }
  }
}

static int MatrixStack(const Context* context) {
  switch (context->state.transform.matrix_mode) {
    case GL_PROJECTION:
      return kProjection;
    case GL_TEXTURE:
      return kTextureMatrix + context->state.texture.active;
    default:
      return kModelview;
  }
}

static float* CurrentMatrix(Context* context) {
  int stack = MatrixStack(context);
  if (stack == kModelview) {
    context->normal_matrix_stale = true;
  }
  return context->matrices[stack][context->matrix_top[stack]];
}

static const float* TopMatrix(const Context* context, int stack) {
  return context->matrices[stack][context->matrix_top[stack]];
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

static void InvertMatrix(const float m[16], float out[16]) {
  double a[4][8];
  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 4; c++) {
      a[r][c] = m[c * 4 + r];
      a[r][c + 4] = r == c ? 1.0 : 0.0;
    }
  }
  for (int c = 0; c < 4; c++) {
    int pivot = c;
    for (int r = c + 1; r < 4; r++) {
      if (fabs(a[r][c]) > fabs(a[pivot][c])) {
        pivot = r;
      }
    }
    if (a[pivot][c] == 0.0) {
      memcpy(out, kIdentity, sizeof(kIdentity));
      return;
    }
    for (int k = 0; k < 8; k++) {
      double swap = a[c][k];
      a[c][k] = a[pivot][k];
      a[pivot][k] = swap;
    }
    double scale = 1.0 / a[c][c];
    for (int k = 0; k < 8; k++) {
      a[c][k] *= scale;
    }
    for (int r = 0; r < 4; r++) {
      double factor = a[r][c];
      for (int k = 0; r != c && k < 8; k++) {
        a[r][k] -= factor * a[c][k];
      }
    }
  }
  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 4; c++) {
      out[c * 4 + r] = (float)a[r][c + 4];
    }
  }
}

static void TransformPlane(const Context* context, const float plane[4],
                           float out[4]) {
  float inverse[16];
  InvertMatrix(TopMatrix(context, kModelview), inverse);
  for (int c = 0; c < 4; c++) {
    out[c] = Dot4(plane, &inverse[c * 4]);
  }
}

static void EyeNormal(Context* context, const float normal[3], float out[3]) {
  if (context->normal_matrix_stale) {
    const float* m = TopMatrix(context, kModelview);
    float inverse[16];
    InvertMatrix((const float[16]){m[0], m[1], m[2], 0, m[4], m[5], m[6], 0,
                                   m[8], m[9], m[10], 0, 0, 0, 0, 1},
                 inverse);
    for (int c = 0; c < 3; c++) {
      for (int r = 0; r < 3; r++) {
        context->normal_matrix[c * 3 + r] = inverse[c * 4 + r];
      }
    }
    context->normal_matrix_stale = false;
  }
  for (int c = 0; c < 3; c++) {
    out[c] = Dot3(normal, &context->normal_matrix[c * 3]);
  }
  if (context->state.transform.normalize) {
    Normalize3(out);
  }
}

static bool Tracks(const LightingState* lighting, int face, GLenum property) {
  GLenum tracked_face = lighting->color_material_face;
  GLenum mode = lighting->color_material_mode;
  return lighting->color_material &&
         (tracked_face == GL_FRONT_AND_BACK ||
          tracked_face == (face == kFront ? GL_FRONT : GL_BACK)) &&
         (mode == property ||
          (mode == GL_AMBIENT_AND_DIFFUSE &&
           (property == GL_AMBIENT || property == GL_DIFFUSE)));
}

static void TrackColor(Context* context) {
  LightingState* lighting = &context->state.lighting;
  for (int face = kFront; face <= kBack; face++) {
    Material* material = &lighting->material[face];
    const float* color = context->state.current.color;
    if (Tracks(lighting, face, GL_AMBIENT)) {
      memcpy(material->ambient, color, sizeof(material->ambient));
    }
    if (Tracks(lighting, face, GL_DIFFUSE)) {
      memcpy(material->diffuse, color, sizeof(material->diffuse));
    }
    if (Tracks(lighting, face, GL_SPECULAR)) {
      memcpy(material->specular, color, sizeof(material->specular));
    }
    if (Tracks(lighting, face, GL_EMISSION)) {
      memcpy(material->emission, color, sizeof(material->emission));
    }
  }
}

static const float* MaterialColor(const LightingState* lighting, int face,
                                  GLenum property, const float color[4]) {
  const Material* material = &lighting->material[face];
  if (Tracks(lighting, face, property)) {
    return color;
  }
  switch (property) {
    case GL_AMBIENT:
      return material->ambient;
    case GL_DIFFUSE:
      return material->diffuse;
    case GL_SPECULAR:
      return material->specular;
    default:
      return material->emission;
  }
}

static void LightFace(const Context* context, int face, const float eye[4],
                      const float normal[3], const float color[4],
                      float out[4]) {
  const LightingState* lighting = &context->state.lighting;
  const float* ambient = MaterialColor(lighting, face, GL_AMBIENT, color);
  const float* diffuse = MaterialColor(lighting, face, GL_DIFFUSE, color);
  const float* specular = MaterialColor(lighting, face, GL_SPECULAR, color);
  const float* emission = MaterialColor(lighting, face, GL_EMISSION, color);
  float shininess = lighting->material[face].shininess;
  float n[3] = {normal[0], normal[1], normal[2]};
  if (face == kBack) {
    for (int i = 0; i < 3; i++) {
      n[i] = -n[i];
    }
  }
  float vertex[3] = {eye[0], eye[1], eye[2]};
  if (eye[3] != 0.0f && eye[3] != 1.0f) {
    for (int i = 0; i < 3; i++) {
      vertex[i] /= eye[3];
    }
  }
  for (int i = 0; i < 3; i++) {
    out[i] = emission[i] + ambient[i] * lighting->model_ambient[i];
  }
  for (int l = 0; l < kMaxLights; l++) {
    const Light* light = &lighting->lights[l];
    if (!light->enabled) {
      continue;
    }
    float vp[3];
    float attenuation = 1.0f;
    if (light->position[3] != 0.0f) {
      for (int i = 0; i < 3; i++) {
        vp[i] = light->position[i] / light->position[3] - vertex[i];
      }
      float distance = sqrtf(Dot3(vp, vp));
      attenuation =
          1.0f / (light->attenuation[0] + light->attenuation[1] * distance +
                  light->attenuation[2] * distance * distance);
    } else {
      memcpy(vp, light->position, sizeof(vp));
    }
    Normalize3(vp);
    float spot = 1.0f;
    if (light->cutoff != 180.0f) {
      float direction[3] = {light->direction[0], light->direction[1],
                            light->direction[2]};
      Normalize3(direction);
      float cosine = -Dot3(vp, direction);
      spot = cosine >= cosf(light->cutoff * kRadiansPerDegree)
                 ? powf(MaxFloat(cosine, 0.0f), light->exponent)
                 : 0.0f;
    }
    float factor = attenuation * spot;
    if (factor == 0.0f) {
      continue;
    }
    float n_dot_vp = MaxFloat(Dot3(n, vp), 0.0f);
    float h[3];
    for (int i = 0; i < 3; i++) {
      h[i] = vp[i];
    }
    if (lighting->local_viewer) {
      float viewer[3] = {-vertex[0], -vertex[1], -vertex[2]};
      Normalize3(viewer);
      for (int i = 0; i < 3; i++) {
        h[i] += viewer[i];
      }
    } else {
      h[2] += 1.0f;
    }
    Normalize3(h);
    float highlight =
        n_dot_vp != 0.0f ? powf(MaxFloat(Dot3(n, h), 0.0f), shininess) : 0.0f;
    for (int i = 0; i < 3; i++) {
      out[i] += factor * (ambient[i] * light->ambient[i] +
                          n_dot_vp * diffuse[i] * light->diffuse[i] +
                          highlight * specular[i] * light->specular[i]);
    }
  }
  out[3] = diffuse[3];
}

static bool SphereMap(const TexGen gen[4]) {
  return (gen[0].enabled && gen[0].mode == GL_SPHERE_MAP) ||
         (gen[1].enabled && gen[1].mode == GL_SPHERE_MAP);
}

static void GenerateTexcoords(const TexGen gen[4], const float object[4],
                              const float eye[4], const float normal[3],
                              float texcoord[4]) {
  float reflection[3] = {0};
  float m = 1.0f;
  if (SphereMap(gen)) {
    float u[3] = {eye[0], eye[1], eye[2]};
    Normalize3(u);
    float d = Dot3(normal, u);
    for (int i = 0; i < 3; i++) {
      reflection[i] = u[i] - 2.0f * normal[i] * d;
    }
    m = 2.0f *
        sqrtf(reflection[0] * reflection[0] + reflection[1] * reflection[1] +
              (reflection[2] + 1.0f) * (reflection[2] + 1.0f));
  }
  for (int coord = 0; coord < 4; coord++) {
    if (!gen[coord].enabled) {
      continue;
    }
    switch (gen[coord].mode) {
      case GL_OBJECT_LINEAR:
        texcoord[coord] = Dot4(gen[coord].object, object);
        break;
      case GL_EYE_LINEAR:
        texcoord[coord] = Dot4(gen[coord].eye, eye);
        break;
      default:
        texcoord[coord] = m != 0.0f ? reflection[coord] / m + 0.5f : 0.5f;
    }
  }
}

static void ProcessVertex(Context* context, const float object[4],
                          const float color[4], const float normal[3],
                          const float texcoords[kTextureUnits][4],
                          Vertex* vertex) {
  Transform(TopMatrix(context, kModelview), object, vertex->eye);
  Transform(TopMatrix(context, kProjection), vertex->eye, vertex->clip);
  const State* state = &context->state;
  bool sphere = false;
  for (int unit = 0; unit < kTextureUnits; unit++) {
    sphere = sphere || SphereMap(state->texture.units[unit].gen);
  }
  float eye_normal[3] = {0};
  if (state->lighting.enabled || sphere) {
    EyeNormal(context, normal, eye_normal);
  }
  if (state->lighting.enabled) {
    LightFace(context, kFront, vertex->eye, eye_normal, color,
              vertex->color[kFront]);
    if (state->lighting.two_side) {
      LightFace(context, kBack, vertex->eye, eye_normal, color,
                vertex->color[kBack]);
    } else {
      memcpy(vertex->color[kBack], vertex->color[kFront], 4 * sizeof(float));
    }
  } else {
    memcpy(vertex->color[kFront], color, 4 * sizeof(float));
    memcpy(vertex->color[kBack], color, 4 * sizeof(float));
  }
  for (int face = kFront; face <= kBack; face++) {
    for (int i = 0; i < 4; i++) {
      vertex->color[face][i] = Clamp01(vertex->color[face][i]);
    }
  }
  for (int unit = 0; unit < kTextureUnits; unit++) {
    const TexGen* gen = state->texture.units[unit].gen;
    float generated[4];
    memcpy(generated, texcoords[unit], sizeof(generated));
    if (gen[0].enabled || gen[1].enabled || gen[2].enabled || gen[3].enabled) {
      GenerateTexcoords(gen, object, vertex->eye, eye_normal, generated);
    }
    Transform(TopMatrix(context, kTextureMatrix + unit), generated,
              vertex->texcoord[unit]);
  }
  vertex->fog = vertex->eye[2];
  vertex->edge = state->current.edge_flag;
}

static int ClipPlanes(const Context* context, int planes[kClipPlanes]) {
  int count = 0;
  for (int plane = 0; plane < kClipPlanes; plane++) {
    if (plane < 6 || context->state.transform.clip[plane - 6]) {
      planes[count++] = plane;
    }
  }
  return count;
}

static float ClipDistance(const Context* context, const Vertex* vertex,
                          int plane) {
  if (plane >= 6) {
    return Dot4(context->state.transform.planes[plane - 6], vertex->eye);
  }
  float w = vertex->clip[3];
  float p = vertex->clip[plane / 2];
  return plane % 2 ? w - p : w + p;
}

static void Interpolate(const Vertex* p, const Vertex* q, float t,
                        Vertex* out) {
  Vertex result = *p;
  for (int i = 0; i < 4; i++) {
    result.clip[i] += (q->clip[i] - p->clip[i]) * t;
    result.eye[i] += (q->eye[i] - p->eye[i]) * t;
    result.color[kFront][i] += (q->color[kFront][i] - p->color[kFront][i]) * t;
    result.color[kBack][i] += (q->color[kBack][i] - p->color[kBack][i]) * t;
    for (int unit = 0; unit < kTextureUnits; unit++) {
      result.texcoord[unit][i] +=
          (q->texcoord[unit][i] - p->texcoord[unit][i]) * t;
    }
  }
  result.fog += (q->fog - p->fog) * t;
  *out = result;
}

static bool InsideClipVolume(const Context* context, const Vertex* vertex) {
  int planes[kClipPlanes];
  int count = ClipPlanes(context, planes);
  for (int i = 0; i < count; i++) {
    if (ClipDistance(context, vertex, planes[i]) < 0.0f) {
      return false;
    }
  }
  return true;
}

static bool ClipLine(const Context* context, Vertex* a, Vertex* b) {
  int planes[kClipPlanes];
  int count = ClipPlanes(context, planes);
  float t0 = 0.0f;
  float t1 = 1.0f;
  for (int i = 0; i < count; i++) {
    float da = ClipDistance(context, a, planes[i]);
    float db = ClipDistance(context, b, planes[i]);
    if (da < 0.0f && db < 0.0f) {
      return false;
    }
    if (da < 0.0f) {
      t0 = MaxFloat(t0, da / (da - db));
    } else if (db < 0.0f) {
      t1 = MinFloat(t1, da / (da - db));
    }
  }
  if (t0 > t1) {
    return false;
  }
  Vertex start = *a;
  Vertex end = *b;
  if (t0 > 0.0f) {
    Interpolate(&start, &end, t0, a);
  }
  if (t1 < 1.0f) {
    Interpolate(&start, &end, t1, b);
  }
  return true;
}

static int ClipPolygon(const Context* context, Vertex* polygon, Vertex* scratch,
                       int count, int capacity) {
  int planes[kClipPlanes];
  int plane_count = ClipPlanes(context, planes);
  for (int n = 0; n < plane_count && count > 0; n++) {
    int plane = planes[n];
    bool inside = true;
    for (int i = 0; i < count; i++) {
      inside = inside && ClipDistance(context, &polygon[i], plane) >= 0.0f;
    }
    if (inside) {
      continue;
    }
    memcpy(scratch, polygon, (size_t)count * sizeof(Vertex));
    int output = 0;
    for (int i = 0; i < count; i++) {
      const Vertex* p = &scratch[i];
      const Vertex* q = &scratch[(i + 1) % count];
      float dp = ClipDistance(context, p, plane);
      float dq = ClipDistance(context, q, plane);
      if (dp >= 0.0f && output < capacity) {
        polygon[output++] = *p;
      }
      if ((dp >= 0.0f) != (dq >= 0.0f) && output < capacity) {
        Vertex* v = &polygon[output++];
        Interpolate(p, q, dp / (dp - dq), v);
        v->edge = dp >= 0.0f || p->edge;
      }
    }
    count = output;
  }
  return count;
}

static float ToWindow(float ndc, int origin, int size) {
  float window = (ndc + 1.0f) * 0.5f * (float)size + (float)origin;
  window = MaxFloat(MinFloat(window, (float)kGuardBand), -(float)kGuardBand);
  return (float)llroundf(window * kSubpixels) / kSubpixels;
}

static void ToCorner(const Context* context, const Vertex* vertex, int face,
                     Corner* corner) {
  const ViewportState* viewport = &context->state.viewport;
  float q = 1.0f / vertex->clip[3];
  corner->q = q;
  corner->x = ToWindow(vertex->clip[0] * q, viewport->box[0], viewport->box[2]);
  corner->y = ToWindow(vertex->clip[1] * q, viewport->box[1], viewport->box[3]);
  corner->z = viewport->depth_range[0] +
              (vertex->clip[2] * q + 1.0f) * 0.5f *
                  (viewport->depth_range[1] - viewport->depth_range[0]);
  for (int i = 0; i < 4; i++) {
    corner->varying[i] = vertex->color[face][i] * q;
  }
  corner->varying[kFog] = vertex->fog * q;
  for (int unit = 0; unit < kTextureUnits; unit++) {
    float* coord = &corner->varying[TexcoordVarying(unit)];
    coord[kS] = vertex->texcoord[unit][0] * q;
    coord[kT] = vertex->texcoord[unit][1] * q;
    coord[kQ] = vertex->texcoord[unit][3] * q;
  }
}

static void SelectHit(Context* context, float z) {
  Selection* selection = &context->selection;
  if (!selection->hit) {
    selection->hit = true;
    selection->min_depth = z;
    selection->max_depth = z;
  }
  selection->min_depth = MinFloat(selection->min_depth, z);
  selection->max_depth = MaxFloat(selection->max_depth, z);
}

static void SelectValue(Selection* selection, GLuint value) {
  if (selection->count < selection->size) {
    selection->buffer[selection->count++] = value;
  } else {
    selection->overflow = true;
  }
}

static void WriteHitRecord(Context* context) {
  Selection* selection = &context->selection;
  if (!selection->hit) {
    return;
  }
  SelectValue(selection, (GLuint)selection->depth);
  SelectValue(selection, (GLuint)(Clamp01(selection->min_depth) * kUintMax));
  SelectValue(selection, (GLuint)(Clamp01(selection->max_depth) * kUintMax));
  for (int i = 0; i < selection->depth; i++) {
    SelectValue(selection, selection->names[i]);
  }
  selection->hits++;
  selection->hit = false;
}

static void FeedbackValue(Context* context, float value) {
  Feedback* feedback = &context->feedback;
  if (feedback->count < feedback->size) {
    feedback->buffer[feedback->count++] = value;
  } else {
    feedback->overflow = true;
  }
}

static void FeedbackVertex(Context* context, const float window[4],
                           const float color[4], const float texcoord[4]) {
  GLenum type = context->feedback.type;
  int coordinates = type == GL_2D ? 2 : (type == GL_4D_COLOR_TEXTURE ? 4 : 3);
  for (int i = 0; i < coordinates; i++) {
    FeedbackValue(context, window[i]);
  }
  for (int i = 0; i < 4 && type != GL_2D && type != GL_3D; i++) {
    FeedbackValue(context, color[i]);
  }
  for (int i = 0;
       i < 4 && (type == GL_3D_COLOR_TEXTURE || type == GL_4D_COLOR_TEXTURE);
       i++) {
    FeedbackValue(context, texcoord[i]);
  }
}

static void FeedbackCorner(Context* context, const Vertex* vertex,
                           const Corner* corner, int face) {
  FeedbackVertex(
      context,
      (const float[4]){corner->x, corner->y, corner->z, vertex->clip[3]},
      vertex->color[face], vertex->texcoord[0]);
}

static void SelectTargets(Context* context, Targets* targets) {
  GLenum buffer = context->state.color.draw_buffer;
  targets->count = 0;
  if (buffer == GL_FRONT || buffer == GL_FRONT_LEFT || buffer == GL_LEFT ||
      buffer == GL_FRONT_AND_BACK) {
    targets->buffers[targets->count++] = context->framebuffer->front;
    context->framebuffer->front_dirty = true;
  }
  if (buffer == GL_BACK || buffer == GL_BACK_LEFT || buffer == GL_LEFT ||
      buffer == GL_FRONT_AND_BACK) {
    targets->buffers[targets->count++] = context->framebuffer->back;
  }
  const bool* mask = context->state.color.mask;
  targets->mask = (mask[0] ? 0x00FF0000u : 0) | (mask[1] ? 0x0000FF00u : 0) |
                  (mask[2] ? 0x000000FFu : 0) | (mask[3] ? 0xFF000000u : 0);
}

static void DrawableRect(const Context* context, int rect[4]) {
  rect[0] = 0;
  rect[1] = 0;
  rect[2] = context->framebuffer->width;
  rect[3] = context->framebuffer->height;
  const ScissorState* scissor = &context->state.scissor;
  if (scissor->test) {
    rect[0] = MaxInt(rect[0], scissor->box[0]);
    rect[1] = MaxInt(rect[1], scissor->box[1]);
    rect[2] = MinInt(rect[2], scissor->box[0] + scissor->box[2]);
    rect[3] = MinInt(rect[3], scissor->box[1] + scissor->box[3]);
  }
}

static void PrepareSampler(const Context* context, int unit, Sampler* sampler) {
  *sampler = (Sampler){0};
  const TextureUnit* state = &context->state.texture.units[unit];
  int target = state->enabled[kTexture2D] ? kTexture2D : kTexture1D;
  if (!state->enabled[target]) {
    return;
  }
  const Texture* texture = context->bound[unit][target];
  bool one_dimensional = target == kTexture1D;
  int levels = CompleteLevels(texture, one_dimensional);
  if (levels == 0) {
    return;
  }
  sampler->texture = texture;
  sampler->levels = levels;
  sampler->one_dimensional = one_dimensional;
  sampler->format = texture->levels[0].format;
  uint8_t rgba[4];
  for (int i = 0; i < 4; i++) {
    rgba[i] = (uint8_t)PackChannel(texture->parameters.border_color[i]);
  }
  StoreTexel(sampler->format, rgba, sampler->border);
}

static void PreparePrimitive(Context* context, Primitive* primitive) {
  *primitive = (Primitive){.varyings = kTexcoords};
  for (int unit = 0; unit < kTextureUnits; unit++) {
    PrepareSampler(context, unit, &primitive->samplers[unit]);
    if (primitive->samplers[unit].texture) {
      primitive->varyings = TexcoordVarying(unit + 1);
    }
  }
  SelectTargets(context, &primitive->targets);
  DrawableRect(context, primitive->rect);
}

static bool Facing(const Context* context, const Corner* corners, int count) {
  float area = 0.0f;
  for (int i = 0; i < count; i++) {
    const Corner* p = &corners[i];
    const Corner* q = &corners[(i + 1) % count];
    area += p->x * q->y - q->x * p->y;
  }
  if (context->state.polygon.front_face == GL_CW) {
    area = -area;
  }
  return area > 0.0f;
}

static float DepthSlope(const Corner* a, const Corner* b, const Corner* c) {
  float e1x = b->x - a->x;
  float e1y = b->y - a->y;
  float e1z = b->z - a->z;
  float e2x = c->x - a->x;
  float e2y = c->y - a->y;
  float e2z = c->z - a->z;
  float area = e1x * e2y - e2x * e1y;
  if (area == 0.0f) {
    return 0.0f;
  }
  float dzdx = (e1z * e2y - e2z * e1y) / area;
  float dzdy = (e2z * e1x - e1z * e2x) / area;
  return MaxFloat(fabsf(dzdx), fabsf(dzdy));
}

static float OffsetForSlope(const Context* context, float slope) {
  return context->state.polygon.offset_factor * slope +
         context->state.polygon.offset_units * kDepthUnit;
}

static void EnsureClipCapacity(Context* context, int count) {
  if (count <= context->clip_capacity) {
    return;
  }
  int old_capacity = context->clip_capacity;
  context->clip_capacity = MaxInt(count, 2 * old_capacity);
  size_t capacity = (size_t)context->clip_capacity;
  context->clip_buffer =
      Reallocate(context->clip_buffer, 2 * capacity * sizeof(Vertex));
  context->corners = Reallocate(context->corners, capacity * sizeof(Corner));
}

static void FeedbackEdges(Context* context, const Vertex* polygon,
                          const Corner* corners, int count, int face,
                          GLenum mode) {
  bool reset = true;
  for (int i = 0; i < count; i++) {
    if (!polygon[i].edge) {
      continue;
    }
    int next = (i + 1) % count;
    if (mode == GL_POINT) {
      FeedbackValue(context, GL_POINT_TOKEN);
      FeedbackCorner(context, &polygon[i], &corners[i], face);
      continue;
    }
    FeedbackValue(context, reset ? GL_LINE_RESET_TOKEN : GL_LINE_TOKEN);
    FeedbackCorner(context, &polygon[i], &corners[i], face);
    FeedbackCorner(context, &polygon[next], &corners[next], face);
    reset = false;
  }
}

static void ClipToViewport(const Context* context, int rect[4]) {
  const GLint* box = context->state.viewport.box;
  rect[0] = MaxInt(rect[0], box[0]);
  rect[1] = MaxInt(rect[1], box[1]);
  rect[2] = MinInt(rect[2], box[0] + box[2]);
  rect[3] = MinInt(rect[3], box[1] + box[3]);
}

static void DrawPolygon(Context* context, int count, int provoking,
                        bool edges) {
  Vertex* polygon = context->clip_buffer;
  Vertex* scratch = polygon + context->clip_capacity;
  float flat[2][4];
  memcpy(flat, polygon[provoking].color, sizeof(flat));
  for (int i = 0; i < count; i++) {
    polygon[i].edge = polygon[i].edge || !edges;
    if (context->state.lighting.shade_model == GL_FLAT) {
      memcpy(polygon[i].color, flat, sizeof(flat));
    }
  }
  count = ClipPolygon(context, polygon, scratch, count, context->clip_capacity);
  if (count < 3) {
    return;
  }
  Corner* corners = context->corners;
  for (int i = 0; i < count; i++) {
    if (!(polygon[i].clip[3] > 0.0f)) {
      return;
    }
    ToCorner(context, &polygon[i], kFront, &corners[i]);
  }
  const PolygonState* state = &context->state.polygon;
  bool front = Facing(context, corners, count);
  if (state->cull && (state->cull_mode == GL_FRONT_AND_BACK ||
                      state->cull_mode == (front ? GL_FRONT : GL_BACK))) {
    return;
  }
  if (context->render_mode == GL_SELECT) {
    for (int i = 0; i < count; i++) {
      SelectHit(context, corners[i].z);
    }
    return;
  }
  const LightingState* lighting = &context->state.lighting;
  int face = !front && lighting->enabled && lighting->two_side ? kBack : kFront;
  if (face == kBack) {
    for (int i = 0; i < count; i++) {
      ToCorner(context, &polygon[i], kBack, &corners[i]);
    }
  }
  GLenum mode = state->mode[front ? 0 : 1];
  if (context->render_mode == GL_FEEDBACK) {
    if (mode == GL_FILL) {
      FeedbackValue(context, GL_POLYGON_TOKEN);
      FeedbackValue(context, (float)count);
      for (int i = 0; i < count; i++) {
        FeedbackCorner(context, &polygon[i], &corners[i], face);
      }
    } else {
      FeedbackEdges(context, polygon, corners, count, face, mode);
    }
    return;
  }
  Primitive primitive;
  PreparePrimitive(context, &primitive);
  bool offset = mode == GL_FILL   ? state->offset_fill
                : mode == GL_LINE ? state->offset_line
                                  : state->offset_point;
  if (mode == GL_FILL && !state->smooth) {
    ClipToViewport(context, primitive.rect);
    primitive.stipple = state->stipple;
    for (int i = 2; i < count; i++) {
      if (offset) {
        primitive.offset = OffsetForSlope(
            context, DepthSlope(&corners[0], &corners[i - 1], &corners[i]));
      }
      RasterizeTriangle(context, &primitive, &corners[0], &corners[i - 1],
                        &corners[i]);
    }
    return;
  }
  if (offset) {
    float slope = 0.0f;
    for (int i = 2; i < count; i++) {
      slope = MaxFloat(slope,
                       DepthSlope(&corners[0], &corners[i - 1], &corners[i]));
    }
    primitive.offset = OffsetForSlope(context, slope);
  }
  if (mode == GL_FILL) {
    ClipToViewport(context, primitive.rect);
    primitive.stipple = state->stipple;
    SmoothPolygon(context, &primitive, corners, count);
    return;
  }
  context->stipple_counter = 0;
  for (int i = 0; i < count; i++) {
    if (!polygon[i].edge) {
      continue;
    }
    if (mode == GL_POINT) {
      RasterizePoint(context, &primitive, &corners[i]);
    } else {
      RasterizeLine(context, &primitive, &corners[i],
                    &corners[(i + 1) % count]);
    }
  }
}

static void DrawVertices(Context* context, const Vertex* const* vertices,
                         int count, bool edges) {
  EnsureClipCapacity(context, count + kClipPlanes);
  for (int i = 0; i < count; i++) {
    context->clip_buffer[i] = *vertices[i];
  }
  DrawPolygon(context, count, count - 1, edges);
}

static void DrawLine(Context* context, const Vertex* a, const Vertex* b,
                     bool reset) {
  if (reset) {
    context->stipple_counter = 0;
  }
  Vertex start = *a;
  Vertex end = *b;
  if (context->state.lighting.shade_model == GL_FLAT) {
    memcpy(start.color, end.color, sizeof(start.color));
  }
  if (!ClipLine(context, &start, &end)) {
    return;
  }
  Corner corners[2];
  ToCorner(context, &start, kFront, &corners[0]);
  ToCorner(context, &end, kFront, &corners[1]);
  if (context->render_mode == GL_SELECT) {
    SelectHit(context, corners[0].z);
    SelectHit(context, corners[1].z);
    return;
  }
  if (context->render_mode == GL_FEEDBACK) {
    FeedbackValue(context, reset ? GL_LINE_RESET_TOKEN : GL_LINE_TOKEN);
    FeedbackCorner(context, &start, &corners[0], kFront);
    FeedbackCorner(context, &end, &corners[1], kFront);
    return;
  }
  Primitive primitive;
  PreparePrimitive(context, &primitive);
  RasterizeLine(context, &primitive, &corners[0], &corners[1]);
}

static void DrawPoint(Context* context, const Vertex* vertex) {
  if (!InsideClipVolume(context, vertex) || !(vertex->clip[3] > 0.0f)) {
    return;
  }
  Corner corner;
  ToCorner(context, vertex, kFront, &corner);
  if (context->render_mode == GL_SELECT) {
    SelectHit(context, corner.z);
    return;
  }
  if (context->render_mode == GL_FEEDBACK) {
    FeedbackValue(context, GL_POINT_TOKEN);
    FeedbackCorner(context, vertex, &corner, kFront);
    return;
  }
  Primitive primitive;
  PreparePrimitive(context, &primitive);
  RasterizePoint(context, &primitive, &corner);
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
    case GL_POINTS:
      DrawPoint(context, v3);
      break;
    case GL_LINES:
      if (k % 2 == 1) {
        DrawLine(context, v2, v3, true);
      }
      break;
    case GL_LINE_STRIP:
    case GL_LINE_LOOP:
      if (k >= 1) {
        DrawLine(context, v2, v3, k == 1);
      }
      break;
    case GL_TRIANGLES:
      if (k % 3 == 2) {
        DrawVertices(context, (const Vertex* const[3]){v1, v2, v3}, 3, true);
      }
      break;
    case GL_TRIANGLE_STRIP:
      if (k >= 2) {
        DrawVertices(
            context,
            (const Vertex* const[3]){k % 2 ? v2 : v1, k % 2 ? v1 : v2, v3}, 3,
            false);
      }
      break;
    case GL_TRIANGLE_FAN:
      if (k >= 2) {
        DrawVertices(context, (const Vertex* const[3]){first, v2, v3}, 3,
                     false);
      }
      break;
    case GL_QUADS:
      if (k % 4 == 3) {
        DrawVertices(context, (const Vertex* const[4]){v0, v1, v2, v3}, 4,
                     true);
      }
      break;
    case GL_QUAD_STRIP:
      if (k >= 3 && k % 2 == 1) {
        DrawVertices(context, (const Vertex* const[4]){v2, v0, v1, v3}, 4,
                     false);
      }
      break;
    default:
      EnsureClipCapacity(context, k + 1 + kClipPlanes);
      context->clip_buffer[k] = *vertex;
  }
}

static void BeginPrimitive(Context* context, GLenum mode) {
  context->inside = true;
  context->mode = mode;
  context->vertex_count = 0;
  context->stipple_counter = 0;
}

static void EndPrimitive(Context* context) {
  context->inside = false;
  int count = context->vertex_count;
  if (context->mode == GL_LINE_LOOP && count >= 2) {
    DrawLine(context, &context->recent[(count - 1) % 4], &context->first,
             false);
  }
  if (context->mode == GL_POLYGON && count >= 3) {
    DrawPolygon(context, count, 0, true);
  }
}

static void EmitVertex(Context* context, const float object[4],
                       const float color[4], const float normal[3],
                       const float texcoords[kTextureUnits][4]) {
  if (!context->inside) {
    return;
  }
  Vertex vertex;
  ProcessVertex(context, object, color, normal, texcoords, &vertex);
  Assemble(context, &vertex);
}

static void EmitCurrentVertex(Context* context, const float object[4]) {
  const CurrentState* current_state = &context->state.current;
  EmitVertex(context, object, current_state->color, current_state->normal,
             current_state->texcoord);
}

static void APIENTRY Begin(GLenum mode) {
  Context* context = current;
  if (Record(context, kOpBegin, (Arg[]){{.u = mode}}, 1)) {
    return;
  }
  if (context->inside) {
    SetError(context, GL_INVALID_OPERATION);
    return;
  }
  if (mode > GL_POLYGON) {
    SetError(context, GL_INVALID_ENUM);
    return;
  }
  BeginPrimitive(context, mode);
}

static void APIENTRY End(void) {
  Context* context = current;
  if (Record(context, kOpEnd, NULL, 0)) {
    return;
  }
  if (!context->inside) {
    SetError(context, GL_INVALID_OPERATION);
    return;
  }
  EndPrimitive(context);
}

static void APIENTRY Vertex4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
  Context* context = current;
  if (Record(context, kOpVertex,
             (Arg[]){{.f = x}, {.f = y}, {.f = z}, {.f = w}}, 4)) {
    return;
  }
  EmitCurrentVertex(context, (const float[4]){x, y, z, w});
}

static void APIENTRY Vertex2d(GLdouble x, GLdouble y) {
  Vertex4f((GLfloat)x, (GLfloat)y, 0.0f, 1.0f);
}

static void APIENTRY Vertex2dv(const GLdouble* v) { Vertex2d(v[0], v[1]); }

static void APIENTRY Vertex2f(GLfloat x, GLfloat y) {
  Vertex4f(x, y, 0.0f, 1.0f);
}

static void APIENTRY Vertex2fv(const GLfloat* v) { Vertex2f(v[0], v[1]); }

static void APIENTRY Vertex2i(GLint x, GLint y) {
  Vertex4f((GLfloat)x, (GLfloat)y, 0.0f, 1.0f);
}

static void APIENTRY Vertex2iv(const GLint* v) { Vertex2i(v[0], v[1]); }

static void APIENTRY Vertex2s(GLshort x, GLshort y) {
  Vertex4f((GLfloat)x, (GLfloat)y, 0.0f, 1.0f);
}

static void APIENTRY Vertex2sv(const GLshort* v) { Vertex2s(v[0], v[1]); }

static void APIENTRY Vertex3d(GLdouble x, GLdouble y, GLdouble z) {
  Vertex4f((GLfloat)x, (GLfloat)y, (GLfloat)z, 1.0f);
}

static void APIENTRY Vertex3dv(const GLdouble* v) {
  Vertex3d(v[0], v[1], v[2]);
}

static void APIENTRY Vertex3f(GLfloat x, GLfloat y, GLfloat z) {
  Vertex4f(x, y, z, 1.0f);
}

static void APIENTRY Vertex3fv(const GLfloat* v) { Vertex3f(v[0], v[1], v[2]); }

static void APIENTRY Vertex3i(GLint x, GLint y, GLint z) {
  Vertex4f((GLfloat)x, (GLfloat)y, (GLfloat)z, 1.0f);
}

static void APIENTRY Vertex3iv(const GLint* v) { Vertex3i(v[0], v[1], v[2]); }

static void APIENTRY Vertex3s(GLshort x, GLshort y, GLshort z) {
  Vertex4f((GLfloat)x, (GLfloat)y, (GLfloat)z, 1.0f);
}

static void APIENTRY Vertex3sv(const GLshort* v) { Vertex3s(v[0], v[1], v[2]); }

static void APIENTRY Vertex4d(GLdouble x, GLdouble y, GLdouble z, GLdouble w) {
  Vertex4f((GLfloat)x, (GLfloat)y, (GLfloat)z, (GLfloat)w);
}

static void APIENTRY Vertex4dv(const GLdouble* v) {
  Vertex4d(v[0], v[1], v[2], v[3]);
}

static void APIENTRY Vertex4fv(const GLfloat* v) {
  Vertex4f(v[0], v[1], v[2], v[3]);
}

static void APIENTRY Vertex4i(GLint x, GLint y, GLint z, GLint w) {
  Vertex4f((GLfloat)x, (GLfloat)y, (GLfloat)z, (GLfloat)w);
}

static void APIENTRY Vertex4iv(const GLint* v) {
  Vertex4i(v[0], v[1], v[2], v[3]);
}

static void APIENTRY Vertex4s(GLshort x, GLshort y, GLshort z, GLshort w) {
  Vertex4f((GLfloat)x, (GLfloat)y, (GLfloat)z, (GLfloat)w);
}

static void APIENTRY Vertex4sv(const GLshort* v) {
  Vertex4s(v[0], v[1], v[2], v[3]);
}

static void APIENTRY Color4f(GLfloat red, GLfloat green, GLfloat blue,
                             GLfloat alpha) {
  Context* context = current;
  if (Record(context, kOpColor,
             (Arg[]){{.f = red}, {.f = green}, {.f = blue}, {.f = alpha}}, 4)) {
    return;
  }
  float* color = context->state.current.color;
  color[0] = red;
  color[1] = green;
  color[2] = blue;
  color[3] = alpha;
  if (context->state.lighting.color_material) {
    TrackColor(context);
  }
}

static void APIENTRY Color3b(GLbyte red, GLbyte green, GLbyte blue) {
  Color4f(FromByte(red), FromByte(green), FromByte(blue), 1.0f);
}

static void APIENTRY Color3bv(const GLbyte* v) { Color3b(v[0], v[1], v[2]); }

static void APIENTRY Color3d(GLdouble red, GLdouble green, GLdouble blue) {
  Color4f((GLfloat)red, (GLfloat)green, (GLfloat)blue, 1.0f);
}

static void APIENTRY Color3dv(const GLdouble* v) { Color3d(v[0], v[1], v[2]); }

static void APIENTRY Color3f(GLfloat red, GLfloat green, GLfloat blue) {
  Color4f(red, green, blue, 1.0f);
}

static void APIENTRY Color3fv(const GLfloat* v) { Color3f(v[0], v[1], v[2]); }

static void APIENTRY Color3i(GLint red, GLint green, GLint blue) {
  Color4f(FromInt(red), FromInt(green), FromInt(blue), 1.0f);
}

static void APIENTRY Color3iv(const GLint* v) { Color3i(v[0], v[1], v[2]); }

static void APIENTRY Color3s(GLshort red, GLshort green, GLshort blue) {
  Color4f(FromShort(red), FromShort(green), FromShort(blue), 1.0f);
}

static void APIENTRY Color3sv(const GLshort* v) { Color3s(v[0], v[1], v[2]); }

static void APIENTRY Color3ub(GLubyte red, GLubyte green, GLubyte blue) {
  Color4f(FromUbyte(red), FromUbyte(green), FromUbyte(blue), 1.0f);
}

static void APIENTRY Color3ubv(const GLubyte* v) { Color3ub(v[0], v[1], v[2]); }

static void APIENTRY Color3ui(GLuint red, GLuint green, GLuint blue) {
  Color4f(FromUint(red), FromUint(green), FromUint(blue), 1.0f);
}

static void APIENTRY Color3uiv(const GLuint* v) { Color3ui(v[0], v[1], v[2]); }

static void APIENTRY Color3us(GLushort red, GLushort green, GLushort blue) {
  Color4f(FromUshort(red), FromUshort(green), FromUshort(blue), 1.0f);
}

static void APIENTRY Color3usv(const GLushort* v) {
  Color3us(v[0], v[1], v[2]);
}

static void APIENTRY Color4b(GLbyte red, GLbyte green, GLbyte blue,
                             GLbyte alpha) {
  Color4f(FromByte(red), FromByte(green), FromByte(blue), FromByte(alpha));
}

static void APIENTRY Color4bv(const GLbyte* v) {
  Color4b(v[0], v[1], v[2], v[3]);
}

static void APIENTRY Color4d(GLdouble red, GLdouble green, GLdouble blue,
                             GLdouble alpha) {
  Color4f((GLfloat)red, (GLfloat)green, (GLfloat)blue, (GLfloat)alpha);
}

static void APIENTRY Color4dv(const GLdouble* v) {
  Color4d(v[0], v[1], v[2], v[3]);
}

static void APIENTRY Color4fv(const GLfloat* v) {
  Color4f(v[0], v[1], v[2], v[3]);
}

static void APIENTRY Color4i(GLint red, GLint green, GLint blue, GLint alpha) {
  Color4f(FromInt(red), FromInt(green), FromInt(blue), FromInt(alpha));
}

static void APIENTRY Color4iv(const GLint* v) {
  Color4i(v[0], v[1], v[2], v[3]);
}

static void APIENTRY Color4s(GLshort red, GLshort green, GLshort blue,
                             GLshort alpha) {
  Color4f(FromShort(red), FromShort(green), FromShort(blue), FromShort(alpha));
}

static void APIENTRY Color4sv(const GLshort* v) {
  Color4s(v[0], v[1], v[2], v[3]);
}

static void APIENTRY Color4ub(GLubyte red, GLubyte green, GLubyte blue,
                              GLubyte alpha) {
  Color4f(FromUbyte(red), FromUbyte(green), FromUbyte(blue), FromUbyte(alpha));
}

static void APIENTRY Color4ubv(const GLubyte* v) {
  Color4ub(v[0], v[1], v[2], v[3]);
}

static void APIENTRY Color4ui(GLuint red, GLuint green, GLuint blue,
                              GLuint alpha) {
  Color4f(FromUint(red), FromUint(green), FromUint(blue), FromUint(alpha));
}

static void APIENTRY Color4uiv(const GLuint* v) {
  Color4ui(v[0], v[1], v[2], v[3]);
}

static void APIENTRY Color4us(GLushort red, GLushort green, GLushort blue,
                              GLushort alpha) {
  Color4f(FromUshort(red), FromUshort(green), FromUshort(blue),
          FromUshort(alpha));
}

static void APIENTRY Color4usv(const GLushort* v) {
  Color4us(v[0], v[1], v[2], v[3]);
}

static void APIENTRY Indexf(GLfloat c) {
  Context* context = current;
  if (Record(context, kOpIndex, (Arg[]){{.f = c}}, 1)) {
    return;
  }
  context->state.current.index = c;
}

static void APIENTRY Indexd(GLdouble c) { Indexf((GLfloat)c); }

static void APIENTRY Indexdv(const GLdouble* c) { Indexf((GLfloat)*c); }

static void APIENTRY Indexfv(const GLfloat* c) { Indexf(*c); }

static void APIENTRY Indexi(GLint c) { Indexf((GLfloat)c); }

static void APIENTRY Indexiv(const GLint* c) { Indexf((GLfloat)*c); }

static void APIENTRY Indexs(GLshort c) { Indexf((GLfloat)c); }

static void APIENTRY Indexsv(const GLshort* c) { Indexf((GLfloat)*c); }

static void APIENTRY Indexub(GLubyte c) { Indexf((GLfloat)c); }

static void APIENTRY Indexubv(const GLubyte* c) { Indexf((GLfloat)*c); }

static void APIENTRY Normal3f(GLfloat nx, GLfloat ny, GLfloat nz) {
  Context* context = current;
  if (Record(context, kOpNormal, (Arg[]){{.f = nx}, {.f = ny}, {.f = nz}}, 3)) {
    return;
  }
  float* normal = context->state.current.normal;
  normal[0] = nx;
  normal[1] = ny;
  normal[2] = nz;
}

static void APIENTRY Normal3b(GLbyte nx, GLbyte ny, GLbyte nz) {
  Normal3f(FromByte(nx), FromByte(ny), FromByte(nz));
}

static void APIENTRY Normal3bv(const GLbyte* v) { Normal3b(v[0], v[1], v[2]); }

static void APIENTRY Normal3d(GLdouble nx, GLdouble ny, GLdouble nz) {
  Normal3f((GLfloat)nx, (GLfloat)ny, (GLfloat)nz);
}

static void APIENTRY Normal3dv(const GLdouble* v) {
  Normal3d(v[0], v[1], v[2]);
}

static void APIENTRY Normal3fv(const GLfloat* v) { Normal3f(v[0], v[1], v[2]); }

static void APIENTRY Normal3i(GLint nx, GLint ny, GLint nz) {
  Normal3f(FromInt(nx), FromInt(ny), FromInt(nz));
}

static void APIENTRY Normal3iv(const GLint* v) { Normal3i(v[0], v[1], v[2]); }

static void APIENTRY Normal3s(GLshort nx, GLshort ny, GLshort nz) {
  Normal3f(FromShort(nx), FromShort(ny), FromShort(nz));
}

static void APIENTRY Normal3sv(const GLshort* v) { Normal3s(v[0], v[1], v[2]); }

static void APIENTRY MultiTexCoord4fARB(GLenum texture, GLfloat s, GLfloat t,
                                        GLfloat r, GLfloat q) {
  Context* context = current;
  if (Record(context, kOpTexCoord,
             (Arg[]){{.u = texture}, {.f = s}, {.f = t}, {.f = r}, {.f = q}},
             5)) {
    return;
  }
  if (!IsTextureUnit(texture)) {
    SetError(context, GL_INVALID_ENUM);
    return;
  }
  float* texcoord = context->state.current.texcoord[texture - GL_TEXTURE0_ARB];
  texcoord[0] = s;
  texcoord[1] = t;
  texcoord[2] = r;
  texcoord[3] = q;
}

static void APIENTRY TexCoord4f(GLfloat s, GLfloat t, GLfloat r, GLfloat q) {
  MultiTexCoord4fARB(GL_TEXTURE0_ARB, s, t, r, q);
}

static void APIENTRY TexCoord1d(GLdouble s) {
  TexCoord4f((GLfloat)s, 0.0f, 0.0f, 1.0f);
}

static void APIENTRY TexCoord1dv(const GLdouble* v) { TexCoord1d(v[0]); }

static void APIENTRY TexCoord1f(GLfloat s) { TexCoord4f(s, 0.0f, 0.0f, 1.0f); }

static void APIENTRY TexCoord1fv(const GLfloat* v) { TexCoord1f(v[0]); }

static void APIENTRY TexCoord1i(GLint s) {
  TexCoord4f((GLfloat)s, 0.0f, 0.0f, 1.0f);
}

static void APIENTRY TexCoord1iv(const GLint* v) { TexCoord1i(v[0]); }

static void APIENTRY TexCoord1s(GLshort s) {
  TexCoord4f((GLfloat)s, 0.0f, 0.0f, 1.0f);
}

static void APIENTRY TexCoord1sv(const GLshort* v) { TexCoord1s(v[0]); }

static void APIENTRY TexCoord2d(GLdouble s, GLdouble t) {
  TexCoord4f((GLfloat)s, (GLfloat)t, 0.0f, 1.0f);
}

static void APIENTRY TexCoord2dv(const GLdouble* v) { TexCoord2d(v[0], v[1]); }

static void APIENTRY TexCoord2f(GLfloat s, GLfloat t) {
  TexCoord4f(s, t, 0.0f, 1.0f);
}

static void APIENTRY TexCoord2fv(const GLfloat* v) { TexCoord2f(v[0], v[1]); }

static void APIENTRY TexCoord2i(GLint s, GLint t) {
  TexCoord4f((GLfloat)s, (GLfloat)t, 0.0f, 1.0f);
}

static void APIENTRY TexCoord2iv(const GLint* v) { TexCoord2i(v[0], v[1]); }

static void APIENTRY TexCoord2s(GLshort s, GLshort t) {
  TexCoord4f((GLfloat)s, (GLfloat)t, 0.0f, 1.0f);
}

static void APIENTRY TexCoord2sv(const GLshort* v) { TexCoord2s(v[0], v[1]); }

static void APIENTRY TexCoord3d(GLdouble s, GLdouble t, GLdouble r) {
  TexCoord4f((GLfloat)s, (GLfloat)t, (GLfloat)r, 1.0f);
}

static void APIENTRY TexCoord3dv(const GLdouble* v) {
  TexCoord3d(v[0], v[1], v[2]);
}

static void APIENTRY TexCoord3f(GLfloat s, GLfloat t, GLfloat r) {
  TexCoord4f(s, t, r, 1.0f);
}

static void APIENTRY TexCoord3fv(const GLfloat* v) {
  TexCoord3f(v[0], v[1], v[2]);
}

static void APIENTRY TexCoord3i(GLint s, GLint t, GLint r) {
  TexCoord4f((GLfloat)s, (GLfloat)t, (GLfloat)r, 1.0f);
}

static void APIENTRY TexCoord3iv(const GLint* v) {
  TexCoord3i(v[0], v[1], v[2]);
}

static void APIENTRY TexCoord3s(GLshort s, GLshort t, GLshort r) {
  TexCoord4f((GLfloat)s, (GLfloat)t, (GLfloat)r, 1.0f);
}

static void APIENTRY TexCoord3sv(const GLshort* v) {
  TexCoord3s(v[0], v[1], v[2]);
}

static void APIENTRY TexCoord4d(GLdouble s, GLdouble t, GLdouble r,
                                GLdouble q) {
  TexCoord4f((GLfloat)s, (GLfloat)t, (GLfloat)r, (GLfloat)q);
}

static void APIENTRY TexCoord4dv(const GLdouble* v) {
  TexCoord4d(v[0], v[1], v[2], v[3]);
}

static void APIENTRY TexCoord4fv(const GLfloat* v) {
  TexCoord4f(v[0], v[1], v[2], v[3]);
}

static void APIENTRY TexCoord4i(GLint s, GLint t, GLint r, GLint q) {
  TexCoord4f((GLfloat)s, (GLfloat)t, (GLfloat)r, (GLfloat)q);
}

static void APIENTRY TexCoord4iv(const GLint* v) {
  TexCoord4i(v[0], v[1], v[2], v[3]);
}

static void APIENTRY TexCoord4s(GLshort s, GLshort t, GLshort r, GLshort q) {
  TexCoord4f((GLfloat)s, (GLfloat)t, (GLfloat)r, (GLfloat)q);
}

static void APIENTRY TexCoord4sv(const GLshort* v) {
  TexCoord4s(v[0], v[1], v[2], v[3]);
}

static void APIENTRY MultiTexCoord1dARB(GLenum texture, GLdouble s) {
  MultiTexCoord4fARB(texture, (GLfloat)s, 0.0f, 0.0f, 1.0f);
}

static void APIENTRY MultiTexCoord1dvARB(GLenum texture, const GLdouble* v) {
  MultiTexCoord1dARB(texture, v[0]);
}

static void APIENTRY MultiTexCoord1fARB(GLenum texture, GLfloat s) {
  MultiTexCoord4fARB(texture, s, 0.0f, 0.0f, 1.0f);
}

static void APIENTRY MultiTexCoord1fvARB(GLenum texture, const GLfloat* v) {
  MultiTexCoord1fARB(texture, v[0]);
}

static void APIENTRY MultiTexCoord1iARB(GLenum texture, GLint s) {
  MultiTexCoord4fARB(texture, (GLfloat)s, 0.0f, 0.0f, 1.0f);
}

static void APIENTRY MultiTexCoord1ivARB(GLenum texture, const GLint* v) {
  MultiTexCoord1iARB(texture, v[0]);
}

static void APIENTRY MultiTexCoord1sARB(GLenum texture, GLshort s) {
  MultiTexCoord4fARB(texture, (GLfloat)s, 0.0f, 0.0f, 1.0f);
}

static void APIENTRY MultiTexCoord1svARB(GLenum texture, const GLshort* v) {
  MultiTexCoord1sARB(texture, v[0]);
}

static void APIENTRY MultiTexCoord2dARB(GLenum texture, GLdouble s,
                                        GLdouble t) {
  MultiTexCoord4fARB(texture, (GLfloat)s, (GLfloat)t, 0.0f, 1.0f);
}

static void APIENTRY MultiTexCoord2dvARB(GLenum texture, const GLdouble* v) {
  MultiTexCoord2dARB(texture, v[0], v[1]);
}

static void APIENTRY MultiTexCoord2fARB(GLenum texture, GLfloat s, GLfloat t) {
  MultiTexCoord4fARB(texture, s, t, 0.0f, 1.0f);
}

static void APIENTRY MultiTexCoord2fvARB(GLenum texture, const GLfloat* v) {
  MultiTexCoord2fARB(texture, v[0], v[1]);
}

static void APIENTRY MultiTexCoord2iARB(GLenum texture, GLint s, GLint t) {
  MultiTexCoord4fARB(texture, (GLfloat)s, (GLfloat)t, 0.0f, 1.0f);
}

static void APIENTRY MultiTexCoord2ivARB(GLenum texture, const GLint* v) {
  MultiTexCoord2iARB(texture, v[0], v[1]);
}

static void APIENTRY MultiTexCoord2sARB(GLenum texture, GLshort s, GLshort t) {
  MultiTexCoord4fARB(texture, (GLfloat)s, (GLfloat)t, 0.0f, 1.0f);
}

static void APIENTRY MultiTexCoord2svARB(GLenum texture, const GLshort* v) {
  MultiTexCoord2sARB(texture, v[0], v[1]);
}

static void APIENTRY MultiTexCoord3dARB(GLenum texture, GLdouble s, GLdouble t,
                                        GLdouble r) {
  MultiTexCoord4fARB(texture, (GLfloat)s, (GLfloat)t, (GLfloat)r, 1.0f);
}

static void APIENTRY MultiTexCoord3dvARB(GLenum texture, const GLdouble* v) {
  MultiTexCoord3dARB(texture, v[0], v[1], v[2]);
}

static void APIENTRY MultiTexCoord3fARB(GLenum texture, GLfloat s, GLfloat t,
                                        GLfloat r) {
  MultiTexCoord4fARB(texture, s, t, r, 1.0f);
}

static void APIENTRY MultiTexCoord3fvARB(GLenum texture, const GLfloat* v) {
  MultiTexCoord3fARB(texture, v[0], v[1], v[2]);
}

static void APIENTRY MultiTexCoord3iARB(GLenum texture, GLint s, GLint t,
                                        GLint r) {
  MultiTexCoord4fARB(texture, (GLfloat)s, (GLfloat)t, (GLfloat)r, 1.0f);
}

static void APIENTRY MultiTexCoord3ivARB(GLenum texture, const GLint* v) {
  MultiTexCoord3iARB(texture, v[0], v[1], v[2]);
}

static void APIENTRY MultiTexCoord3sARB(GLenum texture, GLshort s, GLshort t,
                                        GLshort r) {
  MultiTexCoord4fARB(texture, (GLfloat)s, (GLfloat)t, (GLfloat)r, 1.0f);
}

static void APIENTRY MultiTexCoord3svARB(GLenum texture, const GLshort* v) {
  MultiTexCoord3sARB(texture, v[0], v[1], v[2]);
}

static void APIENTRY MultiTexCoord4dARB(GLenum texture, GLdouble s, GLdouble t,
                                        GLdouble r, GLdouble q) {
  MultiTexCoord4fARB(texture, (GLfloat)s, (GLfloat)t, (GLfloat)r, (GLfloat)q);
}

static void APIENTRY MultiTexCoord4dvARB(GLenum texture, const GLdouble* v) {
  MultiTexCoord4dARB(texture, v[0], v[1], v[2], v[3]);
}

static void APIENTRY MultiTexCoord4fvARB(GLenum texture, const GLfloat* v) {
  MultiTexCoord4fARB(texture, v[0], v[1], v[2], v[3]);
}

static void APIENTRY MultiTexCoord4iARB(GLenum texture, GLint s, GLint t,
                                        GLint r, GLint q) {
  MultiTexCoord4fARB(texture, (GLfloat)s, (GLfloat)t, (GLfloat)r, (GLfloat)q);
}

static void APIENTRY MultiTexCoord4ivARB(GLenum texture, const GLint* v) {
  MultiTexCoord4iARB(texture, v[0], v[1], v[2], v[3]);
}

static void APIENTRY MultiTexCoord4sARB(GLenum texture, GLshort s, GLshort t,
                                        GLshort r, GLshort q) {
  MultiTexCoord4fARB(texture, (GLfloat)s, (GLfloat)t, (GLfloat)r, (GLfloat)q);
}

static void APIENTRY MultiTexCoord4svARB(GLenum texture, const GLshort* v) {
  MultiTexCoord4sARB(texture, v[0], v[1], v[2], v[3]);
}

static void APIENTRY EdgeFlag(GLboolean flag) {
  Context* context = current;
  if (Record(context, kOpEdgeFlag, (Arg[]){{.u = flag}}, 1)) {
    return;
  }
  context->state.current.edge_flag = flag != GL_FALSE;
}

static void APIENTRY EdgeFlagv(const GLboolean* flag) { EdgeFlag(*flag); }

static void APIENTRY Rectf(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2) {
  Context* context = current;
  if (Record(context, kOpRect,
             (Arg[]){{.f = x1}, {.f = y1}, {.f = x2}, {.f = y2}}, 4) ||
      InsideBeginEnd(context)) {
    return;
  }
  BeginPrimitive(context, GL_POLYGON);
  EmitCurrentVertex(context, (const float[4]){x1, y1, 0.0f, 1.0f});
  EmitCurrentVertex(context, (const float[4]){x2, y1, 0.0f, 1.0f});
  EmitCurrentVertex(context, (const float[4]){x2, y2, 0.0f, 1.0f});
  EmitCurrentVertex(context, (const float[4]){x1, y2, 0.0f, 1.0f});
  EndPrimitive(context);
}

static void APIENTRY Rectd(GLdouble x1, GLdouble y1, GLdouble x2, GLdouble y2) {
  Rectf((GLfloat)x1, (GLfloat)y1, (GLfloat)x2, (GLfloat)y2);
}

static void APIENTRY Rectdv(const GLdouble* v1, const GLdouble* v2) {
  Rectd(v1[0], v1[1], v2[0], v2[1]);
}

static void APIENTRY Rectfv(const GLfloat* v1, const GLfloat* v2) {
  Rectf(v1[0], v1[1], v2[0], v2[1]);
}

static void APIENTRY Recti(GLint x1, GLint y1, GLint x2, GLint y2) {
  Rectf((GLfloat)x1, (GLfloat)y1, (GLfloat)x2, (GLfloat)y2);
}

static void APIENTRY Rectiv(const GLint* v1, const GLint* v2) {
  Recti(v1[0], v1[1], v2[0], v2[1]);
}

static void APIENTRY Rects(GLshort x1, GLshort y1, GLshort x2, GLshort y2) {
  Rectf((GLfloat)x1, (GLfloat)y1, (GLfloat)x2, (GLfloat)y2);
}

static void APIENTRY Rectsv(const GLshort* v1, const GLshort* v2) {
  Rects(v1[0], v1[1], v2[0], v2[1]);
}

static int TypeSize(GLenum type) {
  switch (type) {
    case GL_BYTE:
    case GL_UNSIGNED_BYTE:
      return 1;
    case GL_SHORT:
    case GL_UNSIGNED_SHORT:
      return 2;
    case GL_INT:
    case GL_UNSIGNED_INT:
    case GL_FLOAT:
      return 4;
    case GL_DOUBLE:
      return 8;
    default:
      return 0;
  }
}

static float ArrayComponent(const Array* array, int element, int component,
                            bool normalized) {
  int size = TypeSize(array->type);
  GLsizei stride = array->stride ? array->stride : array->size * size;
  const uint8_t* p = (const uint8_t*)array->pointer +
                     (ptrdiff_t)element * stride + component * size;
  switch (array->type) {
    case GL_BYTE: {
      GLbyte value;
      memcpy(&value, p, sizeof(value));
      return normalized ? FromByte(value) : (float)value;
    }
    case GL_UNSIGNED_BYTE:
      return normalized ? FromUbyte(*p) : (float)*p;
    case GL_SHORT: {
      GLshort value;
      memcpy(&value, p, sizeof(value));
      return normalized ? FromShort(value) : (float)value;
    }
    case GL_UNSIGNED_SHORT: {
      GLushort value;
      memcpy(&value, p, sizeof(value));
      return normalized ? FromUshort(value) : (float)value;
    }
    case GL_INT: {
      GLint value;
      memcpy(&value, p, sizeof(value));
      return normalized ? FromInt(value) : (float)value;
    }
    case GL_UNSIGNED_INT: {
      GLuint value;
      memcpy(&value, p, sizeof(value));
      return normalized ? FromUint(value) : (float)value;
    }
    case GL_FLOAT: {
      GLfloat value;
      memcpy(&value, p, sizeof(value));
      return value;
    }
    default: {
      GLdouble value;
      memcpy(&value, p, sizeof(value));
      return (float)value;
    }
  }
}

static void APIENTRY ArrayElement(GLint i) {
  const Array* arrays = current->client.arrays;
  if (arrays[kEdgeFlagArray].enabled) {
    GLsizei stride = arrays[kEdgeFlagArray].stride;
    EdgeFlag(((const GLboolean*)arrays[kEdgeFlagArray]
                  .pointer)[(ptrdiff_t)i * (stride ? stride : 1)]);
  }
  float values[4];
  for (int unit = 0; unit < kTextureUnits; unit++) {
    const Array* texcoords = &arrays[kTexCoordArray + unit];
    if (texcoords->enabled) {
      for (int c = 0; c < 4; c++) {
        values[c] = c < texcoords->size ? ArrayComponent(texcoords, i, c, false)
                                        : (c == 3 ? 1.0f : 0.0f);
      }
      MultiTexCoord4fvARB(GL_TEXTURE0_ARB + unit, values);
    }
  }
  const Array* colors = &arrays[kColorArray];
  if (colors->enabled) {
    for (int c = 0; c < 4; c++) {
      values[c] = c < colors->size ? ArrayComponent(colors, i, c, true) : 1.0f;
    }
    Color4fv(values);
  }
  if (arrays[kIndexArray].enabled) {
    Indexf(ArrayComponent(&arrays[kIndexArray], i, 0, false));
  }
  const Array* normals = &arrays[kNormalArray];
  if (normals->enabled) {
    Normal3f(ArrayComponent(normals, i, 0, true),
             ArrayComponent(normals, i, 1, true),
             ArrayComponent(normals, i, 2, true));
  }
  const Array* vertices = &arrays[kVertexArray];
  if (vertices->enabled) {
    for (int c = 0; c < 4; c++) {
      values[c] = c < vertices->size ? ArrayComponent(vertices, i, c, false)
                                     : (c == 3 ? 1.0f : 0.0f);
    }
    Vertex4fv(values);
  }
}

static void EmitArrays(GLenum mode, GLint first, GLsizei count, GLenum type,
                       const GLvoid* indices) {
  Begin(mode);
  for (GLsizei i = 0; i < count; i++) {
    if (type == GL_UNSIGNED_BYTE) {
      ArrayElement(((const GLubyte*)indices)[i]);
    } else if (type == GL_UNSIGNED_SHORT) {
      ArrayElement(((const GLushort*)indices)[i]);
    } else if (type == GL_UNSIGNED_INT) {
      ArrayElement((GLint)((const GLuint*)indices)[i]);
    } else {
      ArrayElement(first + i);
    }
  }
  End();
}

static void DrawArrayElements(GLenum mode, GLint first, GLsizei count,
                              GLenum type, const GLvoid* indices) {
  Context* context = current;
  if (Compiling(context)) {
    Record(context, kOpArrays, NULL, 0);
    if (context->inside) {
      context->list_mode = GL_COMPILE;
      EmitArrays(mode, first, count, type, indices);
      context->list_mode = GL_COMPILE_AND_EXECUTE;
    }
  }
  if (!InsideBeginEnd(context)) {
    EmitArrays(mode, first, count, type, indices);
  }
}

static void APIENTRY DrawArrays(GLenum mode, GLint first, GLsizei count) {
  Context* context = current;
  if (count < 0) {
    SetError(context, GL_INVALID_VALUE);
  } else if (mode > GL_POLYGON) {
    SetError(context, GL_INVALID_ENUM);
  } else {
    DrawArrayElements(mode, first, count, GL_NONE, NULL);
  }
}

static void APIENTRY DrawElements(GLenum mode, GLsizei count, GLenum type,
                                  const GLvoid* indices) {
  Context* context = current;
  if (count < 0) {
    SetError(context, GL_INVALID_VALUE);
  } else if (mode > GL_POLYGON ||
             (type != GL_UNSIGNED_BYTE && type != GL_UNSIGNED_SHORT &&
              type != GL_UNSIGNED_INT)) {
    SetError(context, GL_INVALID_ENUM);
  } else {
    DrawArrayElements(mode, 0, count, type, indices);
  }
}

static void SetArray(int index, bool valid_size, bool valid_type, GLint size,
                     GLenum type, GLsizei stride, const GLvoid* pointer) {
  Context* context = current;
  if (InsideBeginEnd(context)) {
    return;
  }
  if (!valid_size || stride < 0) {
    SetError(context, GL_INVALID_VALUE);
    return;
  }
  if (!valid_type) {
    SetError(context, GL_INVALID_ENUM);
    return;
  }
  Array* array = &context->client.arrays[index];
  array->size = size;
  array->type = type;
  array->stride = stride;
  array->pointer = pointer;
}

static bool IsArrayType(GLenum type, bool bytes, bool unsigned_types) {
  switch (type) {
    case GL_BYTE:
      return bytes;
    case GL_UNSIGNED_BYTE:
    case GL_UNSIGNED_SHORT:
    case GL_UNSIGNED_INT:
      return unsigned_types;
    case GL_SHORT:
    case GL_INT:
    case GL_FLOAT:
    case GL_DOUBLE:
      return true;
    default:
      return false;
  }
}

static void APIENTRY VertexPointer(GLint size, GLenum type, GLsizei stride,
                                   const GLvoid* pointer) {
  SetArray(kVertexArray, size >= 2 && size <= 4,
           IsArrayType(type, false, false), size, type, stride, pointer);
}

static void APIENTRY NormalPointer(GLenum type, GLsizei stride,
                                   const GLvoid* pointer) {
  SetArray(kNormalArray, true, IsArrayType(type, true, false), 3, type, stride,
           pointer);
}

static void APIENTRY ColorPointer(GLint size, GLenum type, GLsizei stride,
                                  const GLvoid* pointer) {
  SetArray(kColorArray, size >= 3 && size <= 4, IsArrayType(type, true, true),
           size, type, stride, pointer);
}

static void APIENTRY IndexPointer(GLenum type, GLsizei stride,
                                  const GLvoid* pointer) {
  SetArray(kIndexArray, true,
           type == GL_UNSIGNED_BYTE || IsArrayType(type, false, false), 1, type,
           stride, pointer);
}

static int TexCoordArray(const Context* context) {
  return kTexCoordArray + context->client.active_texture;
}

static void APIENTRY TexCoordPointer(GLint size, GLenum type, GLsizei stride,
                                     const GLvoid* pointer) {
  SetArray(TexCoordArray(current), size >= 1 && size <= 4,
           IsArrayType(type, false, false), size, type, stride, pointer);
}

static void APIENTRY EdgeFlagPointer(GLsizei stride, const GLvoid* pointer) {
  SetArray(kEdgeFlagArray, true, true, 1, GL_UNSIGNED_BYTE, stride, pointer);
}

static int ArrayIndex(const Context* context, GLenum array) {
  switch (array) {
    case GL_VERTEX_ARRAY:
      return kVertexArray;
    case GL_NORMAL_ARRAY:
      return kNormalArray;
    case GL_COLOR_ARRAY:
      return kColorArray;
    case GL_INDEX_ARRAY:
      return kIndexArray;
    case GL_TEXTURE_COORD_ARRAY:
      return TexCoordArray(context);
    case GL_EDGE_FLAG_ARRAY:
      return kEdgeFlagArray;
    default:
      return -1;
  }
}

static void SetClientState(GLenum array, bool enabled) {
  if (InsideBeginEnd(current)) {
    return;
  }
  int index = ArrayIndex(current, array);
  if (index < 0) {
    SetError(current, GL_INVALID_ENUM);
    return;
  }
  current->client.arrays[index].enabled = enabled;
}

static void APIENTRY EnableClientState(GLenum array) {
  SetClientState(array, true);
}

static void APIENTRY DisableClientState(GLenum array) {
  SetClientState(array, false);
}

static void APIENTRY ClientActiveTextureARB(GLenum texture) {
  Context* context = current;
  if (InsideBeginEnd(context)) {
    return;
  }
  if (!IsTextureUnit(texture)) {
    SetError(context, GL_INVALID_ENUM);
    return;
  }
  context->client.active_texture = (int)(texture - GL_TEXTURE0_ARB);
}

static void APIENTRY InterleavedArrays(GLenum format, GLsizei stride,
                                       const GLvoid* pointer) {
  static const struct {
    GLenum format;
    int texcoords;
    int colors;
    bool normals;
    int vertices;
    GLenum color_type;
    int color_offset;
    int normal_offset;
    int vertex_offset;
    int size;
  } kLayouts[] = {
      {GL_V2F, 0, 0, false, 2, 0, 0, 0, 0, 8},
      {GL_V3F, 0, 0, false, 3, 0, 0, 0, 0, 12},
      {GL_C4UB_V2F, 0, 4, false, 2, GL_UNSIGNED_BYTE, 0, 0, 4, 12},
      {GL_C4UB_V3F, 0, 4, false, 3, GL_UNSIGNED_BYTE, 0, 0, 4, 16},
      {GL_C3F_V3F, 0, 3, false, 3, GL_FLOAT, 0, 0, 12, 24},
      {GL_N3F_V3F, 0, 0, true, 3, 0, 0, 0, 12, 24},
      {GL_C4F_N3F_V3F, 0, 4, true, 3, GL_FLOAT, 0, 16, 28, 40},
      {GL_T2F_V3F, 2, 0, false, 3, 0, 0, 0, 8, 20},
      {GL_T4F_V4F, 4, 0, false, 4, 0, 0, 0, 16, 32},
      {GL_T2F_C4UB_V3F, 2, 4, false, 3, GL_UNSIGNED_BYTE, 8, 0, 12, 24},
      {GL_T2F_C3F_V3F, 2, 3, false, 3, GL_FLOAT, 8, 0, 20, 32},
      {GL_T2F_N3F_V3F, 2, 0, true, 3, 0, 0, 8, 20, 32},
      {GL_T2F_C4F_N3F_V3F, 2, 4, true, 3, GL_FLOAT, 8, 24, 36, 48},
      {GL_T4F_C4F_N3F_V4F, 4, 4, true, 4, GL_FLOAT, 16, 32, 44, 60},
  };
  Context* context = current;
  if (InsideBeginEnd(context)) {
    return;
  }
  if (stride < 0) {
    SetError(context, GL_INVALID_VALUE);
    return;
  }
  for (size_t i = 0; i < sizeof(kLayouts) / sizeof(kLayouts[0]); i++) {
    if (kLayouts[i].format != format) {
      continue;
    }
    const uint8_t* base = pointer;
    GLsizei step = stride ? stride : kLayouts[i].size;
    Array* arrays = context->client.arrays;
    arrays[kEdgeFlagArray].enabled = false;
    arrays[kIndexArray].enabled = false;
    arrays[TexCoordArray(context)].enabled = kLayouts[i].texcoords > 0;
    if (kLayouts[i].texcoords) {
      TexCoordPointer(kLayouts[i].texcoords, GL_FLOAT, step, base);
    }
    arrays[kColorArray].enabled = kLayouts[i].colors > 0;
    if (kLayouts[i].colors) {
      ColorPointer(kLayouts[i].colors, kLayouts[i].color_type, step,
                   base + kLayouts[i].color_offset);
    }
    arrays[kNormalArray].enabled = kLayouts[i].normals;
    if (kLayouts[i].normals) {
      NormalPointer(GL_FLOAT, step, base + kLayouts[i].normal_offset);
    }
    arrays[kVertexArray].enabled = true;
    VertexPointer(kLayouts[i].vertices, GL_FLOAT, step,
                  base + kLayouts[i].vertex_offset);
    return;
  }
  SetError(context, GL_INVALID_ENUM);
}

static void Bernstein(int order, float t, float basis[kMaxEvalOrder],
                      float derivative[kMaxEvalOrder]) {
  float lower[kMaxEvalOrder] = {1.0f};
  int degree = order - 1;
  for (int d = 1; d < degree; d++) {
    for (int i = d; i >= 0; i--) {
      lower[i] = (i < d ? (1.0f - t) * lower[i] : 0.0f) +
                 (i > 0 ? t * lower[i - 1] : 0.0f);
    }
  }
  for (int i = 0; i < order; i++) {
    float left = i > 0 ? lower[i - 1] : 0.0f;
    float right = i < degree ? lower[i] : 0.0f;
    basis[i] = degree == 0 ? 1.0f : (1.0f - t) * right + t * left;
    derivative[i] = (float)degree * (left - right);
  }
}

static void EvaluateMap1(const Map* map, int size, float u, float out[4]) {
  float basis[kMaxEvalOrder];
  float derivative[kMaxEvalOrder];
  Bernstein(map->order[0],
            (u - map->domain[0]) / (map->domain[1] - map->domain[0]), basis,
            derivative);
  for (int c = 0; c < size; c++) {
    out[c] = 0.0f;
    for (int i = 0; i < map->order[0]; i++) {
      out[c] += basis[i] * map->points[i * size + c];
    }
  }
}

static void EvaluateMap2(const Map* map, int size, float u, float v,
                         float out[4], float du[4], float dv[4]) {
  float basis_u[kMaxEvalOrder];
  float derivative_u[kMaxEvalOrder];
  float basis_v[kMaxEvalOrder];
  float derivative_v[kMaxEvalOrder];
  float range_u = map->domain[1] - map->domain[0];
  float range_v = map->domain[3] - map->domain[2];
  Bernstein(map->order[0], (u - map->domain[0]) / range_u, basis_u,
            derivative_u);
  Bernstein(map->order[1], (v - map->domain[2]) / range_v, basis_v,
            derivative_v);
  for (int c = 0; c < size; c++) {
    out[c] = 0.0f;
    du[c] = 0.0f;
    dv[c] = 0.0f;
    for (int i = 0; i < map->order[0]; i++) {
      for (int j = 0; j < map->order[1]; j++) {
        float point = map->points[(i * map->order[1] + j) * size + c];
        out[c] += basis_u[i] * basis_v[j] * point;
        du[c] += derivative_u[i] * basis_v[j] * point / range_u;
        dv[c] += basis_u[i] * derivative_v[j] * point / range_v;
      }
    }
  }
}

static int HighestMap(const bool enabled[kMaps], int low, int high) {
  for (int map = high; map >= low; map--) {
    if (enabled[map]) {
      return map;
    }
  }
  return -1;
}

static void EmitEvaluated(Context* context, const bool enabled[kMaps],
                          const float values[kMaps][4],
                          const float* normal_override) {
  int vertex_map = HighestMap(enabled, 7, 8);
  if (vertex_map < 0) {
    return;
  }
  const CurrentState* current_state = &context->state.current;
  const float* color = enabled[0] ? values[0] : current_state->color;
  const float* normal = normal_override ? normal_override
                        : enabled[2]    ? values[2]
                                        : current_state->normal;
  float texcoords[kTextureUnits][4];
  memcpy(texcoords, current_state->texcoord, sizeof(texcoords));
  int texcoord_map = HighestMap(enabled, 3, 6);
  if (texcoord_map >= 0) {
    for (int c = 0; c < 4; c++) {
      texcoords[0][c] = c < kMapSizes[texcoord_map] ? values[texcoord_map][c]
                                                    : (c == 3 ? 1.0f : 0.0f);
    }
  }
  float object[4] = {values[vertex_map][0], values[vertex_map][1],
                     values[vertex_map][2],
                     vertex_map == 8 ? values[vertex_map][3] : 1.0f};
  EmitVertex(context, object, color, normal, texcoords);
}

static void EvalCoord1(Context* context, float u) {
  const bool* enabled = context->state.eval.map1;
  float values[kMaps][4] = {{0}};
  for (int map = 0; map < kMaps; map++) {
    if (enabled[map]) {
      EvaluateMap1(&context->maps1[map], kMapSizes[map], u, values[map]);
    }
  }
  EmitEvaluated(context, enabled, values, NULL);
}

static void EvalCoord2(Context* context, float u, float v) {
  const bool* enabled = context->state.eval.map2;
  float values[kMaps][4] = {{0}};
  float du[kMaps][4] = {{0}};
  float dv[kMaps][4] = {{0}};
  for (int map = 0; map < kMaps; map++) {
    if (enabled[map]) {
      EvaluateMap2(&context->maps2[map], kMapSizes[map], u, v, values[map],
                   du[map], dv[map]);
    }
  }
  int vertex_map = HighestMap(enabled, 7, 8);
  if (!context->state.eval.auto_normal || vertex_map < 0) {
    EmitEvaluated(context, enabled, values, NULL);
    return;
  }
  float pu[3];
  float pv[3];
  for (int c = 0; c < 3; c++) {
    pu[c] = du[vertex_map][c];
    pv[c] = dv[vertex_map][c];
    if (vertex_map == 8) {
      float w = values[8][3];
      pu[c] = (pu[c] * w - values[8][c] * du[8][3]) / (w * w);
      pv[c] = (pv[c] * w - values[8][c] * dv[8][3]) / (w * w);
    }
  }
  float normal[3] = {pu[1] * pv[2] - pu[2] * pv[1],
                     pu[2] * pv[0] - pu[0] * pv[2],
                     pu[0] * pv[1] - pu[1] * pv[0]};
  Normalize3(normal);
  EmitEvaluated(context, enabled, values, normal);
}

static void APIENTRY EvalCoord1f(GLfloat u) {
  Context* context = current;
  if (Record(context, kOpEvalCoord1, (Arg[]){{.f = u}}, 1)) {
    return;
  }
  EvalCoord1(context, u);
}

static void APIENTRY EvalCoord1d(GLdouble u) { EvalCoord1f((GLfloat)u); }

static void APIENTRY EvalCoord1dv(const GLdouble* u) {
  EvalCoord1f((GLfloat)*u);
}

static void APIENTRY EvalCoord1fv(const GLfloat* u) { EvalCoord1f(*u); }

static void APIENTRY EvalCoord2f(GLfloat u, GLfloat v) {
  Context* context = current;
  if (Record(context, kOpEvalCoord2, (Arg[]){{.f = u}, {.f = v}}, 2)) {
    return;
  }
  EvalCoord2(context, u, v);
}

static void APIENTRY EvalCoord2d(GLdouble u, GLdouble v) {
  EvalCoord2f((GLfloat)u, (GLfloat)v);
}

static void APIENTRY EvalCoord2dv(const GLdouble* u) {
  EvalCoord2f((GLfloat)u[0], (GLfloat)u[1]);
}

static void APIENTRY EvalCoord2fv(const GLfloat* u) { EvalCoord2f(u[0], u[1]); }

static float GridValue(int i, int segments, float start, float end) {
  if (i == segments) {
    return end;
  }
  return start + (float)i * (end - start) / (float)segments;
}

static float GridU1(const Context* context, int i) {
  const EvalState* eval = &context->state.eval;
  return GridValue(i, eval->segments1, eval->domain1[0], eval->domain1[1]);
}

static float GridU2(const Context* context, int i) {
  const EvalState* eval = &context->state.eval;
  return GridValue(i, eval->segments2[0], eval->domain2[0], eval->domain2[1]);
}

static float GridV2(const Context* context, int j) {
  const EvalState* eval = &context->state.eval;
  return GridValue(j, eval->segments2[1], eval->domain2[2], eval->domain2[3]);
}

static void APIENTRY EvalPoint1(GLint i) {
  Context* context = current;
  if (Record(context, kOpEvalPoint1, (Arg[]){{.i = i}}, 1)) {
    return;
  }
  EvalCoord1(context, GridU1(context, i));
}

static void APIENTRY EvalPoint2(GLint i, GLint j) {
  Context* context = current;
  if (Record(context, kOpEvalPoint2, (Arg[]){{.i = i}, {.i = j}}, 2)) {
    return;
  }
  EvalCoord2(context, GridU2(context, i), GridV2(context, j));
}

static void APIENTRY EvalMesh1(GLenum mode, GLint i1, GLint i2) {
  Context* context = current;
  if (Record(context, kOpEvalMesh1, (Arg[]){{.u = mode}, {.i = i1}, {.i = i2}},
             3) ||
      InsideBeginEnd(context)) {
    return;
  }
  if (mode != GL_POINT && mode != GL_LINE) {
    SetError(context, GL_INVALID_ENUM);
    return;
  }
  BeginPrimitive(context, mode == GL_POINT ? GL_POINTS : GL_LINE_STRIP);
  for (int i = i1; i <= i2; i++) {
    EvalCoord1(context, GridU1(context, i));
  }
  EndPrimitive(context);
}

static void APIENTRY EvalMesh2(GLenum mode, GLint i1, GLint i2, GLint j1,
                               GLint j2) {
  Context* context = current;
  if (Record(context, kOpEvalMesh2,
             (Arg[]){{.u = mode}, {.i = i1}, {.i = i2}, {.i = j1}, {.i = j2}},
             5) ||
      InsideBeginEnd(context)) {
    return;
  }
  switch (mode) {
    case GL_FILL:
      for (int j = j1; j < j2; j++) {
        BeginPrimitive(context, GL_QUAD_STRIP);
        for (int i = i1; i <= i2; i++) {
          EvalCoord2(context, GridU2(context, i), GridV2(context, j));
          EvalCoord2(context, GridU2(context, i), GridV2(context, j + 1));
        }
        EndPrimitive(context);
      }
      break;
    case GL_LINE:
      for (int j = j1; j <= j2; j++) {
        BeginPrimitive(context, GL_LINE_STRIP);
        for (int i = i1; i <= i2; i++) {
          EvalCoord2(context, GridU2(context, i), GridV2(context, j));
        }
        EndPrimitive(context);
      }
      for (int i = i1; i <= i2; i++) {
        BeginPrimitive(context, GL_LINE_STRIP);
        for (int j = j1; j <= j2; j++) {
          EvalCoord2(context, GridU2(context, i), GridV2(context, j));
        }
        EndPrimitive(context);
      }
      break;
    case GL_POINT:
      BeginPrimitive(context, GL_POINTS);
      for (int j = j1; j <= j2; j++) {
        for (int i = i1; i <= i2; i++) {
          EvalCoord2(context, GridU2(context, i), GridV2(context, j));
        }
      }
      EndPrimitive(context);
      break;
    default:
      SetError(context, GL_INVALID_ENUM);
  }
}

static int MapIndex(GLenum target, GLenum first) {
  return target >= first && target < first + kMaps ? (int)(target - first) : -1;
}

static void SetMap(Context* context, int dimensions, GLenum target,
                   const float domain[4], const int stride[2],
                   const int order[2], const float* points) {
  if (InsideBeginEnd(context)) {
    return;
  }
  if (context->state.texture.active != 0) {
    SetError(context, GL_INVALID_OPERATION);
    return;
  }
  int index =
      MapIndex(target, dimensions == 1 ? GL_MAP1_COLOR_4 : GL_MAP2_COLOR_4);
  if (index < 0) {
    SetError(context, GL_INVALID_ENUM);
    return;
  }
  int size = kMapSizes[index];
  for (int d = 0; d < dimensions; d++) {
    if (domain[2 * d] == domain[2 * d + 1] || stride[d] < size ||
        order[d] < 1 || order[d] > kMaxEvalOrder) {
      SetError(context, GL_INVALID_VALUE);
      return;
    }
  }
  Map* map = dimensions == 1 ? &context->maps1[index] : &context->maps2[index];
  map->order[0] = order[0];
  map->order[1] = dimensions == 2 ? order[1] : 1;
  memcpy(map->domain, domain, sizeof(map->domain));
  for (int i = 0; i < map->order[0]; i++) {
    for (int j = 0; j < map->order[1]; j++) {
      for (int c = 0; c < size; c++) {
        map->points[(i * map->order[1] + j) * size + c] =
            points[i * stride[0] + j * (dimensions == 2 ? stride[1] : 0) + c];
      }
    }
  }
}

static float* PackMap(int dimensions, GLenum target, const int stride[2],
                      const int order[2], const GLfloat* floats,
                      const GLdouble* doubles) {
  int index =
      MapIndex(target, dimensions == 1 ? GL_MAP1_COLOR_4 : GL_MAP2_COLOR_4);
  if (index < 0) {
    return NULL;
  }
  int size = kMapSizes[index];
  int orders[2] = {order[0], dimensions == 2 ? order[1] : 1};
  if (orders[0] < 1 || orders[0] > kMaxEvalOrder || orders[1] < 1 ||
      orders[1] > kMaxEvalOrder || stride[0] < size ||
      (dimensions == 2 && stride[1] < size)) {
    return NULL;
  }
  float* packed =
      Allocate((size_t)(orders[0] * orders[1] * size) * sizeof(float));
  for (int i = 0; i < orders[0]; i++) {
    for (int j = 0; j < orders[1]; j++) {
      for (int c = 0; c < size; c++) {
        size_t source =
            (size_t)(i * stride[0] + j * (dimensions == 2 ? stride[1] : 0) + c);
        packed[(i * orders[1] + j) * size + c] =
            floats ? floats[source] : (float)doubles[source];
      }
    }
  }
  return packed;
}

static void Map1(GLenum target, float u1, float u2, GLint stride, GLint order,
                 const GLfloat* floats, const GLdouble* doubles) {
  Context* context = current;
  int strides[2] = {stride, 0};
  int orders[2] = {order, 1};
  float* packed = PackMap(1, target, strides, orders, floats, doubles);
  int index = MapIndex(target, GL_MAP1_COLOR_4);
  int packed_stride = packed ? kMapSizes[index] : 0;
  if (Compiling(context) &&
      RecordData(context, kOpMap1,
                 (Arg[]){{.u = target},
                         {.f = u1},
                         {.f = u2},
                         {.i = packed_stride},
                         {.i = order}},
                 5,
                 packed ? Duplicate(packed, (size_t)(order * packed_stride) *
                                                sizeof(float))
                        : NULL)) {
    free(packed);
    return;
  }
  int packed_strides[2] = {packed_stride, 0};
  SetMap(context, 1, target, (const float[4]){u1, u2, 0.0f, 1.0f},
         packed_strides, orders, packed);
  free(packed);
}

static void APIENTRY Map1d(GLenum target, GLdouble u1, GLdouble u2,
                           GLint stride, GLint order, const GLdouble* points) {
  Map1(target, (float)u1, (float)u2, stride, order, NULL, points);
}

static void APIENTRY Map1f(GLenum target, GLfloat u1, GLfloat u2, GLint stride,
                           GLint order, const GLfloat* points) {
  Map1(target, u1, u2, stride, order, points, NULL);
}

static void Map2(GLenum target, float u1, float u2, GLint ustride, GLint uorder,
                 float v1, float v2, GLint vstride, GLint vorder,
                 const GLfloat* floats, const GLdouble* doubles) {
  Context* context = current;
  int strides[2] = {ustride, vstride};
  int orders[2] = {uorder, vorder};
  float* packed = PackMap(2, target, strides, orders, floats, doubles);
  int index = MapIndex(target, GL_MAP2_COLOR_4);
  int size = packed ? kMapSizes[index] : 0;
  if (Compiling(context) &&
      RecordData(context, kOpMap2,
                 (Arg[]){{.u = target},
                         {.f = u1},
                         {.f = u2},
                         {.i = size * vorder},
                         {.i = uorder},
                         {.f = v1},
                         {.f = v2},
                         {.i = size},
                         {.i = vorder}},
                 9,
                 packed ? Duplicate(packed, (size_t)(uorder * vorder * size) *
                                                sizeof(float))
                        : NULL)) {
    free(packed);
    return;
  }
  SetMap(context, 2, target, (const float[4]){u1, u2, v1, v2},
         (const int[2]){size * vorder, size}, orders, packed);
  free(packed);
}

static void APIENTRY Map2d(GLenum target, GLdouble u1, GLdouble u2,
                           GLint ustride, GLint uorder, GLdouble v1,
                           GLdouble v2, GLint vstride, GLint vorder,
                           const GLdouble* points) {
  Map2(target, (float)u1, (float)u2, ustride, uorder, (float)v1, (float)v2,
       vstride, vorder, NULL, points);
}

static void APIENTRY Map2f(GLenum target, GLfloat u1, GLfloat u2, GLint ustride,
                           GLint uorder, GLfloat v1, GLfloat v2, GLint vstride,
                           GLint vorder, const GLfloat* points) {
  Map2(target, u1, u2, ustride, uorder, v1, v2, vstride, vorder, points, NULL);
}

static void APIENTRY MapGrid1f(GLint un, GLfloat u1, GLfloat u2) {
  Context* context = current;
  if (Record(context, kOpMapGrid1, (Arg[]){{.i = un}, {.f = u1}, {.f = u2}},
             3) ||
      InsideBeginEnd(context)) {
    return;
  }
  if (un <= 0) {
    SetError(context, GL_INVALID_VALUE);
    return;
  }
  EvalState* eval = &context->state.eval;
  eval->segments1 = un;
  eval->domain1[0] = u1;
  eval->domain1[1] = u2;
}

static void APIENTRY MapGrid1d(GLint un, GLdouble u1, GLdouble u2) {
  MapGrid1f(un, (GLfloat)u1, (GLfloat)u2);
}

static void APIENTRY MapGrid2f(GLint un, GLfloat u1, GLfloat u2, GLint vn,
                               GLfloat v1, GLfloat v2) {
  Context* context = current;
  if (Record(
          context, kOpMapGrid2,
          (Arg[]){
              {.i = un}, {.f = u1}, {.f = u2}, {.i = vn}, {.f = v1}, {.f = v2}},
          6) ||
      InsideBeginEnd(context)) {
    return;
  }
  if (un <= 0 || vn <= 0) {
    SetError(context, GL_INVALID_VALUE);
    return;
  }
  EvalState* eval = &context->state.eval;
  eval->segments2[0] = un;
  eval->segments2[1] = vn;
  eval->domain2[0] = u1;
  eval->domain2[1] = u2;
  eval->domain2[2] = v1;
  eval->domain2[3] = v2;
}

static void APIENTRY MapGrid2d(GLint un, GLdouble u1, GLdouble u2, GLint vn,
                               GLdouble v1, GLdouble v2) {
  MapGrid2f(un, (GLfloat)u1, (GLfloat)u2, vn, (GLfloat)v1, (GLfloat)v2);
}

static int StackDepth(int stack) {
  return stack == kModelview    ? kModelviewDepth
         : stack == kProjection ? kProjectionDepth
                                : kTextureDepth;
}

static void APIENTRY MatrixMode(GLenum mode) {
  Context* context = current;
  if (Record(context, kOpMatrixMode, (Arg[]){{.u = mode}}, 1) ||
      InsideBeginEnd(context)) {
    return;
  }
  if (mode != GL_MODELVIEW && mode != GL_PROJECTION && mode != GL_TEXTURE) {
    SetError(context, GL_INVALID_ENUM);
    return;
  }
  context->state.transform.matrix_mode = mode;
}

static void APIENTRY LoadIdentity(void) {
  Context* context = current;
  if (Record(context, kOpLoadIdentity, NULL, 0) || InsideBeginEnd(context)) {
    return;
  }
  memcpy(CurrentMatrix(context), kIdentity, sizeof(kIdentity));
}

static void APIENTRY LoadMatrixf(const GLfloat* m) {
  Context* context = current;
  if ((Compiling(context) && RecordData(context, kOpLoadMatrix, NULL, 0,
                                        Duplicate(m, 16 * sizeof(GLfloat)))) ||
      InsideBeginEnd(context)) {
    return;
  }
  memcpy(CurrentMatrix(context), m, 16 * sizeof(GLfloat));
}

static void APIENTRY LoadMatrixd(const GLdouble* m) {
  GLfloat matrix[16];
  for (int i = 0; i < 16; i++) {
    matrix[i] = (GLfloat)m[i];
  }
  LoadMatrixf(matrix);
}

static void APIENTRY MultMatrixf(const GLfloat* m) {
  Context* context = current;
  if ((Compiling(context) && RecordData(context, kOpMultMatrix, NULL, 0,
                                        Duplicate(m, 16 * sizeof(GLfloat)))) ||
      InsideBeginEnd(context)) {
    return;
  }
  MultMatrix(CurrentMatrix(context), m);
}

static void APIENTRY MultMatrixd(const GLdouble* m) {
  GLfloat matrix[16];
  for (int i = 0; i < 16; i++) {
    matrix[i] = (GLfloat)m[i];
  }
  MultMatrixf(matrix);
}

static void APIENTRY Rotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z) {
  Context* context = current;
  if (Record(context, kOpRotate,
             (Arg[]){{.f = angle}, {.f = x}, {.f = y}, {.f = z}}, 4) ||
      InsideBeginEnd(context)) {
    return;
  }
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
      CurrentMatrix(context),
      (const float[16]){x * x * t + c, y * x * t + z * s, x * z * t - y * s, 0,
                        x * y * t - z * s, y * y * t + c, y * z * t + x * s, 0,
                        x * z * t + y * s, y * z * t - x * s, z * z * t + c, 0,
                        0, 0, 0, 1});
}

static void APIENTRY Rotated(GLdouble angle, GLdouble x, GLdouble y,
                             GLdouble z) {
  Rotatef((GLfloat)angle, (GLfloat)x, (GLfloat)y, (GLfloat)z);
}

static void APIENTRY Scalef(GLfloat x, GLfloat y, GLfloat z) {
  Context* context = current;
  if (Record(context, kOpScale, (Arg[]){{.f = x}, {.f = y}, {.f = z}}, 3) ||
      InsideBeginEnd(context)) {
    return;
  }
  MultMatrix(CurrentMatrix(context),
             (const float[16]){x, 0, 0, 0, 0, y, 0, 0, 0, 0, z, 0, 0, 0, 0, 1});
}

static void APIENTRY Scaled(GLdouble x, GLdouble y, GLdouble z) {
  Scalef((GLfloat)x, (GLfloat)y, (GLfloat)z);
}

static void APIENTRY Translatef(GLfloat x, GLfloat y, GLfloat z) {
  Context* context = current;
  if (Record(context, kOpTranslate, (Arg[]){{.f = x}, {.f = y}, {.f = z}}, 3) ||
      InsideBeginEnd(context)) {
    return;
  }
  MultMatrix(CurrentMatrix(context),
             (const float[16]){1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, x, y, z, 1});
}

static void APIENTRY Translated(GLdouble x, GLdouble y, GLdouble z) {
  Translatef((GLfloat)x, (GLfloat)y, (GLfloat)z);
}

static void APIENTRY Frustum(GLdouble left, GLdouble right, GLdouble bottom,
                             GLdouble top, GLdouble z_near, GLdouble z_far) {
  Context* context = current;
  if (Record(context, kOpFrustum,
             (Arg[]){{.d = left},
                     {.d = right},
                     {.d = bottom},
                     {.d = top},
                     {.d = z_near},
                     {.d = z_far}},
             6) ||
      InsideBeginEnd(context)) {
    return;
  }
  if (z_near <= 0 || z_far <= 0 || left == right || bottom == top ||
      z_near == z_far) {
    SetError(context, GL_INVALID_VALUE);
    return;
  }
  double width = right - left;
  double height = top - bottom;
  double depth = z_far - z_near;
  MultMatrix(CurrentMatrix(context),
             (const float[16]){(float)(2 * z_near / width), 0, 0, 0, 0,
                               (float)(2 * z_near / height), 0, 0,
                               (float)((right + left) / width),
                               (float)((top + bottom) / height),
                               (float)(-(z_far + z_near) / depth), -1, 0, 0,
                               (float)(-2 * z_far * z_near / depth), 0});
}

static void APIENTRY Ortho(GLdouble left, GLdouble right, GLdouble bottom,
                           GLdouble top, GLdouble z_near, GLdouble z_far) {
  Context* context = current;
  if (Record(context, kOpOrtho,
             (Arg[]){{.d = left},
                     {.d = right},
                     {.d = bottom},
                     {.d = top},
                     {.d = z_near},
                     {.d = z_far}},
             6) ||
      InsideBeginEnd(context)) {
    return;
  }
  if (left == right || bottom == top || z_near == z_far) {
    SetError(context, GL_INVALID_VALUE);
    return;
  }
  double width = right - left;
  double height = top - bottom;
  double depth = z_far - z_near;
  MultMatrix(CurrentMatrix(context),
             (const float[16]){
                 (float)(2 / width), 0, 0, 0, 0, (float)(2 / height), 0, 0, 0,
                 0, (float)(-2 / depth), 0, (float)(-(right + left) / width),
                 (float)(-(top + bottom) / height),
                 (float)(-(z_far + z_near) / depth), 1});
}

static void APIENTRY PushMatrix(void) {
  Context* context = current;
  if (Record(context, kOpPushMatrix, NULL, 0) || InsideBeginEnd(context)) {
    return;
  }
  int stack = MatrixStack(context);
  int* top = &context->matrix_top[stack];
  if (*top + 1 >= StackDepth(stack)) {
    SetError(context, GL_STACK_OVERFLOW);
    return;
  }
  memcpy(context->matrices[stack][*top + 1], context->matrices[stack][*top],
         sizeof(kIdentity));
  (*top)++;
}

static void APIENTRY PopMatrix(void) {
  Context* context = current;
  if (Record(context, kOpPopMatrix, NULL, 0) || InsideBeginEnd(context)) {
    return;
  }
  int stack = MatrixStack(context);
  if (context->matrix_top[stack] == 0) {
    SetError(context, GL_STACK_UNDERFLOW);
    return;
  }
  context->matrix_top[stack]--;
  context->normal_matrix_stale = true;
}

static void APIENTRY Viewport(GLint x, GLint y, GLsizei width, GLsizei height) {
  Context* context = current;
  if (Record(context, kOpViewport,
             (Arg[]){{.i = x}, {.i = y}, {.i = width}, {.i = height}}, 4) ||
      InsideBeginEnd(context)) {
    return;
  }
  if (width < 0 || height < 0) {
    SetError(context, GL_INVALID_VALUE);
    return;
  }
  GLint* box = context->state.viewport.box;
  box[0] = x;
  box[1] = y;
  box[2] = MinInt(width, kMaxViewport);
  box[3] = MinInt(height, kMaxViewport);
}

static void APIENTRY DepthRange(GLclampd z_near, GLclampd z_far) {
  Context* context = current;
  if (Record(context, kOpDepthRange, (Arg[]){{.d = z_near}, {.d = z_far}}, 2) ||
      InsideBeginEnd(context)) {
    return;
  }
  context->state.viewport.depth_range[0] = Clamp01((float)z_near);
  context->state.viewport.depth_range[1] = Clamp01((float)z_far);
}

static void APIENTRY ClipPlane(GLenum plane, const GLdouble* equation) {
  Context* context = current;
  if (Record(context, kOpClipPlane,
             (Arg[]){{.u = plane},
                     {.d = equation[0]},
                     {.d = equation[1]},
                     {.d = equation[2]},
                     {.d = equation[3]}},
             5) ||
      InsideBeginEnd(context)) {
    return;
  }
  if (plane < GL_CLIP_PLANE0 || plane >= GL_CLIP_PLANE0 + kMaxClipPlanes) {
    SetError(context, GL_INVALID_ENUM);
    return;
  }
  float object[4] = {(float)equation[0], (float)equation[1], (float)equation[2],
                     (float)equation[3]};
  TransformPlane(context, object,
                 context->state.transform.planes[plane - GL_CLIP_PLANE0]);
}

static void APIENTRY TexGenfv(GLenum coord, GLenum pname,
                              const GLfloat* params) {
  Context* context = current;
  int count = pname == GL_OBJECT_PLANE || pname == GL_EYE_PLANE ? 4 : 1;
  Arg args[6] = {{.u = coord}, {.u = pname}};
  for (int i = 0; i < count; i++) {
    args[2 + i].f = params[i];
  }
  if (Record(context, kOpTexGen, args, 6) || InsideBeginEnd(context)) {
    return;
  }
  if (coord < GL_S || coord > GL_Q) {
    SetError(context, GL_INVALID_ENUM);
    return;
  }
  TexGen* gen = &ActiveUnit(context)->gen[coord - GL_S];
  switch (pname) {
    case GL_TEXTURE_GEN_MODE: {
      GLenum mode = (GLenum)params[0];
      bool sphere = mode == GL_SPHERE_MAP && coord <= GL_T;
      if (mode != GL_OBJECT_LINEAR && mode != GL_EYE_LINEAR && !sphere) {
        SetError(context, GL_INVALID_ENUM);
        return;
      }
      gen->mode = mode;
      break;
    }
    case GL_OBJECT_PLANE:
      memcpy(gen->object, params, sizeof(gen->object));
      break;
    case GL_EYE_PLANE:
      TransformPlane(context, params, gen->eye);
      break;
    default:
      SetError(context, GL_INVALID_ENUM);
  }
}

static void APIENTRY TexGendv(GLenum coord, GLenum pname,
                              const GLdouble* params) {
  int count = pname == GL_OBJECT_PLANE || pname == GL_EYE_PLANE ? 4 : 1;
  GLfloat values[4] = {0};
  for (int i = 0; i < count; i++) {
    values[i] = (GLfloat)params[i];
  }
  TexGenfv(coord, pname, values);
}

static void APIENTRY TexGeniv(GLenum coord, GLenum pname, const GLint* params) {
  int count = pname == GL_OBJECT_PLANE || pname == GL_EYE_PLANE ? 4 : 1;
  GLfloat values[4] = {0};
  for (int i = 0; i < count; i++) {
    values[i] = (GLfloat)params[i];
  }
  TexGenfv(coord, pname, values);
}

static void APIENTRY TexGenf(GLenum coord, GLenum pname, GLfloat param) {
  TexGenfv(coord, pname != GL_TEXTURE_GEN_MODE ? GL_NONE : pname, &param);
}

static void APIENTRY TexGend(GLenum coord, GLenum pname, GLdouble param) {
  TexGenf(coord, pname, (GLfloat)param);
}

static void APIENTRY TexGeni(GLenum coord, GLenum pname, GLint param) {
  TexGenf(coord, pname, (GLfloat)param);
}

static int MaterialCount(GLenum pname) {
  switch (pname) {
    case GL_AMBIENT:
    case GL_DIFFUSE:
    case GL_SPECULAR:
    case GL_EMISSION:
    case GL_AMBIENT_AND_DIFFUSE:
      return 4;
    case GL_SHININESS:
      return 1;
    case GL_COLOR_INDEXES:
      return 3;
    default:
      return 0;
  }
}

static bool IsFace(GLenum face) {
  return face == GL_FRONT || face == GL_BACK || face == GL_FRONT_AND_BACK;
}

static void APIENTRY Materialfv(GLenum face, GLenum pname,
                                const GLfloat* params) {
  Context* context = current;
  int count = MaterialCount(pname);
  Arg args[6] = {{.u = face}, {.u = pname}};
  for (int i = 0; i < count; i++) {
    args[2 + i].f = params[i];
  }
  if (Record(context, kOpMaterial, args, 6)) {
    return;
  }
  if (!IsFace(face) || count == 0) {
    SetError(context, GL_INVALID_ENUM);
    return;
  }
  if (pname == GL_SHININESS && (params[0] < 0.0f || params[0] > 128.0f)) {
    SetError(context, GL_INVALID_VALUE);
    return;
  }
  for (int side = kFront; side <= kBack; side++) {
    if (face != GL_FRONT_AND_BACK &&
        face != (side == kFront ? GL_FRONT : GL_BACK)) {
      continue;
    }
    Material* material = &context->state.lighting.material[side];
    switch (pname) {
      case GL_AMBIENT:
        memcpy(material->ambient, params, sizeof(material->ambient));
        break;
      case GL_DIFFUSE:
        memcpy(material->diffuse, params, sizeof(material->diffuse));
        break;
      case GL_SPECULAR:
        memcpy(material->specular, params, sizeof(material->specular));
        break;
      case GL_EMISSION:
        memcpy(material->emission, params, sizeof(material->emission));
        break;
      case GL_AMBIENT_AND_DIFFUSE:
        memcpy(material->ambient, params, sizeof(material->ambient));
        memcpy(material->diffuse, params, sizeof(material->diffuse));
        break;
      case GL_SHININESS:
        material->shininess = params[0];
        break;
      default:
        memcpy(material->indexes, params, sizeof(material->indexes));
    }
  }
  TrackColor(context);
}

static void APIENTRY Materialf(GLenum face, GLenum pname, GLfloat param) {
  Materialfv(face, pname != GL_SHININESS ? GL_NONE : pname, &param);
}

static void APIENTRY Materiali(GLenum face, GLenum pname, GLint param) {
  Materialf(face, pname, (GLfloat)param);
}

static void APIENTRY Materialiv(GLenum face, GLenum pname,
                                const GLint* params) {
  GLfloat values[4] = {0};
  int count = MaterialCount(pname);
  for (int i = 0; i < count; i++) {
    values[i] = count == 4 ? FromInt(params[i]) : (GLfloat)params[i];
  }
  Materialfv(face, pname, values);
}

static int LightCount(GLenum pname) {
  switch (pname) {
    case GL_AMBIENT:
    case GL_DIFFUSE:
    case GL_SPECULAR:
    case GL_POSITION:
      return 4;
    case GL_SPOT_DIRECTION:
      return 3;
    case GL_SPOT_EXPONENT:
    case GL_SPOT_CUTOFF:
    case GL_CONSTANT_ATTENUATION:
    case GL_LINEAR_ATTENUATION:
    case GL_QUADRATIC_ATTENUATION:
      return 1;
    default:
      return 0;
  }
}

static void APIENTRY Lightfv(GLenum light, GLenum pname,
                             const GLfloat* params) {
  Context* context = current;
  int count = LightCount(pname);
  Arg args[6] = {{.u = light}, {.u = pname}};
  for (int i = 0; i < count; i++) {
    args[2 + i].f = params[i];
  }
  if (Record(context, kOpLight, args, 6) || InsideBeginEnd(context)) {
    return;
  }
  if (light < GL_LIGHT0 || light >= GL_LIGHT0 + kMaxLights || count == 0) {
    SetError(context, GL_INVALID_ENUM);
    return;
  }
  float value = params[0];
  if ((pname == GL_SPOT_EXPONENT && (value < 0.0f || value > 128.0f)) ||
      (pname == GL_SPOT_CUTOFF &&
       (value < 0.0f || (value > 90.0f && value != 180.0f))) ||
      ((pname == GL_CONSTANT_ATTENUATION || pname == GL_LINEAR_ATTENUATION ||
        pname == GL_QUADRATIC_ATTENUATION) &&
       value < 0.0f)) {
    SetError(context, GL_INVALID_VALUE);
    return;
  }
  Light* target = &context->state.lighting.lights[light - GL_LIGHT0];
  const float* m = TopMatrix(context, kModelview);
  switch (pname) {
    case GL_AMBIENT:
      memcpy(target->ambient, params, sizeof(target->ambient));
      break;
    case GL_DIFFUSE:
      memcpy(target->diffuse, params, sizeof(target->diffuse));
      break;
    case GL_SPECULAR:
      memcpy(target->specular, params, sizeof(target->specular));
      break;
    case GL_POSITION:
      Transform(m, params, target->position);
      break;
    case GL_SPOT_DIRECTION: {
      float direction[4];
      Transform(m, (const float[4]){params[0], params[1], params[2], 0.0f},
                direction);
      memcpy(target->direction, direction, sizeof(target->direction));
      break;
    }
    case GL_SPOT_EXPONENT:
      target->exponent = value;
      break;
    case GL_SPOT_CUTOFF:
      target->cutoff = value;
      break;
    case GL_CONSTANT_ATTENUATION:
      target->attenuation[0] = value;
      break;
    case GL_LINEAR_ATTENUATION:
      target->attenuation[1] = value;
      break;
    default:
      target->attenuation[2] = value;
  }
}

static void APIENTRY Lightf(GLenum light, GLenum pname, GLfloat param) {
  Lightfv(light, LightCount(pname) != 1 ? GL_NONE : pname, &param);
}

static void APIENTRY Lighti(GLenum light, GLenum pname, GLint param) {
  Lightf(light, pname, (GLfloat)param);
}

static void APIENTRY Lightiv(GLenum light, GLenum pname, const GLint* params) {
  GLfloat values[4] = {0};
  bool color =
      pname == GL_AMBIENT || pname == GL_DIFFUSE || pname == GL_SPECULAR;
  for (int i = 0; i < LightCount(pname); i++) {
    values[i] = color ? FromInt(params[i]) : (GLfloat)params[i];
  }
  Lightfv(light, pname, values);
}

static void APIENTRY LightModelfv(GLenum pname, const GLfloat* params) {
  Context* context = current;
  int count = pname == GL_LIGHT_MODEL_AMBIENT ? 4 : 1;
  Arg args[5] = {{.u = pname}};
  for (int i = 0; i < count; i++) {
    args[1 + i].f = params[i];
  }
  if (Record(context, kOpLightModel, args, 5) || InsideBeginEnd(context)) {
    return;
  }
  LightingState* lighting = &context->state.lighting;
  switch (pname) {
    case GL_LIGHT_MODEL_AMBIENT:
      memcpy(lighting->model_ambient, params, sizeof(lighting->model_ambient));
      break;
    case GL_LIGHT_MODEL_LOCAL_VIEWER:
      lighting->local_viewer = params[0] != 0.0f;
      break;
    case GL_LIGHT_MODEL_TWO_SIDE:
      lighting->two_side = params[0] != 0.0f;
      break;
    default:
      SetError(context, GL_INVALID_ENUM);
  }
}

static void APIENTRY LightModelf(GLenum pname, GLfloat param) {
  LightModelfv(pname == GL_LIGHT_MODEL_AMBIENT ? GL_NONE : pname, &param);
}

static void APIENTRY LightModeli(GLenum pname, GLint param) {
  LightModelf(pname, (GLfloat)param);
}

static void APIENTRY LightModeliv(GLenum pname, const GLint* params) {
  GLfloat values[4] = {0};
  if (pname == GL_LIGHT_MODEL_AMBIENT) {
    for (int i = 0; i < 4; i++) {
      values[i] = FromInt(params[i]);
    }
  } else {
    values[0] = (GLfloat)params[0];
  }
  LightModelfv(pname, values);
}

static void APIENTRY ColorMaterial(GLenum face, GLenum mode) {
  Context* context = current;
  if (Record(context, kOpColorMaterial, (Arg[]){{.u = face}, {.u = mode}}, 2) ||
      InsideBeginEnd(context)) {
    return;
  }
  if (!IsFace(face) ||
      (mode != GL_EMISSION && mode != GL_AMBIENT && mode != GL_DIFFUSE &&
       mode != GL_SPECULAR && mode != GL_AMBIENT_AND_DIFFUSE)) {
    SetError(context, GL_INVALID_ENUM);
    return;
  }
  context->state.lighting.color_material_face = face;
  context->state.lighting.color_material_mode = mode;
  if (context->state.lighting.color_material) {
    TrackColor(context);
  }
}

static void APIENTRY ShadeModel(GLenum mode) {
  Context* context = current;
  if (Record(context, kOpShadeModel, (Arg[]){{.u = mode}}, 1) ||
      InsideBeginEnd(context)) {
    return;
  }
  if (mode != GL_FLAT && mode != GL_SMOOTH) {
    SetError(context, GL_INVALID_ENUM);
    return;
  }
  context->state.lighting.shade_model = mode;
}

static void APIENTRY Fogfv(GLenum pname, const GLfloat* params) {
  Context* context = current;
  int count = pname == GL_FOG_COLOR ? 4 : 1;
  Arg args[5] = {{.u = pname}};
  for (int i = 0; i < count; i++) {
    args[1 + i].f = params[i];
  }
  if (Record(context, kOpFog, args, 5) || InsideBeginEnd(context)) {
    return;
  }
  FogState* fog = &context->state.fog;
  switch (pname) {
    case GL_FOG_MODE: {
      GLenum mode = (GLenum)params[0];
      if (mode != GL_LINEAR && mode != GL_EXP && mode != GL_EXP2) {
        SetError(context, GL_INVALID_ENUM);
        return;
      }
      fog->mode = mode;
      break;
    }
    case GL_FOG_DENSITY:
      if (params[0] < 0.0f) {
        SetError(context, GL_INVALID_VALUE);
        return;
      }
      fog->density = params[0];
      break;
    case GL_FOG_START:
      fog->start = params[0];
      break;
    case GL_FOG_END:
      fog->end = params[0];
      break;
    case GL_FOG_INDEX:
      fog->index = params[0];
      break;
    case GL_FOG_COLOR:
      for (int i = 0; i < 4; i++) {
        fog->color[i] = Clamp01(params[i]);
      }
      break;
    default:
      SetError(context, GL_INVALID_ENUM);
  }
}

static void APIENTRY Fogf(GLenum pname, GLfloat param) {
  Fogfv(pname == GL_FOG_COLOR ? GL_NONE : pname, &param);
}

static void APIENTRY Fogi(GLenum pname, GLint param) {
  Fogf(pname, (GLfloat)param);
}

static void APIENTRY Fogiv(GLenum pname, const GLint* params) {
  GLfloat values[4] = {0};
  if (pname == GL_FOG_COLOR) {
    for (int i = 0; i < 4; i++) {
      values[i] = FromInt(params[i]);
    }
  } else {
    values[0] = (GLfloat)params[0];
  }
  Fogfv(pname, values);
}

static void SetRasterPosition(Context* context, const float object[4]) {
  CurrentState* current_state = &context->state.current;
  Vertex vertex;
  ProcessVertex(context, object, current_state->color, current_state->normal,
                current_state->texcoord, &vertex);
  RasterPosition* raster = &current_state->raster;
  raster->valid = InsideClipVolume(context, &vertex) && vertex.clip[3] > 0.0f;
  if (!raster->valid) {
    return;
  }
  Corner corner;
  ToCorner(context, &vertex, kFront, &corner);
  raster->window[0] = corner.x;
  raster->window[1] = corner.y;
  raster->window[2] = corner.z;
  raster->window[3] = vertex.clip[3];
  raster->distance = fabsf(vertex.fog);
  memcpy(raster->color, vertex.color[kFront], sizeof(raster->color));
  memcpy(raster->texcoord, vertex.texcoord, sizeof(raster->texcoord));
  if (context->render_mode == GL_SELECT) {
    SelectHit(context, corner.z);
  }
}

static void APIENTRY RasterPos4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
  Context* context = current;
  if (Record(context, kOpRasterPos,
             (Arg[]){{.f = x}, {.f = y}, {.f = z}, {.f = w}}, 4) ||
      InsideBeginEnd(context)) {
    return;
  }
  SetRasterPosition(context, (const float[4]){x, y, z, w});
}

static void APIENTRY RasterPos2d(GLdouble x, GLdouble y) {
  RasterPos4f((GLfloat)x, (GLfloat)y, 0.0f, 1.0f);
}

static void APIENTRY RasterPos2dv(const GLdouble* v) {
  RasterPos2d(v[0], v[1]);
}

static void APIENTRY RasterPos2f(GLfloat x, GLfloat y) {
  RasterPos4f(x, y, 0.0f, 1.0f);
}

static void APIENTRY RasterPos2fv(const GLfloat* v) { RasterPos2f(v[0], v[1]); }

static void APIENTRY RasterPos2i(GLint x, GLint y) {
  RasterPos4f((GLfloat)x, (GLfloat)y, 0.0f, 1.0f);
}

static void APIENTRY RasterPos2iv(const GLint* v) { RasterPos2i(v[0], v[1]); }

static void APIENTRY RasterPos2s(GLshort x, GLshort y) {
  RasterPos4f((GLfloat)x, (GLfloat)y, 0.0f, 1.0f);
}

static void APIENTRY RasterPos2sv(const GLshort* v) { RasterPos2s(v[0], v[1]); }

static void APIENTRY RasterPos3d(GLdouble x, GLdouble y, GLdouble z) {
  RasterPos4f((GLfloat)x, (GLfloat)y, (GLfloat)z, 1.0f);
}

static void APIENTRY RasterPos3dv(const GLdouble* v) {
  RasterPos3d(v[0], v[1], v[2]);
}

static void APIENTRY RasterPos3f(GLfloat x, GLfloat y, GLfloat z) {
  RasterPos4f(x, y, z, 1.0f);
}

static void APIENTRY RasterPos3fv(const GLfloat* v) {
  RasterPos3f(v[0], v[1], v[2]);
}

static void APIENTRY RasterPos3i(GLint x, GLint y, GLint z) {
  RasterPos4f((GLfloat)x, (GLfloat)y, (GLfloat)z, 1.0f);
}

static void APIENTRY RasterPos3iv(const GLint* v) {
  RasterPos3i(v[0], v[1], v[2]);
}

static void APIENTRY RasterPos3s(GLshort x, GLshort y, GLshort z) {
  RasterPos4f((GLfloat)x, (GLfloat)y, (GLfloat)z, 1.0f);
}

static void APIENTRY RasterPos3sv(const GLshort* v) {
  RasterPos3s(v[0], v[1], v[2]);
}

static void APIENTRY RasterPos4d(GLdouble x, GLdouble y, GLdouble z,
                                 GLdouble w) {
  RasterPos4f((GLfloat)x, (GLfloat)y, (GLfloat)z, (GLfloat)w);
}

static void APIENTRY RasterPos4dv(const GLdouble* v) {
  RasterPos4d(v[0], v[1], v[2], v[3]);
}

static void APIENTRY RasterPos4fv(const GLfloat* v) {
  RasterPos4f(v[0], v[1], v[2], v[3]);
}

static void APIENTRY RasterPos4i(GLint x, GLint y, GLint z, GLint w) {
  RasterPos4f((GLfloat)x, (GLfloat)y, (GLfloat)z, (GLfloat)w);
}

static void APIENTRY RasterPos4iv(const GLint* v) {
  RasterPos4i(v[0], v[1], v[2], v[3]);
}

static void APIENTRY RasterPos4s(GLshort x, GLshort y, GLshort z, GLshort w) {
  RasterPos4f((GLfloat)x, (GLfloat)y, (GLfloat)z, (GLfloat)w);
}

static void APIENTRY RasterPos4sv(const GLshort* v) {
  RasterPos4s(v[0], v[1], v[2], v[3]);
}

static bool* Capability(State* state, GLenum cap) {
  if (cap >= GL_CLIP_PLANE0 && cap < GL_CLIP_PLANE0 + kMaxClipPlanes) {
    return &state->transform.clip[cap - GL_CLIP_PLANE0];
  }
  if (cap >= GL_LIGHT0 && cap < GL_LIGHT0 + kMaxLights) {
    return &state->lighting.lights[cap - GL_LIGHT0].enabled;
  }
  if (MapIndex(cap, GL_MAP1_COLOR_4) >= 0) {
    return &state->eval.map1[MapIndex(cap, GL_MAP1_COLOR_4)];
  }
  if (MapIndex(cap, GL_MAP2_COLOR_4) >= 0) {
    return &state->eval.map2[MapIndex(cap, GL_MAP2_COLOR_4)];
  }
  TextureUnit* unit = &state->texture.units[state->texture.active];
  if (cap >= GL_TEXTURE_GEN_S && cap <= GL_TEXTURE_GEN_Q) {
    return &unit->gen[cap - GL_TEXTURE_GEN_S].enabled;
  }
  switch (cap) {
    case GL_ALPHA_TEST:
      return &state->color.alpha_test;
    case GL_AUTO_NORMAL:
      return &state->eval.auto_normal;
    case GL_BLEND:
      return &state->color.blend;
    case GL_COLOR_LOGIC_OP:
      return &state->color.color_logic_op;
    case GL_COLOR_MATERIAL:
      return &state->lighting.color_material;
    case GL_CULL_FACE:
      return &state->polygon.cull;
    case GL_DEPTH_TEST:
      return &state->depth.test;
    case GL_DITHER:
      return &state->color.dither;
    case GL_FOG:
      return &state->fog.enabled;
    case GL_INDEX_LOGIC_OP:
      return &state->color.index_logic_op;
    case GL_LIGHTING:
      return &state->lighting.enabled;
    case GL_LINE_SMOOTH:
      return &state->line.smooth;
    case GL_LINE_STIPPLE:
      return &state->line.stipple;
    case GL_NORMALIZE:
      return &state->transform.normalize;
    case GL_POINT_SMOOTH:
      return &state->point.smooth;
    case GL_POLYGON_OFFSET_FILL:
      return &state->polygon.offset_fill;
    case GL_POLYGON_OFFSET_LINE:
      return &state->polygon.offset_line;
    case GL_POLYGON_OFFSET_POINT:
      return &state->polygon.offset_point;
    case GL_POLYGON_SMOOTH:
      return &state->polygon.smooth;
    case GL_POLYGON_STIPPLE:
      return &state->polygon.stipple;
    case GL_SCISSOR_TEST:
      return &state->scissor.test;
    case GL_STENCIL_TEST:
      return &state->stencil.test;
    case GL_TEXTURE_1D:
      return &unit->enabled[kTexture1D];
    case GL_TEXTURE_2D:
      return &unit->enabled[kTexture2D];
    default:
      return NULL;
  }
}

static void SetCapability(Op op, GLenum cap, bool value) {
  Context* context = current;
  if (ArrayIndex(context, cap) >= 0) {
    SetClientState(cap, value);
    return;
  }
  if (Record(context, op, (Arg[]){{.u = cap}}, 1) || InsideBeginEnd(context)) {
    return;
  }
  bool* flag = Capability(&context->state, cap);
  if (!flag) {
    SetError(context, GL_INVALID_ENUM);
    return;
  }
  *flag = value;
  if (cap == GL_COLOR_MATERIAL && value) {
    TrackColor(context);
  }
}

static void APIENTRY Enable(GLenum cap) { SetCapability(kOpEnable, cap, true); }

static void APIENTRY Disable(GLenum cap) {
  SetCapability(kOpDisable, cap, false);
}

static void APIENTRY CullFace(GLenum mode) {
  Context* context = current;
  if (Record(context, kOpCullFace, (Arg[]){{.u = mode}}, 1) ||
      InsideBeginEnd(context)) {
    return;
  }
  if (!IsFace(mode)) {
    SetError(context, GL_INVALID_ENUM);
    return;
  }
  context->state.polygon.cull_mode = mode;
}

static void APIENTRY FrontFace(GLenum mode) {
  Context* context = current;
  if (Record(context, kOpFrontFace, (Arg[]){{.u = mode}}, 1) ||
      InsideBeginEnd(context)) {
    return;
  }
  if (mode != GL_CW && mode != GL_CCW) {
    SetError(context, GL_INVALID_ENUM);
    return;
  }
  context->state.polygon.front_face = mode;
}

static void APIENTRY PolygonMode(GLenum face, GLenum mode) {
  Context* context = current;
  if (Record(context, kOpPolygonMode, (Arg[]){{.u = face}, {.u = mode}}, 2) ||
      InsideBeginEnd(context)) {
    return;
  }
  if (!IsFace(face) ||
      (mode != GL_POINT && mode != GL_LINE && mode != GL_FILL)) {
    SetError(context, GL_INVALID_ENUM);
    return;
  }
  if (face != GL_BACK) {
    context->state.polygon.mode[0] = mode;
  }
  if (face != GL_FRONT) {
    context->state.polygon.mode[1] = mode;
  }
}

static void APIENTRY PolygonOffset(GLfloat factor, GLfloat units) {
  Context* context = current;
  if (Record(context, kOpPolygonOffset, (Arg[]){{.f = factor}, {.f = units}},
             2) ||
      InsideBeginEnd(context)) {
    return;
  }
  context->state.polygon.offset_factor = factor;
  context->state.polygon.offset_units = units;
}

static void APIENTRY LineWidth(GLfloat width) {
  Context* context = current;
  if (Record(context, kOpLineWidth, (Arg[]){{.f = width}}, 1) ||
      InsideBeginEnd(context)) {
    return;
  }
  if (width <= 0.0f) {
    SetError(context, GL_INVALID_VALUE);
    return;
  }
  context->state.line.width = width;
}

static void APIENTRY LineStipple(GLint factor, GLushort pattern) {
  Context* context = current;
  if (Record(context, kOpLineStipple, (Arg[]){{.i = factor}, {.u = pattern}},
             2) ||
      InsideBeginEnd(context)) {
    return;
  }
  context->state.line.repeat = ClampInt(factor, 1, 256);
  context->state.line.pattern = pattern;
}

static void APIENTRY PointSize(GLfloat size) {
  Context* context = current;
  if (Record(context, kOpPointSize, (Arg[]){{.f = size}}, 1) ||
      InsideBeginEnd(context)) {
    return;
  }
  if (size <= 0.0f) {
    SetError(context, GL_INVALID_VALUE);
    return;
  }
  context->state.point.size = size;
}

static void APIENTRY Hint(GLenum target, GLenum mode) {
  Context* context = current;
  if (Record(context, kOpHint, (Arg[]){{.u = target}, {.u = mode}}, 2) ||
      InsideBeginEnd(context)) {
    return;
  }
  HintState* hint = &context->state.hint;
  GLenum* slot = target == GL_PERSPECTIVE_CORRECTION_HINT ? &hint->perspective
                 : target == GL_POINT_SMOOTH_HINT         ? &hint->point
                 : target == GL_LINE_SMOOTH_HINT          ? &hint->line
                 : target == GL_POLYGON_SMOOTH_HINT       ? &hint->polygon
                 : target == GL_FOG_HINT                  ? &hint->fog
                                                          : NULL;
  if (!slot ||
      (mode != GL_FASTEST && mode != GL_NICEST && mode != GL_DONT_CARE)) {
    SetError(context, GL_INVALID_ENUM);
    return;
  }
  *slot = mode;
}

static int FormatSize(GLenum format) {
  switch (format) {
    case GL_COLOR_INDEX:
    case GL_STENCIL_INDEX:
    case GL_DEPTH_COMPONENT:
    case GL_RED:
    case GL_GREEN:
    case GL_BLUE:
    case GL_ALPHA:
    case GL_LUMINANCE:
      return 1;
    case GL_LUMINANCE_ALPHA:
      return 2;
    case GL_RGB:
      return 3;
    case GL_RGBA:
      return 4;
    default:
      return 0;
  }
}

static bool IsPixelType(GLenum type) {
  return type == GL_BITMAP || (TypeSize(type) > 0 && type != GL_DOUBLE);
}

static size_t RowBytes(const PixelStore* store, int width, int components,
                       GLenum type) {
  size_t length = (size_t)(store->row_length > 0 ? store->row_length : width);
  size_t alignment = (size_t)store->alignment;
  if (type == GL_BITMAP) {
    return (length + 8 * alignment - 1) / (8 * alignment) * alignment;
  }
  size_t size = (size_t)TypeSize(type);
  size_t bytes = length * (size_t)components * size;
  return size >= alignment ? bytes
                           : (bytes + alignment - 1) / alignment * alignment;
}

static size_t GroupOffset(const PixelStore* store, int width, int components,
                          GLenum type, int i, int j) {
  return (size_t)(store->skip_rows + j) *
             RowBytes(store, width, components, type) +
         (size_t)(store->skip_pixels + i) * (size_t)components *
             (size_t)TypeSize(type);
}

static size_t BitOffset(const PixelStore* store, int width, int i, int j,
                        int* shift) {
  int column = store->skip_pixels + i;
  *shift = store->lsb_first ? column % 8 : 7 - column % 8;
  return (size_t)(store->skip_rows + j) * RowBytes(store, width, 1, GL_BITMAP) +
         (size_t)(column / 8);
}

static bool ReadBit(const PixelStore* store, const void* data, int width, int i,
                    int j) {
  int shift;
  size_t offset = BitOffset(store, width, i, j, &shift);
  return (((const uint8_t*)data)[offset] >> shift) & 1;
}

static void WriteBit(const PixelStore* store, void* data, int width, int i,
                     int j, bool value) {
  int shift;
  uint8_t* byte = (uint8_t*)data + BitOffset(store, width, i, j, &shift);
  *byte = (uint8_t)(value ? *byte | (1 << shift) : *byte & ~(1 << shift));
}

static void CopyElement(const uint8_t* source, uint8_t* destination, int size,
                        bool swap) {
  for (int i = 0; i < size; i++) {
    destination[i] = source[swap ? size - 1 - i : i];
  }
}

static double ReadElement(const uint8_t* p, GLenum type, bool swap) {
  uint8_t bytes[4];
  CopyElement(p, bytes, TypeSize(type), swap);
  switch (type) {
    case GL_UNSIGNED_BYTE:
      return bytes[0];
    case GL_BYTE:
      return (int8_t)bytes[0];
    case GL_UNSIGNED_SHORT: {
      uint16_t value;
      memcpy(&value, bytes, sizeof(value));
      return value;
    }
    case GL_SHORT: {
      int16_t value;
      memcpy(&value, bytes, sizeof(value));
      return value;
    }
    case GL_UNSIGNED_INT: {
      uint32_t value;
      memcpy(&value, bytes, sizeof(value));
      return value;
    }
    case GL_INT: {
      int32_t value;
      memcpy(&value, bytes, sizeof(value));
      return value;
    }
    default: {
      float value;
      memcpy(&value, bytes, sizeof(value));
      return value;
    }
  }
}

static double RoundClamp(double value, double low, double high) {
  return fmin(fmax(round(value), low), high);
}

static void WriteElement(uint8_t* p, GLenum type, bool swap, double value) {
  uint8_t bytes[4];
  switch (type) {
    case GL_UNSIGNED_BYTE:
      bytes[0] = (uint8_t)RoundClamp(value, 0, 255);
      break;
    case GL_BYTE:
      bytes[0] = (uint8_t)(int8_t)RoundClamp(value, -128, 127);
      break;
    case GL_UNSIGNED_SHORT: {
      uint16_t element = (uint16_t)RoundClamp(value, 0, 65535);
      memcpy(bytes, &element, sizeof(element));
      break;
    }
    case GL_SHORT: {
      int16_t element = (int16_t)RoundClamp(value, -32768, 32767);
      memcpy(bytes, &element, sizeof(element));
      break;
    }
    case GL_UNSIGNED_INT: {
      uint32_t element = (uint32_t)RoundClamp(value, 0, kUintMax);
      memcpy(bytes, &element, sizeof(element));
      break;
    }
    case GL_INT: {
      int32_t element = (int32_t)RoundClamp(value, -2147483648.0, 2147483647.0);
      memcpy(bytes, &element, sizeof(element));
      break;
    }
    default: {
      float element = (float)value;
      memcpy(bytes, &element, sizeof(element));
    }
  }
  CopyElement(bytes, p, TypeSize(type), swap);
}

static double NormalizeElement(GLenum type, double c) {
  switch (type) {
    case GL_UNSIGNED_BYTE:
      return c / 255.0;
    case GL_BYTE:
      return (2.0 * c + 1.0) / 255.0;
    case GL_UNSIGNED_SHORT:
      return c / 65535.0;
    case GL_SHORT:
      return (2.0 * c + 1.0) / 65535.0;
    case GL_UNSIGNED_INT:
      return c / kUintMax;
    case GL_INT:
      return (2.0 * c + 1.0) / kUintMax;
    default:
      return c;
  }
}

static double DenormalizeElement(GLenum type, double f) {
  switch (type) {
    case GL_UNSIGNED_BYTE:
      return 255.0 * f;
    case GL_BYTE:
      return (255.0 * f - 1.0) / 2.0;
    case GL_UNSIGNED_SHORT:
      return 65535.0 * f;
    case GL_SHORT:
      return (65535.0 * f - 1.0) / 2.0;
    case GL_UNSIGNED_INT:
      return kUintMax * f;
    case GL_INT:
      return (kUintMax * f - 1.0) / 2.0;
    default:
      return f;
  }
}

static void UnpackGroup(const PixelStore* store, const void* data, int width,
                        GLenum format, GLenum type, int i, int j,
                        double raw[4]) {
  if (type == GL_BITMAP) {
    raw[0] = ReadBit(store, data, width, i, j);
    return;
  }
  int components = FormatSize(format);
  const uint8_t* p =
      (const uint8_t*)data + GroupOffset(store, width, components, type, i, j);
  for (int c = 0; c < components; c++) {
    raw[c] = ReadElement(p + c * TypeSize(type), type, store->swap_bytes);
  }
}

static void* CopyClientImage(const PixelStore* store, int width, int height,
                             GLenum format, GLenum type, const void* pixels) {
  int components = FormatSize(format);
  if (!pixels || width < 0 || height < 0 || components == 0 ||
      !IsPixelType(type)) {
    return NULL;
  }
  size_t row = RowBytes(&kPackedStore, width, components, type);
  uint8_t* copy = Allocate(row * (size_t)height);
  memset(copy, 0, row * (size_t)height);
  int size = TypeSize(type);
  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      if (type == GL_BITMAP) {
        WriteBit(&kPackedStore, copy, width, i, j,
                 ReadBit(store, pixels, width, i, j));
        continue;
      }
      const uint8_t* source = (const uint8_t*)pixels +
                              GroupOffset(store, width, components, type, i, j);
      uint8_t* destination =
          copy + GroupOffset(&kPackedStore, width, components, type, i, j);
      for (int c = 0; c < components; c++) {
        CopyElement(source + c * size, destination + c * size, size,
                    store->swap_bytes);
      }
    }
  }
  return copy;
}

static double ShiftIndex(const Context* context, double index) {
  const PixelState* pixel = &context->state.pixel;
  return ldexp(index, pixel->index_shift) + pixel->index_offset;
}

static float LookupMap(const PixelMap* map, double index) {
  return map->values[llround(index) & (map->size - 1)];
}

static void TransferColor(const Context* context, float rgba[4]) {
  const PixelState* pixel = &context->state.pixel;
  for (int c = 0; c < 4; c++) {
    rgba[c] = rgba[c] * pixel->scale[c] + pixel->bias[c];
    if (pixel->map_color) {
      const PixelMap* map = &context->pixel_maps[6 + c];
      rgba[c] = map->values[lroundf(Clamp01(rgba[c]) * (float)(map->size - 1))];
    }
  }
}

static bool TransferIsIdentity(const Context* context) {
  const PixelState* pixel = &context->state.pixel;
  for (int c = 0; c < 4; c++) {
    if (pixel->scale[c] != 1.0f || pixel->bias[c] != 0.0f) {
      return false;
    }
  }
  return !pixel->map_color;
}

static GLuint TransferStencil(const Context* context, double index) {
  double value = ShiftIndex(context, index);
  if (context->state.pixel.map_stencil) {
    value = LookupMap(&context->pixel_maps[1], value);
  }
  return (GLuint)llround(value);
}

static void GroupToColor(const Context* context, GLenum format, GLenum type,
                         const double raw[4], float rgba[4]) {
  if (format == GL_COLOR_INDEX) {
    double index = ShiftIndex(context, raw[0]);
    for (int c = 0; c < 4; c++) {
      rgba[c] = LookupMap(&context->pixel_maps[2 + c], index);
    }
    return;
  }
  float v[4];
  for (int c = 0; c < FormatSize(format); c++) {
    v[c] = (float)NormalizeElement(type, raw[c]);
  }
  float r = 0.0f;
  float g = 0.0f;
  float b = 0.0f;
  float a = 1.0f;
  switch (format) {
    case GL_RED:
      r = v[0];
      break;
    case GL_GREEN:
      g = v[0];
      break;
    case GL_BLUE:
      b = v[0];
      break;
    case GL_ALPHA:
      a = v[0];
      break;
    case GL_RGB:
      r = v[0];
      g = v[1];
      b = v[2];
      break;
    case GL_RGBA:
      r = v[0];
      g = v[1];
      b = v[2];
      a = v[3];
      break;
    case GL_LUMINANCE:
      r = g = b = v[0];
      break;
    default:
      r = g = b = v[0];
      a = v[1];
  }
  rgba[0] = r;
  rgba[1] = g;
  rgba[2] = b;
  rgba[3] = a;
  TransferColor(context, rgba);
}

static void UnpackColor8(const Context* context, const PixelStore* store,
                         const void* data, int width, GLenum format,
                         GLenum type, int i, int j, uint8_t rgba[4]) {
  if (type == GL_UNSIGNED_BYTE && format != GL_COLOR_INDEX &&
      TransferIsIdentity(context)) {
    const GLubyte* p =
        (const GLubyte*)data +
        GroupOffset(store, width, FormatSize(format), type, i, j);
    switch (format) {
      case GL_RGBA:
        memcpy(rgba, p, 4);
        return;
      case GL_RGB:
        memcpy(rgba, (uint8_t[4]){p[0], p[1], p[2], 255}, 4);
        return;
      case GL_LUMINANCE:
        memcpy(rgba, (uint8_t[4]){p[0], p[0], p[0], 255}, 4);
        return;
      case GL_LUMINANCE_ALPHA:
        memcpy(rgba, (uint8_t[4]){p[0], p[0], p[0], p[1]}, 4);
        return;
      default:
        break;
    }
  }
  double raw[4];
  float color[4];
  UnpackGroup(store, data, width, format, type, i, j, raw);
  GroupToColor(context, format, type, raw, color);
  for (int c = 0; c < 4; c++) {
    rgba[c] = (uint8_t)PackChannel(color[c]);
  }
}

static void APIENTRY PixelStorei(GLenum pname, GLint param) {
  Context* context = current;
  if (InsideBeginEnd(context)) {
    return;
  }
  bool pack = pname >= GL_PACK_SWAP_BYTES && pname <= GL_PACK_ALIGNMENT;
  PixelStore* store = pack ? &context->client.pack : &context->client.unpack;
  switch (pname) {
    case GL_PACK_SWAP_BYTES:
    case GL_UNPACK_SWAP_BYTES:
      store->swap_bytes = param != 0;
      break;
    case GL_PACK_LSB_FIRST:
    case GL_UNPACK_LSB_FIRST:
      store->lsb_first = param != 0;
      break;
    case GL_PACK_ROW_LENGTH:
    case GL_UNPACK_ROW_LENGTH:
    case GL_PACK_SKIP_ROWS:
    case GL_UNPACK_SKIP_ROWS:
    case GL_PACK_SKIP_PIXELS:
    case GL_UNPACK_SKIP_PIXELS:
      if (param < 0) {
        SetError(context, GL_INVALID_VALUE);
      } else if (pname == GL_PACK_ROW_LENGTH || pname == GL_UNPACK_ROW_LENGTH) {
        store->row_length = param;
      } else if (pname == GL_PACK_SKIP_ROWS || pname == GL_UNPACK_SKIP_ROWS) {
        store->skip_rows = param;
      } else {
        store->skip_pixels = param;
      }
      break;
    case GL_PACK_ALIGNMENT:
    case GL_UNPACK_ALIGNMENT:
      if (param == 1 || param == 2 || param == 4 || param == 8) {
        store->alignment = param;
      } else {
        SetError(context, GL_INVALID_VALUE);
      }
      break;
    default:
      SetError(context, GL_INVALID_ENUM);
  }
}

static void APIENTRY PixelStoref(GLenum pname, GLfloat param) {
  bool boolean = pname == GL_PACK_SWAP_BYTES || pname == GL_UNPACK_SWAP_BYTES ||
                 pname == GL_PACK_LSB_FIRST || pname == GL_UNPACK_LSB_FIRST;
  PixelStorei(pname, boolean ? param != 0.0f : (GLint)lroundf(param));
}

static void APIENTRY PixelTransferf(GLenum pname, GLfloat param) {
  Context* context = current;
  if (Record(context, kOpPixelTransfer, (Arg[]){{.u = pname}, {.f = param}},
             2) ||
      InsideBeginEnd(context)) {
    return;
  }
  PixelState* pixel = &context->state.pixel;
  switch (pname) {
    case GL_MAP_COLOR:
      pixel->map_color = param != 0.0f;
      break;
    case GL_MAP_STENCIL:
      pixel->map_stencil = param != 0.0f;
      break;
    case GL_INDEX_SHIFT:
      pixel->index_shift = (GLint)lroundf(param);
      break;
    case GL_INDEX_OFFSET:
      pixel->index_offset = (GLint)lroundf(param);
      break;
    case GL_RED_SCALE:
      pixel->scale[0] = param;
      break;
    case GL_GREEN_SCALE:
      pixel->scale[1] = param;
      break;
    case GL_BLUE_SCALE:
      pixel->scale[2] = param;
      break;
    case GL_ALPHA_SCALE:
      pixel->scale[3] = param;
      break;
    case GL_RED_BIAS:
      pixel->bias[0] = param;
      break;
    case GL_GREEN_BIAS:
      pixel->bias[1] = param;
      break;
    case GL_BLUE_BIAS:
      pixel->bias[2] = param;
      break;
    case GL_ALPHA_BIAS:
      pixel->bias[3] = param;
      break;
    case GL_DEPTH_SCALE:
      pixel->depth_scale = param;
      break;
    case GL_DEPTH_BIAS:
      pixel->depth_bias = param;
      break;
    default:
      SetError(context, GL_INVALID_ENUM);
  }
}

static void APIENTRY PixelTransferi(GLenum pname, GLint param) {
  PixelTransferf(pname, (GLfloat)param);
}

static void APIENTRY PixelZoom(GLfloat xfactor, GLfloat yfactor) {
  Context* context = current;
  if (Record(context, kOpPixelZoom, (Arg[]){{.f = xfactor}, {.f = yfactor}},
             2) ||
      InsideBeginEnd(context)) {
    return;
  }
  context->state.pixel.zoom[0] = xfactor;
  context->state.pixel.zoom[1] = yfactor;
}

static int PixelMapIndex(GLenum map) {
  return map >= GL_PIXEL_MAP_I_TO_I && map <= GL_PIXEL_MAP_A_TO_A
             ? (int)(map - GL_PIXEL_MAP_I_TO_I)
             : -1;
}

static void APIENTRY PixelMapfv(GLenum map, GLint mapsize,
                                const GLfloat* values) {
  Context* context = current;
  bool valid_size = mapsize >= 1 && mapsize <= kMaxPixelMap;
  if ((Compiling(context) &&
       RecordData(context, kOpPixelMap, (Arg[]){{.u = map}, {.i = mapsize}}, 2,
                  valid_size
                      ? Duplicate(values, (size_t)mapsize * sizeof(GLfloat))
                      : NULL)) ||
      InsideBeginEnd(context)) {
    return;
  }
  int index = PixelMapIndex(map);
  if (index < 0) {
    SetError(context, GL_INVALID_ENUM);
    return;
  }
  if (!valid_size || (index <= 5 && !IsPowerOfTwo(mapsize))) {
    SetError(context, GL_INVALID_VALUE);
    return;
  }
  PixelMap* target = &context->pixel_maps[index];
  target->size = mapsize;
  for (int i = 0; i < mapsize; i++) {
    target->values[i] = index == 1 ? roundf(values[i]) : values[i];
  }
}

static void APIENTRY PixelMapuiv(GLenum map, GLint mapsize,
                                 const GLuint* values) {
  GLfloat converted[kMaxPixelMap];
  bool index = map == GL_PIXEL_MAP_I_TO_I || map == GL_PIXEL_MAP_S_TO_S;
  for (int i = 0; i < mapsize && i < kMaxPixelMap; i++) {
    converted[i] = index ? (GLfloat)values[i] : FromUint(values[i]);
  }
  PixelMapfv(map, mapsize, converted);
}

static void APIENTRY PixelMapusv(GLenum map, GLint mapsize,
                                 const GLushort* values) {
  GLfloat converted[kMaxPixelMap];
  bool index = map == GL_PIXEL_MAP_I_TO_I || map == GL_PIXEL_MAP_S_TO_S;
  for (int i = 0; i < mapsize && i < kMaxPixelMap; i++) {
    converted[i] = index ? (GLfloat)values[i] : FromUshort(values[i]);
  }
  PixelMapfv(map, mapsize, converted);
}

static void SetStipple(Context* context, const GLubyte* mask,
                       const PixelStore* store) {
  if (InsideBeginEnd(context)) {
    return;
  }
  for (int j = 0; j < kStippleSize; j++) {
    uint32_t row = 0;
    for (int i = 0; i < kStippleSize; i++) {
      row |= (uint32_t)ReadBit(store, mask, kStippleSize, i, j) << i;
    }
    context->state.stipple[j] = row;
  }
}

static void APIENTRY PolygonStipple(const GLubyte* mask) {
  Context* context = current;
  if (Compiling(context) &&
      RecordData(
          context, kOpPolygonStipple, NULL, 0,
          CopyClientImage(&context->client.unpack, kStippleSize, kStippleSize,
                          GL_COLOR_INDEX, GL_BITMAP, mask))) {
    return;
  }
  SetStipple(context, mask, &context->client.unpack);
}

static void FeedbackRaster(Context* context, GLenum token) {
  const RasterPosition* raster = &context->state.current.raster;
  FeedbackValue(context, (float)token);
  FeedbackVertex(context, raster->window, raster->color, raster->texcoord[0]);
}

static void RasterValues(const Context* context, const float color[4],
                         float values[kVaryings]) {
  const RasterPosition* raster = &context->state.current.raster;
  memcpy(values, color, 4 * sizeof(float));
  values[kFog] = raster->distance;
  for (int unit = 0; unit < kTextureUnits; unit++) {
    float* coord = &values[TexcoordVarying(unit)];
    coord[kS] = raster->texcoord[unit][0];
    coord[kT] = raster->texcoord[unit][1];
    coord[kQ] = raster->texcoord[unit][3];
  }
}

static void DrawBitmap(Context* context, GLsizei width, GLsizei height,
                       GLfloat xorig, GLfloat yorig, GLfloat xmove,
                       GLfloat ymove, const GLubyte* bitmap,
                       const PixelStore* store) {
  if (InsideBeginEnd(context)) {
    return;
  }
  if (width < 0 || height < 0) {
    SetError(context, GL_INVALID_VALUE);
    return;
  }
  RasterPosition* raster = &context->state.current.raster;
  if (!raster->valid) {
    return;
  }
  if (context->render_mode == GL_FEEDBACK) {
    FeedbackRaster(context, GL_BITMAP_TOKEN);
  } else if (context->render_mode == GL_RENDER && bitmap) {
    Primitive primitive;
    PreparePrimitive(context, &primitive);
    float values[kVaryings];
    RasterValues(context, raster->color, values);
    int x0 = (int)Floor(ClampFloat(raster->window[0] - xorig, -1e9f, 1e9f));
    int y0 = (int)Floor(ClampFloat(raster->window[1] - yorig, -1e9f, 1e9f));
    for (int j = 0; j < height; j++) {
      for (int i = 0; i < width; i++) {
        if (ReadBit(store, bitmap, width, i, j)) {
          EmitFragment(context, &primitive, x0 + i, y0 + j, raster->window[2],
                       values, NULL, 1.0f);
        }
      }
    }
  }
  raster->window[0] += xmove;
  raster->window[1] += ymove;
}

static void APIENTRY Bitmap(GLsizei width, GLsizei height, GLfloat xorig,
                            GLfloat yorig, GLfloat xmove, GLfloat ymove,
                            const GLubyte* bitmap) {
  Context* context = current;
  if (Compiling(context) &&
      RecordData(context, kOpBitmap,
                 (Arg[]){{.i = width},
                         {.i = height},
                         {.f = xorig},
                         {.f = yorig},
                         {.f = xmove},
                         {.f = ymove}},
                 6,
                 CopyClientImage(&context->client.unpack, width, height,
                                 GL_COLOR_INDEX, GL_BITMAP, bitmap))) {
    return;
  }
  DrawBitmap(context, width, height, xorig, yorig, xmove, ymove, bitmap,
             &context->client.unpack);
}

static void ZoomRange(float origin, float zoom, int n, int low, int high,
                      int* first, int* last) {
  float a = ClampFloat(origin + zoom * (float)n, -1e9f, 1e9f);
  float b = ClampFloat(origin + zoom * (float)(n + 1), -1e9f, 1e9f);
  *first = MaxInt((int)Ceil(MinFloat(a, b) - 0.5f), low);
  *last = MinInt((int)Ceil(MaxFloat(a, b) - 0.5f) - 1, high - 1);
}

static void DrawPixelRow(Context* context, const Primitive* primitive,
                         GLenum format, int row, int width,
                         const float* groups) {
  const RasterPosition* raster = &context->state.current.raster;
  const float* zoom = context->state.pixel.zoom;
  const int* rect = primitive->rect;
  int first_y;
  int last_y;
  ZoomRange(raster->window[1], zoom[1], row, rect[1], rect[3], &first_y,
            &last_y);
  for (int n = 0; n < width; n++) {
    int first_x;
    int last_x;
    ZoomRange(raster->window[0], zoom[0], n, rect[0], rect[2], &first_x,
              &last_x);
    const float* group = &groups[n * 4];
    float values[kVaryings];
    RasterValues(context, format == GL_DEPTH_COMPONENT ? raster->color : group,
                 values);
    float z = format == GL_DEPTH_COMPONENT ? group[0] : raster->window[2];
    for (int y = first_y; y <= last_y; y++) {
      for (int x = first_x; x <= last_x; x++) {
        if (format == GL_STENCIL_INDEX) {
          WriteStencil(
              context,
              &context->framebuffer
                   ->stencil[(size_t)y * (size_t)context->framebuffer->width +
                             (size_t)x],
              (GLuint)group[0] & kStencilMax);
        } else {
          EmitFragment(context, primitive, x, y, z, values, NULL, 1.0f);
        }
      }
    }
  }
}

static bool IsBitmapFormat(GLenum format) {
  return format == GL_COLOR_INDEX || format == GL_STENCIL_INDEX;
}

static void DrawPixelsFrom(Context* context, GLsizei width, GLsizei height,
                           GLenum format, GLenum type, const void* pixels,
                           const PixelStore* store) {
  if (InsideBeginEnd(context)) {
    return;
  }
  if (width < 0 || height < 0) {
    SetError(context, GL_INVALID_VALUE);
    return;
  }
  if (FormatSize(format) == 0 || !IsPixelType(type) ||
      (type == GL_BITMAP && !IsBitmapFormat(format))) {
    SetError(context, GL_INVALID_ENUM);
    return;
  }
  if (!context->state.current.raster.valid) {
    return;
  }
  if (context->render_mode == GL_FEEDBACK) {
    FeedbackRaster(context, GL_DRAW_PIXEL_TOKEN);
    return;
  }
  if (context->render_mode != GL_RENDER || !pixels) {
    return;
  }
  Primitive primitive;
  PreparePrimitive(context, &primitive);
  float* groups = Allocate((size_t)width * 4 * sizeof(float));
  const PixelState* pixel = &context->state.pixel;
  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      double raw[4];
      float* group = &groups[i * 4];
      UnpackGroup(store, pixels, width, format, type, i, j, raw);
      if (format == GL_STENCIL_INDEX) {
        group[0] = (float)(TransferStencil(context, raw[0]) & kStencilMax);
      } else if (format == GL_DEPTH_COMPONENT) {
        group[0] =
            Clamp01((float)NormalizeElement(type, raw[0]) * pixel->depth_scale +
                    pixel->depth_bias);
      } else {
        GroupToColor(context, format, type, raw, group);
        for (int c = 0; c < 4; c++) {
          group[c] = Clamp01(group[c]);
        }
      }
    }
    DrawPixelRow(context, &primitive, format, j, width, groups);
  }
  free(groups);
}

static void APIENTRY DrawPixels(GLsizei width, GLsizei height, GLenum format,
                                GLenum type, const GLvoid* pixels) {
  Context* context = current;
  if (Compiling(context) &&
      RecordData(
          context, kOpDrawPixels,
          (Arg[]){{.i = width}, {.i = height}, {.u = format}, {.u = type}}, 4,
          CopyClientImage(&context->client.unpack, width, height, format, type,
                          pixels))) {
    return;
  }
  DrawPixelsFrom(context, width, height, format, type, pixels,
                 &context->client.unpack);
}

static const uint32_t* ReadBuffer32(const Context* context) {
  GLenum buffer = context->state.pixel.read_buffer;
  return buffer == GL_BACK || buffer == GL_BACK_LEFT
             ? context->framebuffer->back
             : context->framebuffer->front;
}

static bool InsideWindow(const Context* context, int x, int y) {
  return x >= 0 && y >= 0 && x < context->framebuffer->width &&
         y < context->framebuffer->height;
}

static size_t PixelIndex(const Context* context, int x, int y) {
  return (size_t)y * (size_t)context->framebuffer->width + (size_t)x;
}

static void APIENTRY CopyPixels(GLint x, GLint y, GLsizei width, GLsizei height,
                                GLenum type) {
  Context* context = current;
  if (Record(
          context, kOpCopyPixels,
          (Arg[]){{.i = x}, {.i = y}, {.i = width}, {.i = height}, {.u = type}},
          5) ||
      InsideBeginEnd(context)) {
    return;
  }
  if (width < 0 || height < 0) {
    SetError(context, GL_INVALID_VALUE);
    return;
  }
  if (type != GL_COLOR && type != GL_DEPTH && type != GL_STENCIL) {
    SetError(context, GL_INVALID_ENUM);
    return;
  }
  if (!context->state.current.raster.valid) {
    return;
  }
  if (context->render_mode == GL_FEEDBACK) {
    FeedbackRaster(context, GL_COPY_PIXEL_TOKEN);
    return;
  }
  if (context->render_mode != GL_RENDER) {
    return;
  }
  size_t count = (size_t)width * (size_t)height;
  float* source = Allocate(count * 4 * sizeof(float));
  const uint32_t* colors = ReadBuffer32(context);
  const PixelState* pixel = &context->state.pixel;
  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      float* group = &source[((size_t)j * (size_t)width + (size_t)i) * 4];
      memset(group, 0, 4 * sizeof(float));
      if (!InsideWindow(context, x + i, y + j)) {
        continue;
      }
      size_t index = PixelIndex(context, x + i, y + j);
      if (type == GL_STENCIL) {
        group[0] = (float)(TransferStencil(
                               context, context->framebuffer->stencil[index]) &
                           kStencilMax);
      } else if (type == GL_DEPTH) {
        group[0] =
            Clamp01(context->framebuffer->depth[index] * pixel->depth_scale +
                    pixel->depth_bias);
      } else {
        PixelColor(colors[index], group);
        TransferColor(context, group);
        for (int c = 0; c < 4; c++) {
          group[c] = Clamp01(group[c]);
        }
      }
    }
  }
  Primitive primitive;
  PreparePrimitive(context, &primitive);
  GLenum format = type == GL_STENCIL ? GL_STENCIL_INDEX
                  : type == GL_DEPTH ? GL_DEPTH_COMPONENT
                                     : GL_RGBA;
  for (int j = 0; j < height; j++) {
    DrawPixelRow(context, &primitive, format, j, width,
                 &source[(size_t)j * (size_t)width * 4]);
  }
  free(source);
}

static double StencilTypeMask(GLenum type) {
  switch (type) {
    case GL_UNSIGNED_BYTE:
      return 255.0;
    case GL_BITMAP:
      return 1.0;
    case GL_BYTE:
      return 127.0;
    case GL_UNSIGNED_SHORT:
      return 65535.0;
    case GL_SHORT:
      return 32767.0;
    case GL_UNSIGNED_INT:
      return kUintMax;
    default:
      return 2147483647.0;
  }
}

static int SelectComponents(GLenum format, const float rgba[4], float out[4]) {
  switch (format) {
    case GL_RED:
    case GL_GREEN:
    case GL_BLUE:
    case GL_ALPHA:
      out[0] = rgba[format - GL_RED];
      return 1;
    case GL_RGB:
      memcpy(out, rgba, 3 * sizeof(float));
      return 3;
    case GL_RGBA:
      memcpy(out, rgba, 4 * sizeof(float));
      return 4;
    case GL_LUMINANCE:
      out[0] = Clamp01(rgba[0] + rgba[1] + rgba[2]);
      return 1;
    default:
      out[0] = Clamp01(rgba[0] + rgba[1] + rgba[2]);
      out[1] = rgba[3];
      return 2;
  }
}

static void PackColorGroup(const PixelStore* store, void* pixels, int width,
                           GLenum format, GLenum type, int i, int j,
                           const float rgba[4]) {
  float values[4];
  int count = SelectComponents(format, rgba, values);
  uint8_t* p = (uint8_t*)pixels + GroupOffset(store, width, count, type, i, j);
  for (int c = 0; c < count; c++) {
    WriteElement(p + c * TypeSize(type), type, store->swap_bytes,
                 DenormalizeElement(type, values[c]));
  }
}

static void APIENTRY ReadPixels(GLint x, GLint y, GLsizei width, GLsizei height,
                                GLenum format, GLenum type, GLvoid* pixels) {
  Context* context = current;
  if (InsideBeginEnd(context)) {
    return;
  }
  if (width < 0 || height < 0) {
    SetError(context, GL_INVALID_VALUE);
    return;
  }
  if (FormatSize(format) == 0 || !IsPixelType(type) ||
      (type == GL_BITMAP && !IsBitmapFormat(format))) {
    SetError(context, GL_INVALID_ENUM);
    return;
  }
  if (format == GL_COLOR_INDEX) {
    SetError(context, GL_INVALID_OPERATION);
    return;
  }
  const PixelStore* store = &context->client.pack;
  const uint32_t* colors = ReadBuffer32(context);
  const PixelState* pixel = &context->state.pixel;
  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      if (!InsideWindow(context, x + i, y + j)) {
        continue;
      }
      size_t index = PixelIndex(context, x + i, y + j);
      if (format == GL_STENCIL_INDEX) {
        GLuint value =
            TransferStencil(context, context->framebuffer->stencil[index]);
        if (type == GL_BITMAP) {
          WriteBit(store, pixels, width, i, j, value & 1);
        } else {
          uint8_t* p =
              (uint8_t*)pixels + GroupOffset(store, width, 1, type, i, j);
          double element =
              type == GL_FLOAT
                  ? (double)value
                  : (double)(value & (GLuint)StencilTypeMask(type));
          WriteElement(p, type, store->swap_bytes, element);
        }
      } else if (format == GL_DEPTH_COMPONENT) {
        uint8_t* p =
            (uint8_t*)pixels + GroupOffset(store, width, 1, type, i, j);
        float depth =
            Clamp01(context->framebuffer->depth[index] * pixel->depth_scale +
                    pixel->depth_bias);
        WriteElement(p, type, store->swap_bytes,
                     DenormalizeElement(type, depth));
      } else {
        float rgba[4];
        PixelColor(colors[index], rgba);
        TransferColor(context, rgba);
        for (int c = 0; c < 4; c++) {
          rgba[c] = Clamp01(rgba[c]);
        }
        PackColorGroup(store, pixels, width, format, type, i, j, rgba);
      }
    }
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

static int TargetIndex(GLenum target) {
  return target == GL_TEXTURE_1D   ? kTexture1D
         : target == GL_TEXTURE_2D ? kTexture2D
                                   : -1;
}

static bool IsTextureSize(int size, int border) {
  return size == 0 || IsPowerOfTwo(size - 2 * border);
}

static bool FitsTexture(int size, int border, int level) {
  return ((int64_t)(size - 2 * border) << level) <= (int64_t)kMaxTextureSize;
}

static Level* TextureLevel(Context* context, int dimensions, GLint level) {
  return &BoundTexture(context, dimensions - 1)->levels[level];
}

static void ReadTexels(Context* context, Level* level, int x0, int y0, int x,
                       int y, int width, int height) {
  const uint32_t* colors = ReadBuffer32(context);
  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      float rgba[4] = {0};
      if (InsideWindow(context, x + i, y + j)) {
        PixelColor(colors[PixelIndex(context, x + i, y + j)], rgba);
        TransferColor(context, rgba);
      }
      uint8_t bytes[4];
      for (int c = 0; c < 4; c++) {
        bytes[c] = (uint8_t)PackChannel(rgba[c]);
      }
      StoreTexel(level->format, bytes,
                 level->texels[(size_t)(y0 + j) * (size_t)level->width +
                               (size_t)(x0 + i)]);
    }
  }
}

static void WriteTexels(Context* context, Level* level, int x0, int y0,
                        int width, int height, GLenum format, GLenum type,
                        const void* pixels, const PixelStore* store) {
  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      uint8_t rgba[4];
      UnpackColor8(context, store, pixels, width, format, type, i, j, rgba);
      StoreTexel(level->format, rgba,
                 level->texels[(size_t)(y0 + j) * (size_t)level->width +
                               (size_t)(x0 + i)]);
    }
  }
}

static bool DefineLevel(Context* context, int dimensions, GLenum target,
                        GLint level, GLint internal_format, GLenum base,
                        GLsizei width, GLsizei height, GLint border,
                        Level** defined) {
  bool proxy =
      target == (dimensions == 1 ? GL_PROXY_TEXTURE_1D : GL_PROXY_TEXTURE_2D);
  if (level < 0 || level >= kMaxLevels || border < 0 || border > 1 ||
      !IsTextureSize(width, border) ||
      (dimensions == 2 && !IsTextureSize(height, border))) {
    SetError(context, GL_INVALID_VALUE);
    return false;
  }
  bool fits = FitsTexture(width, border, level) &&
              (dimensions == 1 || FitsTexture(height, border, level));
  if (!fits && !proxy) {
    SetError(context, GL_INVALID_VALUE);
    return false;
  }
  Level* destination = proxy ? &context->proxies[dimensions - 1][level]
                             : TextureLevel(context, dimensions, level);
  free(destination->texels);
  if (!fits) {
    *destination = (Level){0};
    *defined = NULL;
    return true;
  }
  *destination = (Level){
      .width = width,
      .height = dimensions == 1 ? 1 : height,
      .border = border,
      .internal_format = internal_format,
      .format = base,
  };
  size_t size = (size_t)width * (size_t)destination->height * 4;
  if (!proxy && width > 0 && destination->height > 0) {
    destination->texels = Allocate(size);
    memset(destination->texels, 0, size);
  }
  *defined = proxy ? NULL : destination;
  return true;
}

static void TexImage(Context* context, int dimensions, GLenum target,
                     GLint level, GLint internal_format, GLsizei width,
                     GLsizei height, GLint border, GLenum format, GLenum type,
                     const void* pixels, const PixelStore* store) {
  if (InsideBeginEnd(context)) {
    return;
  }
  GLenum texture_target = dimensions == 1 ? GL_TEXTURE_1D : GL_TEXTURE_2D;
  GLenum proxy_target =
      dimensions == 1 ? GL_PROXY_TEXTURE_1D : GL_PROXY_TEXTURE_2D;
  if ((target != texture_target && target != proxy_target) ||
      FormatSize(format) == 0 || format == GL_STENCIL_INDEX ||
      format == GL_DEPTH_COMPONENT || !IsPixelType(type) ||
      (type == GL_BITMAP && format != GL_COLOR_INDEX)) {
    SetError(context, GL_INVALID_ENUM);
    return;
  }
  GLenum base = BaseFormat(internal_format);
  if (!base) {
    SetError(context, GL_INVALID_VALUE);
    return;
  }
  Level* defined;
  if (!DefineLevel(context, dimensions, target, level, internal_format, base,
                   width, height, border, &defined)) {
    return;
  }
  if (defined && defined->texels && pixels) {
    WriteTexels(context, defined, 0, 0, defined->width, defined->height, format,
                type, pixels, store);
  }
}

static void TexImageCommand(Op op, int dimensions, GLenum target, GLint level,
                            GLint internal_format, GLsizei width,
                            GLsizei height, GLint border, GLenum format,
                            GLenum type, const GLvoid* pixels) {
  Context* context = current;
  bool proxy = target == GL_PROXY_TEXTURE_1D || target == GL_PROXY_TEXTURE_2D;
  if (!proxy && Compiling(context) &&
      RecordData(context, op,
                 (Arg[]){{.u = target},
                         {.i = level},
                         {.i = internal_format},
                         {.i = width},
                         {.i = height},
                         {.i = border},
                         {.u = format},
                         {.u = type}},
                 8,
                 CopyClientImage(&context->client.unpack, width,
                                 dimensions == 1 ? 1 : height, format, type,
                                 pixels))) {
    return;
  }
  TexImage(context, dimensions, target, level, internal_format, width, height,
           border, format, type, pixels, &context->client.unpack);
}

static void APIENTRY TexImage1D(GLenum target, GLint level,
                                GLint internal_format, GLsizei width,
                                GLint border, GLenum format, GLenum type,
                                const GLvoid* pixels) {
  TexImageCommand(kOpTexImage1D, 1, target, level, internal_format, width, 1,
                  border, format, type, pixels);
}

static void APIENTRY TexImage2D(GLenum target, GLint level,
                                GLint internal_format, GLsizei width,
                                GLsizei height, GLint border, GLenum format,
                                GLenum type, const GLvoid* pixels) {
  TexImageCommand(kOpTexImage2D, 2, target, level, internal_format, width,
                  height, border, format, type, pixels);
}

static Level* SubImageLevel(Context* context, int dimensions, GLenum target,
                            GLint level, GLint xoffset, GLint yoffset,
                            GLsizei width, GLsizei height) {
  if (target != (dimensions == 1 ? GL_TEXTURE_1D : GL_TEXTURE_2D)) {
    SetError(context, GL_INVALID_ENUM);
    return NULL;
  }
  if (level < 0 || level >= kMaxLevels || width < 0 || height < 0) {
    SetError(context, GL_INVALID_VALUE);
    return NULL;
  }
  Level* region = TextureLevel(context, dimensions, level);
  int border = region->border;
  int border_y = dimensions == 1 ? 0 : border;
  if (xoffset < -border || xoffset + width > region->width - border ||
      yoffset < -border_y || yoffset + height > region->height - border_y) {
    SetError(context, GL_INVALID_VALUE);
    return NULL;
  }
  return region;
}

static void TexSubImage(Context* context, int dimensions, GLenum target,
                        GLint level, GLint xoffset, GLint yoffset,
                        GLsizei width, GLsizei height, GLenum format,
                        GLenum type, const void* pixels,
                        const PixelStore* store) {
  if (InsideBeginEnd(context)) {
    return;
  }
  if (FormatSize(format) == 0 || format == GL_STENCIL_INDEX ||
      format == GL_DEPTH_COMPONENT || !IsPixelType(type) ||
      (type == GL_BITMAP && format != GL_COLOR_INDEX)) {
    SetError(context, GL_INVALID_ENUM);
    return;
  }
  if (dimensions == 1) {
    yoffset = 0;
    height = 1;
  }
  Level* region = SubImageLevel(context, dimensions, target, level, xoffset,
                                yoffset, width, height);
  if (region && region->texels && pixels) {
    int border_y = dimensions == 1 ? 0 : region->border;
    WriteTexels(context, region, xoffset + region->border, yoffset + border_y,
                width, height, format, type, pixels, store);
  }
}

static void APIENTRY TexSubImage1D(GLenum target, GLint level, GLint xoffset,
                                   GLsizei width, GLenum format, GLenum type,
                                   const GLvoid* pixels) {
  Context* context = current;
  if (Compiling(context) &&
      RecordData(context, kOpTexSubImage1D,
                 (Arg[]){{.u = target},
                         {.i = level},
                         {.i = xoffset},
                         {.i = width},
                         {.u = format},
                         {.u = type}},
                 6,
                 CopyClientImage(&context->client.unpack, width, 1, format,
                                 type, pixels))) {
    return;
  }
  TexSubImage(context, 1, target, level, xoffset, 0, width, 1, format, type,
              pixels, &context->client.unpack);
}

static void APIENTRY TexSubImage2D(GLenum target, GLint level, GLint xoffset,
                                   GLint yoffset, GLsizei width, GLsizei height,
                                   GLenum format, GLenum type,
                                   const GLvoid* pixels) {
  Context* context = current;
  if (Compiling(context) &&
      RecordData(context, kOpTexSubImage2D,
                 (Arg[]){{.u = target},
                         {.i = level},
                         {.i = xoffset},
                         {.i = yoffset},
                         {.i = width},
                         {.i = height},
                         {.u = format},
                         {.u = type}},
                 8,
                 CopyClientImage(&context->client.unpack, width, height, format,
                                 type, pixels))) {
    return;
  }
  TexSubImage(context, 2, target, level, xoffset, yoffset, width, height,
              format, type, pixels, &context->client.unpack);
}

static void CopyTexImage(int dimensions, GLenum target, GLint level,
                         GLenum internal_format, GLint x, GLint y,
                         GLsizei width, GLsizei height, GLint border) {
  Context* context = current;
  if (InsideBeginEnd(context)) {
    return;
  }
  GLenum base = internal_format >= 1 && internal_format <= 4
                    ? 0
                    : BaseFormat((GLint)internal_format);
  if (target != (dimensions == 1 ? GL_TEXTURE_1D : GL_TEXTURE_2D) || !base) {
    SetError(context, GL_INVALID_ENUM);
    return;
  }
  Level* defined;
  if (DefineLevel(context, dimensions, target, level, (GLint)internal_format,
                  base, width, height, border, &defined) &&
      defined->texels) {
    ReadTexels(context, defined, 0, 0, x, y, defined->width, defined->height);
  }
}

static void APIENTRY CopyTexImage1D(GLenum target, GLint level,
                                    GLenum internal_format, GLint x, GLint y,
                                    GLsizei width, GLint border) {
  if (Record(current, kOpCopyTexImage1D,
             (Arg[]){{.u = target},
                     {.i = level},
                     {.u = internal_format},
                     {.i = x},
                     {.i = y},
                     {.i = width},
                     {.i = border}},
             7)) {
    return;
  }
  CopyTexImage(1, target, level, internal_format, x, y, width, 1, border);
}

static void APIENTRY CopyTexImage2D(GLenum target, GLint level,
                                    GLenum internal_format, GLint x, GLint y,
                                    GLsizei width, GLsizei height,
                                    GLint border) {
  if (Record(current, kOpCopyTexImage2D,
             (Arg[]){{.u = target},
                     {.i = level},
                     {.u = internal_format},
                     {.i = x},
                     {.i = y},
                     {.i = width},
                     {.i = height},
                     {.i = border}},
             8)) {
    return;
  }
  CopyTexImage(2, target, level, internal_format, x, y, width, height, border);
}

static void CopyTexSubImage(int dimensions, GLenum target, GLint level,
                            GLint xoffset, GLint yoffset, GLint x, GLint y,
                            GLsizei width, GLsizei height) {
  Context* context = current;
  if (InsideBeginEnd(context)) {
    return;
  }
  Level* region = SubImageLevel(context, dimensions, target, level, xoffset,
                                yoffset, width, height);
  if (region && region->texels) {
    int border_y = dimensions == 1 ? 0 : region->border;
    ReadTexels(context, region, xoffset + region->border, yoffset + border_y, x,
               y, width, height);
  }
}

static void APIENTRY CopyTexSubImage1D(GLenum target, GLint level,
                                       GLint xoffset, GLint x, GLint y,
                                       GLsizei width) {
  if (Record(current, kOpCopyTexSubImage1D,
             (Arg[]){{.u = target},
                     {.i = level},
                     {.i = xoffset},
                     {.i = x},
                     {.i = y},
                     {.i = width}},
             6)) {
    return;
  }
  CopyTexSubImage(1, target, level, xoffset, 0, x, y, width, 1);
}

static void APIENTRY CopyTexSubImage2D(GLenum target, GLint level,
                                       GLint xoffset, GLint yoffset, GLint x,
                                       GLint y, GLsizei width, GLsizei height) {
  if (Record(current, kOpCopyTexSubImage2D,
             (Arg[]){{.u = target},
                     {.i = level},
                     {.i = xoffset},
                     {.i = yoffset},
                     {.i = x},
                     {.i = y},
                     {.i = width},
                     {.i = height}},
             8)) {
    return;
  }
  CopyTexSubImage(2, target, level, xoffset, yoffset, x, y, width, height);
}

static void APIENTRY TexParameterfv(GLenum target, GLenum pname,
                                    const GLfloat* params) {
  Context* context = current;
  int count = pname == GL_TEXTURE_BORDER_COLOR ? 4 : 1;
  Arg args[6] = {{.u = target}, {.u = pname}};
  for (int i = 0; i < count; i++) {
    args[2 + i].f = params[i];
  }
  if (Record(context, kOpTexParameter, args, 6) || InsideBeginEnd(context)) {
    return;
  }
  int index = TargetIndex(target);
  if (index < 0) {
    SetError(context, GL_INVALID_ENUM);
    return;
  }
  TextureParameters* parameters = &BoundTexture(context, index)->parameters;
  GLenum value = (GLenum)params[0];
  bool simple = value == GL_NEAREST || value == GL_LINEAR;
  bool mipmap =
      value >= GL_NEAREST_MIPMAP_NEAREST && value <= GL_LINEAR_MIPMAP_LINEAR;
  bool wrap = value == GL_CLAMP || value == GL_REPEAT;
  switch (pname) {
    case GL_TEXTURE_MIN_FILTER:
      if (simple || mipmap) {
        parameters->min_filter = value;
        return;
      }
      break;
    case GL_TEXTURE_MAG_FILTER:
      if (simple) {
        parameters->mag_filter = value;
        return;
      }
      break;
    case GL_TEXTURE_WRAP_S:
      if (wrap) {
        parameters->wrap_s = value;
        return;
      }
      break;
    case GL_TEXTURE_WRAP_T:
      if (wrap) {
        parameters->wrap_t = value;
        return;
      }
      break;
    case GL_TEXTURE_BORDER_COLOR:
      for (int i = 0; i < 4; i++) {
        parameters->border_color[i] = Clamp01(params[i]);
      }
      return;
    case GL_TEXTURE_PRIORITY:
      parameters->priority = Clamp01(params[0]);
      return;
    default:
      break;
  }
  SetError(context, GL_INVALID_ENUM);
}

static void APIENTRY TexParameterf(GLenum target, GLenum pname, GLfloat param) {
  TexParameterfv(target, pname == GL_TEXTURE_BORDER_COLOR ? GL_NONE : pname,
                 &param);
}

static void APIENTRY TexParameteri(GLenum target, GLenum pname, GLint param) {
  TexParameterf(target, pname, (GLfloat)param);
}

static void APIENTRY TexParameteriv(GLenum target, GLenum pname,
                                    const GLint* params) {
  GLfloat values[4] = {(GLfloat)params[0]};
  if (pname == GL_TEXTURE_BORDER_COLOR) {
    for (int i = 0; i < 4; i++) {
      values[i] = FromInt(params[i]);
    }
  }
  TexParameterfv(target, pname, values);
}

static bool IsCombineFunction(GLenum function) {
  return function == GL_REPLACE || function == GL_MODULATE ||
         function == GL_ADD || function == GL_ADD_SIGNED_ARB ||
         function == GL_INTERPOLATE_ARB || function == GL_SUBTRACT_ARB;
}

static bool IsCombineSource(GLenum source) {
  return source == GL_TEXTURE || source == GL_CONSTANT_ARB ||
         source == GL_PRIMARY_COLOR_ARB || source == GL_PREVIOUS_ARB;
}

static bool IsCombineOperand(GLenum operand, bool alpha) {
  return operand == GL_SRC_ALPHA || operand == GL_ONE_MINUS_SRC_ALPHA ||
         (!alpha &&
          (operand == GL_SRC_COLOR || operand == GL_ONE_MINUS_SRC_COLOR));
}

static Combiner* ArgumentCombiner(TextureUnit* unit, GLenum pname, GLenum rgb,
                                  GLenum alpha, int* index) {
  if (pname >= rgb && pname < rgb + 3) {
    *index = (int)(pname - rgb);
    return &unit->rgb;
  }
  if (pname >= alpha && pname < alpha + 3) {
    *index = (int)(pname - alpha);
    return &unit->alpha;
  }
  return NULL;
}

static void APIENTRY TexEnvfv(GLenum target, GLenum pname,
                              const GLfloat* params) {
  Context* context = current;
  int count = pname == GL_TEXTURE_ENV_COLOR ? 4 : 1;
  Arg args[6] = {{.u = target}, {.u = pname}};
  for (int i = 0; i < count; i++) {
    args[2 + i].f = params[i];
  }
  if (Record(context, kOpTexEnv, args, 6) || InsideBeginEnd(context)) {
    return;
  }
  if (target != GL_TEXTURE_ENV) {
    SetError(context, GL_INVALID_ENUM);
    return;
  }
  TextureUnit* unit = ActiveUnit(context);
  GLenum value = (GLenum)params[0];
  switch (pname) {
    case GL_TEXTURE_ENV_MODE:
      if (value == GL_MODULATE || value == GL_DECAL || value == GL_BLEND ||
          value == GL_REPLACE || value == GL_ADD || value == GL_COMBINE_ARB) {
        unit->env_mode = value;
        return;
      }
      break;
    case GL_TEXTURE_ENV_COLOR:
      for (int i = 0; i < 4; i++) {
        unit->env_color[i] = Clamp01(params[i]);
      }
      return;
    case GL_COMBINE_RGB_ARB:
    case GL_COMBINE_ALPHA_ARB:
      if (IsCombineFunction(value)) {
        (pname == GL_COMBINE_RGB_ARB ? &unit->rgb : &unit->alpha)->function =
            value;
        return;
      }
      break;
    case GL_RGB_SCALE_ARB:
    case GL_ALPHA_SCALE:
      if (params[0] != 1.0f && params[0] != 2.0f && params[0] != 4.0f) {
        SetError(context, GL_INVALID_VALUE);
        return;
      }
      (pname == GL_RGB_SCALE_ARB ? &unit->rgb : &unit->alpha)->scale =
          params[0];
      return;
    default: {
      int index;
      Combiner* combiner = ArgumentCombiner(unit, pname, GL_SOURCE0_RGB_ARB,
                                            GL_SOURCE0_ALPHA_ARB, &index);
      if (combiner && IsCombineSource(value)) {
        combiner->source[index] = value;
        return;
      }
      combiner = ArgumentCombiner(unit, pname, GL_OPERAND0_RGB_ARB,
                                  GL_OPERAND0_ALPHA_ARB, &index);
      if (combiner && IsCombineOperand(value, combiner == &unit->alpha)) {
        combiner->operand[index] = value;
        return;
      }
    }
  }
  SetError(context, GL_INVALID_ENUM);
}

static void APIENTRY TexEnvf(GLenum target, GLenum pname, GLfloat param) {
  TexEnvfv(target, pname == GL_TEXTURE_ENV_COLOR ? GL_NONE : pname, &param);
}

static void APIENTRY TexEnvi(GLenum target, GLenum pname, GLint param) {
  TexEnvf(target, pname, (GLfloat)param);
}

static void APIENTRY TexEnviv(GLenum target, GLenum pname,
                              const GLint* params) {
  GLfloat values[4] = {(GLfloat)params[0]};
  if (pname == GL_TEXTURE_ENV_COLOR) {
    for (int i = 0; i < 4; i++) {
      values[i] = FromInt(params[i]);
    }
  }
  TexEnvfv(target, pname, values);
}

static void FreeLevels(Texture* texture) {
  for (int level = 0; level < kMaxLevels; level++) {
    free(texture->levels[level].texels);
    texture->levels[level] = (Level){0};
  }
}

static void ResolveBindings(Context* context) {
  for (int unit = 0; unit < kTextureUnits; unit++) {
    GLuint* binding = context->state.texture.units[unit].binding;
    for (int target = kTexture1D; target <= kTexture2D; target++) {
      Name* entry = binding[target]
                        ? FindName(&context->shared->textures, binding[target])
                        : NULL;
      Texture* texture = entry ? entry->object : NULL;
      GLenum expected = target == kTexture1D ? GL_TEXTURE_1D : GL_TEXTURE_2D;
      if (!texture || texture->target != expected) {
        texture = &context->default_textures[target];
        binding[target] = 0;
      }
      context->bound[unit][target] = texture;
    }
  }
}

static void APIENTRY BindTexture(GLenum target, GLuint texture) {
  Context* context = current;
  if (Record(context, kOpBindTexture, (Arg[]){{.u = target}, {.u = texture}},
             2) ||
      InsideBeginEnd(context)) {
    return;
  }
  int index = TargetIndex(target);
  if (index < 0) {
    SetError(context, GL_INVALID_ENUM);
    return;
  }
  if (texture != 0) {
    Names* names = &context->shared->textures;
    Name* entry = FindName(names, texture);
    Texture* object = entry ? entry->object : NULL;
    if (object && object->target != target) {
      SetError(context, GL_INVALID_OPERATION);
      return;
    }
    if (!object) {
      object = Allocate(sizeof(Texture));
      *object = (Texture){
          .name = texture, .target = target, .parameters = kDefaultParameters};
      AddName(names, texture, object);
    }
  }
  ActiveUnit(context)->binding[index] = texture;
  ResolveBindings(context);
}

static void APIENTRY DeleteTextures(GLsizei n, const GLuint* textures) {
  Context* context = current;
  if (InsideBeginEnd(context)) {
    return;
  }
  if (n < 0) {
    SetError(context, GL_INVALID_VALUE);
    return;
  }
  Names* names = &context->shared->textures;
  for (GLsizei i = 0; i < n; i++) {
    Name* entry = textures[i] ? FindName(names, textures[i]) : NULL;
    if (!entry) {
      continue;
    }
    Texture* object = entry->object;
    RemoveName(names, textures[i]);
    if (!object) {
      continue;
    }
    for (int c = 0; c < kMaxContexts; c++) {
      if (atomic_load(&used[c]) && contexts[c].shared == context->shared) {
        ResolveBindings(&contexts[c]);
      }
    }
    FreeLevels(object);
    free(object);
  }
}

static void APIENTRY GenTextures(GLsizei n, GLuint* textures) {
  Context* context = current;
  if (InsideBeginEnd(context)) {
    return;
  }
  if (n < 0) {
    SetError(context, GL_INVALID_VALUE);
    return;
  }
  Names* names = &context->shared->textures;
  for (GLsizei i = 0; i < n; i++) {
    GLuint name = UnusedNames(names, 1);
    if (name == 0) {
      SetError(context, GL_OUT_OF_MEMORY);
      return;
    }
    AddName(names, name, NULL);
    textures[i] = name;
  }
}

static GLboolean APIENTRY IsTexture(GLuint texture) {
  Context* context = current;
  if (InsideBeginEnd(context) || texture == 0) {
    return GL_FALSE;
  }
  Name* entry = FindName(&context->shared->textures, texture);
  return entry && entry->object ? GL_TRUE : GL_FALSE;
}

static GLboolean APIENTRY AreTexturesResident(GLsizei n, const GLuint* textures,
                                              GLboolean* residences) {
  Context* context = current;
  if (InsideBeginEnd(context)) {
    return GL_FALSE;
  }
  if (n < 0) {
    SetError(context, GL_INVALID_VALUE);
    return GL_FALSE;
  }
  for (GLsizei i = 0; i < n; i++) {
    Name* entry =
        textures[i] ? FindName(&context->shared->textures, textures[i]) : NULL;
    if (!entry || !entry->object) {
      SetError(context, GL_INVALID_VALUE);
      return GL_FALSE;
    }
  }
  return GL_TRUE;
}

static void PrioritizeFrom(Context* context, GLsizei n, const GLuint* textures,
                           const GLclampf* priorities) {
  if (n < 0) {
    SetError(context, GL_INVALID_VALUE);
    return;
  }
  if (InsideBeginEnd(context)) {
    return;
  }
  for (GLsizei i = 0; i < n; i++) {
    Name* entry =
        textures[i] ? FindName(&context->shared->textures, textures[i]) : NULL;
    if (entry && entry->object) {
      ((Texture*)entry->object)->parameters.priority = Clamp01(priorities[i]);
    }
  }
}

static void APIENTRY PrioritizeTextures(GLsizei n, const GLuint* textures,
                                        const GLclampf* priorities) {
  Context* context = current;
  if (Compiling(context) && n >= 0) {
    size_t names_size = (size_t)n * sizeof(GLuint);
    uint8_t* data = Allocate(names_size + (size_t)n * sizeof(GLclampf));
    memcpy(data, textures, names_size);
    memcpy(data + names_size, priorities, (size_t)n * sizeof(GLclampf));
    if (RecordData(context, kOpPrioritizeTextures, (Arg[]){{.i = n}}, 1,
                   data)) {
      return;
    }
  }
  PrioritizeFrom(context, n, textures, priorities);
}

static void APIENTRY ActiveTextureARB(GLenum texture) {
  Context* context = current;
  if (Record(context, kOpActiveTexture, (Arg[]){{.u = texture}}, 1) ||
      InsideBeginEnd(context)) {
    return;
  }
  if (!IsTextureUnit(texture)) {
    SetError(context, GL_INVALID_ENUM);
    return;
  }
  context->state.texture.active = (int)(texture - GL_TEXTURE0_ARB);
}

static void APIENTRY Scissor(GLint x, GLint y, GLsizei width, GLsizei height) {
  Context* context = current;
  if (Record(context, kOpScissor,
             (Arg[]){{.i = x}, {.i = y}, {.i = width}, {.i = height}}, 4) ||
      InsideBeginEnd(context)) {
    return;
  }
  if (width < 0 || height < 0) {
    SetError(context, GL_INVALID_VALUE);
    return;
  }
  GLint* box = context->state.scissor.box;
  box[0] = x;
  box[1] = y;
  box[2] = width;
  box[3] = height;
}

static void APIENTRY AlphaFunc(GLenum func, GLclampf ref) {
  Context* context = current;
  if (Record(context, kOpAlphaFunc, (Arg[]){{.u = func}, {.f = ref}}, 2) ||
      InsideBeginEnd(context)) {
    return;
  }
  if (!IsCompare(func)) {
    SetError(context, GL_INVALID_ENUM);
    return;
  }
  context->state.color.alpha_func = func;
  context->state.color.alpha_ref = Clamp01(ref);
}

static void APIENTRY StencilFunc(GLenum func, GLint ref, GLuint mask) {
  Context* context = current;
  if (Record(context, kOpStencilFunc,
             (Arg[]){{.u = func}, {.i = ref}, {.u = mask}}, 3) ||
      InsideBeginEnd(context)) {
    return;
  }
  if (!IsCompare(func)) {
    SetError(context, GL_INVALID_ENUM);
    return;
  }
  context->state.stencil.func = func;
  context->state.stencil.ref = ClampInt(ref, 0, kStencilMax);
  context->state.stencil.value_mask = mask;
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
  Context* context = current;
  if (Record(context, kOpStencilOp,
             (Arg[]){{.u = fail}, {.u = zfail}, {.u = zpass}}, 3) ||
      InsideBeginEnd(context)) {
    return;
  }
  if (!IsStencilOp(fail) || !IsStencilOp(zfail) || !IsStencilOp(zpass)) {
    SetError(context, GL_INVALID_ENUM);
    return;
  }
  context->state.stencil.fail = fail;
  context->state.stencil.zfail = zfail;
  context->state.stencil.zpass = zpass;
}

static void APIENTRY StencilMask(GLuint mask) {
  Context* context = current;
  if (Record(context, kOpStencilMask, (Arg[]){{.u = mask}}, 1) ||
      InsideBeginEnd(context)) {
    return;
  }
  context->state.stencil.writemask = mask;
}

static void APIENTRY DepthFunc(GLenum func) {
  Context* context = current;
  if (Record(context, kOpDepthFunc, (Arg[]){{.u = func}}, 1) ||
      InsideBeginEnd(context)) {
    return;
  }
  if (!IsCompare(func)) {
    SetError(context, GL_INVALID_ENUM);
    return;
  }
  context->state.depth.func = func;
}

static void APIENTRY DepthMask(GLboolean flag) {
  Context* context = current;
  if (Record(context, kOpDepthMask, (Arg[]){{.u = flag}}, 1) ||
      InsideBeginEnd(context)) {
    return;
  }
  context->state.depth.mask = flag != GL_FALSE;
}

static bool IsBlendFactor(GLenum factor, bool source) {
  switch (factor) {
    case GL_ZERO:
    case GL_ONE:
    case GL_SRC_ALPHA:
    case GL_ONE_MINUS_SRC_ALPHA:
    case GL_DST_ALPHA:
    case GL_ONE_MINUS_DST_ALPHA:
      return true;
    case GL_DST_COLOR:
    case GL_ONE_MINUS_DST_COLOR:
    case GL_SRC_ALPHA_SATURATE:
      return source;
    case GL_SRC_COLOR:
    case GL_ONE_MINUS_SRC_COLOR:
      return !source;
    default:
      return false;
  }
}

static void APIENTRY BlendFunc(GLenum sfactor, GLenum dfactor) {
  Context* context = current;
  if (Record(context, kOpBlendFunc, (Arg[]){{.u = sfactor}, {.u = dfactor}},
             2) ||
      InsideBeginEnd(context)) {
    return;
  }
  if (!IsBlendFactor(sfactor, true) || !IsBlendFactor(dfactor, false)) {
    SetError(context, GL_INVALID_ENUM);
    return;
  }
  context->state.color.blend_src = sfactor;
  context->state.color.blend_dst = dfactor;
}

static void APIENTRY LogicOp(GLenum opcode) {
  Context* context = current;
  if (Record(context, kOpLogicOp, (Arg[]){{.u = opcode}}, 1) ||
      InsideBeginEnd(context)) {
    return;
  }
  if (opcode < GL_CLEAR || opcode > GL_SET) {
    SetError(context, GL_INVALID_ENUM);
    return;
  }
  context->state.color.logic_op = opcode;
}

static void APIENTRY ColorMask(GLboolean red, GLboolean green, GLboolean blue,
                               GLboolean alpha) {
  Context* context = current;
  if (Record(context, kOpColorMask,
             (Arg[]){{.u = red}, {.u = green}, {.u = blue}, {.u = alpha}}, 4) ||
      InsideBeginEnd(context)) {
    return;
  }
  bool* mask = context->state.color.mask;
  mask[0] = red != GL_FALSE;
  mask[1] = green != GL_FALSE;
  mask[2] = blue != GL_FALSE;
  mask[3] = alpha != GL_FALSE;
}

static void APIENTRY IndexMask(GLuint mask) {
  Context* context = current;
  if (Record(context, kOpIndexMask, (Arg[]){{.u = mask}}, 1) ||
      InsideBeginEnd(context)) {
    return;
  }
  context->state.color.index_writemask = mask;
}

static bool IsMissingBuffer(const Context* context, GLenum buffer) {
  return buffer == GL_FRONT_RIGHT || buffer == GL_BACK_RIGHT ||
         buffer == GL_RIGHT || (buffer >= GL_AUX0 && buffer <= GL_AUX3) ||
         (context->framebuffer->single_buffered &&
          (buffer == GL_BACK || buffer == GL_BACK_LEFT));
}

static void APIENTRY DrawBuffer(GLenum mode) {
  Context* context = current;
  if (Record(context, kOpDrawBuffer, (Arg[]){{.u = mode}}, 1) ||
      InsideBeginEnd(context)) {
    return;
  }
  if (IsMissingBuffer(context, mode)) {
    SetError(context, GL_INVALID_OPERATION);
    return;
  }
  switch (mode) {
    case GL_NONE:
    case GL_FRONT_LEFT:
    case GL_BACK_LEFT:
    case GL_FRONT:
    case GL_BACK:
    case GL_LEFT:
    case GL_FRONT_AND_BACK:
      context->state.color.draw_buffer = mode;
      return;
    default:
      SetError(context, GL_INVALID_ENUM);
  }
}

static void APIENTRY ReadBuffer(GLenum mode) {
  Context* context = current;
  if (Record(context, kOpReadBuffer, (Arg[]){{.u = mode}}, 1) ||
      InsideBeginEnd(context)) {
    return;
  }
  if (IsMissingBuffer(context, mode)) {
    SetError(context, GL_INVALID_OPERATION);
    return;
  }
  switch (mode) {
    case GL_FRONT_LEFT:
    case GL_BACK_LEFT:
    case GL_FRONT:
    case GL_BACK:
    case GL_LEFT:
      context->state.pixel.read_buffer = mode;
      return;
    default:
      SetError(context, GL_INVALID_ENUM);
  }
}

static void APIENTRY ClearColor(GLclampf red, GLclampf green, GLclampf blue,
                                GLclampf alpha) {
  Context* context = current;
  if (Record(context, kOpClearColor,
             (Arg[]){{.f = red}, {.f = green}, {.f = blue}, {.f = alpha}}, 4) ||
      InsideBeginEnd(context)) {
    return;
  }
  float* clear = context->state.color.clear;
  clear[0] = Clamp01(red);
  clear[1] = Clamp01(green);
  clear[2] = Clamp01(blue);
  clear[3] = Clamp01(alpha);
}

static void APIENTRY ClearIndex(GLfloat c) {
  Context* context = current;
  if (Record(context, kOpClearIndex, (Arg[]){{.f = c}}, 1) ||
      InsideBeginEnd(context)) {
    return;
  }
  context->state.color.clear_index = c;
}

static void APIENTRY ClearDepth(GLclampd depth) {
  Context* context = current;
  if (Record(context, kOpClearDepth, (Arg[]){{.d = depth}}, 1) ||
      InsideBeginEnd(context)) {
    return;
  }
  context->state.depth.clear = Clamp01((float)depth);
}

static void APIENTRY ClearStencil(GLint s) {
  Context* context = current;
  if (Record(context, kOpClearStencil, (Arg[]){{.i = s}}, 1) ||
      InsideBeginEnd(context)) {
    return;
  }
  context->state.stencil.clear = s & kStencilMax;
}

static void APIENTRY ClearAccum(GLfloat red, GLfloat green, GLfloat blue,
                                GLfloat alpha) {
  Context* context = current;
  if (Record(context, kOpClearAccum,
             (Arg[]){{.f = red}, {.f = green}, {.f = blue}, {.f = alpha}}, 4) ||
      InsideBeginEnd(context)) {
    return;
  }
  float* clear = context->state.accum_clear;
  clear[0] = ClampFloat(red, -1.0f, 1.0f);
  clear[1] = ClampFloat(green, -1.0f, 1.0f);
  clear[2] = ClampFloat(blue, -1.0f, 1.0f);
  clear[3] = ClampFloat(alpha, -1.0f, 1.0f);
}

static int16_t* AccumBuffer(Context* context) {
  if (!context->framebuffer->accum) {
    size_t count = (size_t)context->framebuffer->width *
                   (size_t)context->framebuffer->height * 4;
    context->framebuffer->accum = Allocate(count * sizeof(int16_t));
    memset(context->framebuffer->accum, 0, count * sizeof(int16_t));
  }
  return context->framebuffer->accum;
}

static int16_t ToAccum(float value) {
  return (int16_t)lroundf(ClampFloat(value, -1.0f, 1.0f) * kAccumMax);
}

static float FromAccum(int16_t value) { return (float)value / kAccumMax; }

static void APIENTRY Clear(GLbitfield mask) {
  Context* context = current;
  if (Record(context, kOpClear, (Arg[]){{.u = mask}}, 1) ||
      InsideBeginEnd(context)) {
    return;
  }
  if (mask & ~(GLbitfield)(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT |
                           GL_STENCIL_BUFFER_BIT | GL_ACCUM_BUFFER_BIT)) {
    SetError(context, GL_INVALID_VALUE);
    return;
  }
  if (context->render_mode != GL_RENDER) {
    return;
  }
  int rect[4];
  DrawableRect(context, rect);
  Targets targets = {0};
  if (mask & GL_COLOR_BUFFER_BIT) {
    SelectTargets(context, &targets);
  }
  uint32_t color = PackColor(context->state.color.clear);
  bool depth = (mask & GL_DEPTH_BUFFER_BIT) && context->state.depth.mask;
  bool stencil = mask & GL_STENCIL_BUFFER_BIT;
  int16_t* accum =
      (mask & GL_ACCUM_BUFFER_BIT) && context->framebuffer->accumulation
          ? AccumBuffer(context)
          : NULL;
  int16_t accum_clear[4];
  for (int c = 0; c < 4; c++) {
    accum_clear[c] = ToAccum(context->state.accum_clear[c]);
  }
  for (int y = rect[1]; y < rect[3]; y++) {
    for (int x = rect[0]; x < rect[2]; x++) {
      size_t index = PixelIndex(context, x, y);
      for (int i = 0; i < targets.count; i++) {
        uint32_t* pixel = &targets.buffers[i][index];
        *pixel = (*pixel & ~targets.mask) | (color & targets.mask);
      }
      if (depth) {
        context->framebuffer->depth[index] = context->state.depth.clear;
      }
      if (stencil) {
        WriteStencil(context, &context->framebuffer->stencil[index],
                     (GLuint)context->state.stencil.clear);
      }
      if (accum) {
        memcpy(&accum[index * 4], accum_clear, sizeof(accum_clear));
      }
    }
  }
}

static void APIENTRY Accum(GLenum op, GLfloat value) {
  Context* context = current;
  if (Record(context, kOpAccum, (Arg[]){{.u = op}, {.f = value}}, 2) ||
      InsideBeginEnd(context)) {
    return;
  }
  if (op != GL_ACCUM && op != GL_LOAD && op != GL_RETURN && op != GL_MULT &&
      op != GL_ADD) {
    SetError(context, GL_INVALID_ENUM);
    return;
  }
  if (!context->framebuffer->accumulation) {
    SetError(context, GL_INVALID_OPERATION);
    return;
  }
  int16_t* accum = AccumBuffer(context);
  int rect[4];
  DrawableRect(context, rect);
  Targets targets = {0};
  if (op == GL_RETURN) {
    SelectTargets(context, &targets);
  }
  const uint32_t* colors = ReadBuffer32(context);
  for (int y = rect[1]; y < rect[3]; y++) {
    for (int x = rect[0]; x < rect[2]; x++) {
      size_t index = PixelIndex(context, x, y);
      int16_t* cell = &accum[index * 4];
      float color[4];
      if (op == GL_ACCUM || op == GL_LOAD) {
        PixelColor(colors[index], color);
      }
      for (int c = 0; c < 4; c++) {
        float stored = FromAccum(cell[c]);
        switch (op) {
          case GL_ACCUM:
            cell[c] = ToAccum(stored + value * color[c]);
            break;
          case GL_LOAD:
            cell[c] = ToAccum(value * color[c]);
            break;
          case GL_MULT:
            cell[c] = ToAccum(stored * value);
            break;
          case GL_ADD:
            cell[c] = ToAccum(stored + value);
            break;
          default:
            color[c] = Clamp01(stored * value);
        }
      }
      if (op == GL_RETURN) {
        uint32_t packed = PackColor(color);
        for (int i = 0; i < targets.count; i++) {
          uint32_t* pixel = &targets.buffers[i][index];
          *pixel = (*pixel & ~targets.mask) | (packed & targets.mask);
        }
      }
    }
  }
}

static void Present(const Framebuffer* framebuffer, HDC hdc,
                    const uint32_t* pixels) {
  if (!pixels) {
    return;
  }
  BITMAPINFO info = {
      .bmiHeader =
          {
              .biSize = sizeof(info.bmiHeader),
              .biWidth = framebuffer->width,
              .biHeight = framebuffer->height,
              .biPlanes = 1,
              .biBitCount = 32,
              .biCompression = BI_RGB,
          },
  };
  StretchDIBits(hdc, 0, 0, framebuffer->width, framebuffer->height, 0, 0,
                framebuffer->width, framebuffer->height, pixels, &info,
                DIB_RGB_COLORS, SRCCOPY);
}

static void PresentFront(Context* context) {
  if (context->framebuffer->front_dirty) {
    Present(context->framebuffer, context->hdc, context->framebuffer->front);
    context->framebuffer->front_dirty = false;
  }
}

static void APIENTRY Finish(void) {
  Context* context = current;
  if (InsideBeginEnd(context)) {
    return;
  }
  PresentFront(context);
  GdiFlush();
}

static void APIENTRY Flush(void) {
  Context* context = current;
  if (InsideBeginEnd(context)) {
    return;
  }
  PresentFront(context);
}

static void APIENTRY SelectBuffer(GLsizei size, GLuint* buffer) {
  Context* context = current;
  if (InsideBeginEnd(context)) {
    return;
  }
  if (size < 0) {
    SetError(context, GL_INVALID_VALUE);
    return;
  }
  if (context->render_mode == GL_SELECT) {
    SetError(context, GL_INVALID_OPERATION);
    return;
  }
  context->selection.buffer = buffer;
  context->selection.size = size;
}

static void APIENTRY FeedbackBuffer(GLsizei size, GLenum type,
                                    GLfloat* buffer) {
  Context* context = current;
  if (InsideBeginEnd(context)) {
    return;
  }
  if (type != GL_2D && type != GL_3D && type != GL_3D_COLOR &&
      type != GL_3D_COLOR_TEXTURE && type != GL_4D_COLOR_TEXTURE) {
    SetError(context, GL_INVALID_ENUM);
    return;
  }
  if (size < 0) {
    SetError(context, GL_INVALID_VALUE);
    return;
  }
  if (context->render_mode == GL_FEEDBACK) {
    SetError(context, GL_INVALID_OPERATION);
    return;
  }
  context->feedback.buffer = buffer;
  context->feedback.size = size;
  context->feedback.type = type;
}

static GLint APIENTRY RenderMode(GLenum mode) {
  Context* context = current;
  if (InsideBeginEnd(context)) {
    return 0;
  }
  if (mode != GL_RENDER && mode != GL_SELECT && mode != GL_FEEDBACK) {
    SetError(context, GL_INVALID_ENUM);
    return 0;
  }
  if ((mode == GL_SELECT && !context->selection.buffer) ||
      (mode == GL_FEEDBACK && !context->feedback.buffer)) {
    SetError(context, GL_INVALID_OPERATION);
    return 0;
  }
  GLint result = 0;
  Selection* selection = &context->selection;
  Feedback* feedback = &context->feedback;
  if (context->render_mode == GL_SELECT) {
    WriteHitRecord(context);
    result = selection->overflow ? -1 : selection->hits;
  } else if (context->render_mode == GL_FEEDBACK) {
    result = feedback->overflow ? -1 : feedback->count;
  }
  selection->count = 0;
  selection->hits = 0;
  selection->overflow = false;
  selection->hit = false;
  selection->depth = 0;
  feedback->count = 0;
  feedback->overflow = false;
  context->render_mode = mode;
  return result;
}

static bool NameStackCommand(Op op, const Arg* args, int count) {
  Context* context = current;
  if (Record(context, op, args, count) || InsideBeginEnd(context) ||
      context->render_mode != GL_SELECT) {
    return false;
  }
  WriteHitRecord(context);
  return true;
}

static void APIENTRY InitNames(void) {
  if (NameStackCommand(kOpInitNames, NULL, 0)) {
    current->selection.depth = 0;
  }
}

static void APIENTRY LoadName(GLuint name) {
  if (!NameStackCommand(kOpLoadName, (Arg[]){{.u = name}}, 1)) {
    return;
  }
  Selection* selection = &current->selection;
  if (selection->depth == 0) {
    SetError(current, GL_INVALID_OPERATION);
    return;
  }
  selection->names[selection->depth - 1] = name;
}

static void APIENTRY PushName(GLuint name) {
  if (!NameStackCommand(kOpPushName, (Arg[]){{.u = name}}, 1)) {
    return;
  }
  Selection* selection = &current->selection;
  if (selection->depth == kMaxNameStack) {
    SetError(current, GL_STACK_OVERFLOW);
    return;
  }
  selection->names[selection->depth++] = name;
}

static void APIENTRY PopName(void) {
  if (!NameStackCommand(kOpPopName, NULL, 0)) {
    return;
  }
  Selection* selection = &current->selection;
  if (selection->depth == 0) {
    SetError(current, GL_STACK_UNDERFLOW);
    return;
  }
  selection->depth--;
}

static void APIENTRY PassThrough(GLfloat token) {
  Context* context = current;
  if (Record(context, kOpPassThrough, (Arg[]){{.f = token}}, 1) ||
      InsideBeginEnd(context)) {
    return;
  }
  if (context->render_mode == GL_FEEDBACK) {
    FeedbackValue(context, GL_PASS_THROUGH_TOKEN);
    FeedbackValue(context, token);
  }
}

static void APIENTRY ListBase(GLuint base) {
  Context* context = current;
  if (Record(context, kOpListBase, (Arg[]){{.u = base}}, 1) ||
      InsideBeginEnd(context)) {
    return;
  }
  context->state.list_base = base;
}

static void CopyGroups(State* destination, State* source, GLbitfield mask) {
  if (mask & GL_CURRENT_BIT) {
    destination->current = source->current;
  }
  if (mask & GL_POINT_BIT) {
    destination->point = source->point;
  }
  if (mask & GL_LINE_BIT) {
    destination->line = source->line;
  }
  if (mask & GL_POLYGON_BIT) {
    destination->polygon = source->polygon;
  }
  if (mask & GL_POLYGON_STIPPLE_BIT) {
    memcpy(destination->stipple, source->stipple, sizeof(source->stipple));
  }
  if (mask & GL_PIXEL_MODE_BIT) {
    destination->pixel = source->pixel;
  }
  if (mask & GL_LIGHTING_BIT) {
    destination->lighting = source->lighting;
  }
  if (mask & GL_FOG_BIT) {
    destination->fog = source->fog;
  }
  if (mask & GL_DEPTH_BUFFER_BIT) {
    destination->depth = source->depth;
  }
  if (mask & GL_ACCUM_BUFFER_BIT) {
    memcpy(destination->accum_clear, source->accum_clear,
           sizeof(source->accum_clear));
  }
  if (mask & GL_STENCIL_BUFFER_BIT) {
    destination->stencil = source->stencil;
  }
  if (mask & GL_VIEWPORT_BIT) {
    destination->viewport = source->viewport;
  }
  if (mask & GL_TRANSFORM_BIT) {
    destination->transform = source->transform;
  }
  if (mask & GL_COLOR_BUFFER_BIT) {
    destination->color = source->color;
  }
  if (mask & GL_HINT_BIT) {
    destination->hint = source->hint;
  }
  if (mask & GL_EVAL_BIT) {
    destination->eval = source->eval;
  }
  if (mask & GL_LIST_BIT) {
    destination->list_base = source->list_base;
  }
  if (mask & GL_TEXTURE_BIT) {
    destination->texture = source->texture;
  }
  if (mask & GL_SCISSOR_BIT) {
    destination->scissor = source->scissor;
  }
  if (mask & GL_ENABLE_BIT) {
    for (size_t i = 0; i < sizeof(kCapabilities) / sizeof(kCapabilities[0]);
         i++) {
      *Capability(destination, kCapabilities[i]) =
          *Capability(source, kCapabilities[i]);
    }
    for (int unit = 0; unit < kTextureUnits; unit++) {
      TextureUnit* to = &destination->texture.units[unit];
      const TextureUnit* from = &source->texture.units[unit];
      memcpy(to->enabled, from->enabled, sizeof(to->enabled));
      for (int coord = 0; coord < 4; coord++) {
        to->gen[coord].enabled = from->gen[coord].enabled;
      }
    }
  }
}

static void APIENTRY PushAttrib(GLbitfield mask) {
  Context* context = current;
  if (Record(context, kOpPushAttrib, (Arg[]){{.u = mask}}, 1) ||
      InsideBeginEnd(context)) {
    return;
  }
  if (context->attrib_depth == kMaxAttribDepth) {
    SetError(context, GL_STACK_OVERFLOW);
    return;
  }
  AttribEntry* entry = &context->attrib_stack[context->attrib_depth++];
  entry->mask = mask;
  entry->state = context->state;
  for (int unit = 0; unit < kTextureUnits; unit++) {
    for (int target = kTexture1D; target <= kTexture2D; target++) {
      entry->textures[unit][target] = context->bound[unit][target]->parameters;
    }
  }
}

static void APIENTRY PopAttrib(void) {
  Context* context = current;
  if (Record(context, kOpPopAttrib, NULL, 0) || InsideBeginEnd(context)) {
    return;
  }
  if (context->attrib_depth == 0) {
    SetError(context, GL_STACK_UNDERFLOW);
    return;
  }
  AttribEntry* entry = &context->attrib_stack[--context->attrib_depth];
  CopyGroups(&context->state, &entry->state, entry->mask);
  if (entry->mask & GL_TEXTURE_BIT) {
    ResolveBindings(context);
    for (int unit = kTextureUnits - 1; unit >= 0; unit--) {
      for (int target = kTexture1D; target <= kTexture2D; target++) {
        context->bound[unit][target]->parameters =
            entry->textures[unit][target];
      }
    }
  }
}

static void APIENTRY PushClientAttrib(GLbitfield mask) {
  Context* context = current;
  if (InsideBeginEnd(context)) {
    return;
  }
  if (context->client_depth == kMaxAttribDepth) {
    SetError(context, GL_STACK_OVERFLOW);
    return;
  }
  ClientEntry* entry = &context->client_stack[context->client_depth++];
  entry->mask = mask;
  entry->state = context->client;
}

static void APIENTRY PopClientAttrib(void) {
  Context* context = current;
  if (InsideBeginEnd(context)) {
    return;
  }
  if (context->client_depth == 0) {
    SetError(context, GL_STACK_UNDERFLOW);
    return;
  }
  ClientEntry* entry = &context->client_stack[--context->client_depth];
  if (entry->mask & GL_CLIENT_PIXEL_STORE_BIT) {
    context->client.pack = entry->state.pack;
    context->client.unpack = entry->state.unpack;
  }
  if (entry->mask & GL_CLIENT_VERTEX_ARRAY_BIT) {
    memcpy(context->client.arrays, entry->state.arrays,
           sizeof(context->client.arrays));
    context->client.active_texture = entry->state.active_texture;
  }
}

static void ExecuteList(Context* context, GLuint name) {
  Name* entry = FindName(&context->shared->lists, name);
  if (!entry || context->list_depth >= kMaxListNesting) {
    return;
  }
  const List* list = entry->object;
  context->list_depth++;
  for (int i = 0; i < list->count; i++) {
    const Command* command = &list->commands[i];
    const Arg* a = command->args;
    const void* data = command->data;
    switch (command->op) {
      case kOpBegin:
        Begin(a[0].u);
        break;
      case kOpEnd:
        End();
        break;
      case kOpArrays:
        if (InsideBeginEnd(context)) {
          while (list->commands[i].op != kOpEnd) {
            i++;
          }
        }
        break;
      case kOpVertex:
        Vertex4f(a[0].f, a[1].f, a[2].f, a[3].f);
        break;
      case kOpColor:
        Color4f(a[0].f, a[1].f, a[2].f, a[3].f);
        break;
      case kOpIndex:
        Indexf(a[0].f);
        break;
      case kOpNormal:
        Normal3f(a[0].f, a[1].f, a[2].f);
        break;
      case kOpTexCoord:
        MultiTexCoord4fARB(a[0].u, a[1].f, a[2].f, a[3].f, a[4].f);
        break;
      case kOpEdgeFlag:
        EdgeFlag((GLboolean)a[0].u);
        break;
      case kOpRect:
        Rectf(a[0].f, a[1].f, a[2].f, a[3].f);
        break;
      case kOpRasterPos:
        RasterPos4f(a[0].f, a[1].f, a[2].f, a[3].f);
        break;
      case kOpMaterial:
        Materialfv(a[0].u, a[1].u,
                   (const GLfloat[4]){a[2].f, a[3].f, a[4].f, a[5].f});
        break;
      case kOpLight:
        Lightfv(a[0].u, a[1].u,
                (const GLfloat[4]){a[2].f, a[3].f, a[4].f, a[5].f});
        break;
      case kOpLightModel:
        LightModelfv(a[0].u,
                     (const GLfloat[4]){a[1].f, a[2].f, a[3].f, a[4].f});
        break;
      case kOpColorMaterial:
        ColorMaterial(a[0].u, a[1].u);
        break;
      case kOpShadeModel:
        ShadeModel(a[0].u);
        break;
      case kOpFog:
        Fogfv(a[0].u, (const GLfloat[4]){a[1].f, a[2].f, a[3].f, a[4].f});
        break;
      case kOpClipPlane:
        ClipPlane(a[0].u, (const GLdouble[4]){a[1].d, a[2].d, a[3].d, a[4].d});
        break;
      case kOpMatrixMode:
        MatrixMode(a[0].u);
        break;
      case kOpLoadIdentity:
        LoadIdentity();
        break;
      case kOpLoadMatrix:
        LoadMatrixf(data);
        break;
      case kOpMultMatrix:
        MultMatrixf(data);
        break;
      case kOpRotate:
        Rotatef(a[0].f, a[1].f, a[2].f, a[3].f);
        break;
      case kOpScale:
        Scalef(a[0].f, a[1].f, a[2].f);
        break;
      case kOpTranslate:
        Translatef(a[0].f, a[1].f, a[2].f);
        break;
      case kOpFrustum:
        Frustum(a[0].d, a[1].d, a[2].d, a[3].d, a[4].d, a[5].d);
        break;
      case kOpOrtho:
        Ortho(a[0].d, a[1].d, a[2].d, a[3].d, a[4].d, a[5].d);
        break;
      case kOpPushMatrix:
        PushMatrix();
        break;
      case kOpPopMatrix:
        PopMatrix();
        break;
      case kOpViewport:
        Viewport(a[0].i, a[1].i, a[2].i, a[3].i);
        break;
      case kOpDepthRange:
        DepthRange(a[0].d, a[1].d);
        break;
      case kOpEnable:
        Enable(a[0].u);
        break;
      case kOpDisable:
        Disable(a[0].u);
        break;
      case kOpCullFace:
        CullFace(a[0].u);
        break;
      case kOpFrontFace:
        FrontFace(a[0].u);
        break;
      case kOpPolygonMode:
        PolygonMode(a[0].u, a[1].u);
        break;
      case kOpPolygonOffset:
        PolygonOffset(a[0].f, a[1].f);
        break;
      case kOpPolygonStipple:
        if (data) {
          SetStipple(context, data, &kPackedStore);
        }
        break;
      case kOpLineWidth:
        LineWidth(a[0].f);
        break;
      case kOpLineStipple:
        LineStipple(a[0].i, (GLushort)a[1].u);
        break;
      case kOpPointSize:
        PointSize(a[0].f);
        break;
      case kOpHint:
        Hint(a[0].u, a[1].u);
        break;
      case kOpTexParameter:
        TexParameterfv(a[0].u, a[1].u,
                       (const GLfloat[4]){a[2].f, a[3].f, a[4].f, a[5].f});
        break;
      case kOpTexEnv:
        TexEnvfv(a[0].u, a[1].u,
                 (const GLfloat[4]){a[2].f, a[3].f, a[4].f, a[5].f});
        break;
      case kOpTexGen:
        TexGenfv(a[0].u, a[1].u,
                 (const GLfloat[4]){a[2].f, a[3].f, a[4].f, a[5].f});
        break;
      case kOpTexImage1D:
        TexImage(context, 1, a[0].u, a[1].i, a[2].i, a[3].i, 1, a[5].i, a[6].u,
                 a[7].u, data, &kPackedStore);
        break;
      case kOpTexImage2D:
        TexImage(context, 2, a[0].u, a[1].i, a[2].i, a[3].i, a[4].i, a[5].i,
                 a[6].u, a[7].u, data, &kPackedStore);
        break;
      case kOpTexSubImage1D:
        TexSubImage(context, 1, a[0].u, a[1].i, a[2].i, 0, a[3].i, 1, a[4].u,
                    a[5].u, data, &kPackedStore);
        break;
      case kOpTexSubImage2D:
        TexSubImage(context, 2, a[0].u, a[1].i, a[2].i, a[3].i, a[4].i, a[5].i,
                    a[6].u, a[7].u, data, &kPackedStore);
        break;
      case kOpCopyTexImage1D:
        CopyTexImage1D(a[0].u, a[1].i, a[2].u, a[3].i, a[4].i, a[5].i, a[6].i);
        break;
      case kOpCopyTexImage2D:
        CopyTexImage2D(a[0].u, a[1].i, a[2].u, a[3].i, a[4].i, a[5].i, a[6].i,
                       a[7].i);
        break;
      case kOpCopyTexSubImage1D:
        CopyTexSubImage1D(a[0].u, a[1].i, a[2].i, a[3].i, a[4].i, a[5].i);
        break;
      case kOpCopyTexSubImage2D:
        CopyTexSubImage2D(a[0].u, a[1].i, a[2].i, a[3].i, a[4].i, a[5].i,
                          a[6].i, a[7].i);
        break;
      case kOpBindTexture:
        BindTexture(a[0].u, a[1].u);
        break;
      case kOpPrioritizeTextures:
        PrioritizeFrom(context, a[0].i, data,
                       (const GLclampf*)((const GLuint*)data + a[0].i));
        break;
      case kOpActiveTexture:
        ActiveTextureARB(a[0].u);
        break;
      case kOpScissor:
        Scissor(a[0].i, a[1].i, a[2].i, a[3].i);
        break;
      case kOpAlphaFunc:
        AlphaFunc(a[0].u, a[1].f);
        break;
      case kOpStencilFunc:
        StencilFunc(a[0].u, a[1].i, a[2].u);
        break;
      case kOpStencilOp:
        StencilOp(a[0].u, a[1].u, a[2].u);
        break;
      case kOpStencilMask:
        StencilMask(a[0].u);
        break;
      case kOpDepthFunc:
        DepthFunc(a[0].u);
        break;
      case kOpDepthMask:
        DepthMask((GLboolean)a[0].u);
        break;
      case kOpBlendFunc:
        BlendFunc(a[0].u, a[1].u);
        break;
      case kOpLogicOp:
        LogicOp(a[0].u);
        break;
      case kOpColorMask:
        ColorMask((GLboolean)a[0].u, (GLboolean)a[1].u, (GLboolean)a[2].u,
                  (GLboolean)a[3].u);
        break;
      case kOpIndexMask:
        IndexMask(a[0].u);
        break;
      case kOpDrawBuffer:
        DrawBuffer(a[0].u);
        break;
      case kOpReadBuffer:
        ReadBuffer(a[0].u);
        break;
      case kOpClearColor:
        ClearColor(a[0].f, a[1].f, a[2].f, a[3].f);
        break;
      case kOpClearIndex:
        ClearIndex(a[0].f);
        break;
      case kOpClearDepth:
        ClearDepth(a[0].d);
        break;
      case kOpClearStencil:
        ClearStencil(a[0].i);
        break;
      case kOpClearAccum:
        ClearAccum(a[0].f, a[1].f, a[2].f, a[3].f);
        break;
      case kOpClear:
        Clear(a[0].u);
        break;
      case kOpAccum:
        Accum(a[0].u, a[1].f);
        break;
      case kOpPixelZoom:
        PixelZoom(a[0].f, a[1].f);
        break;
      case kOpPixelTransfer:
        PixelTransferf(a[0].u, a[1].f);
        break;
      case kOpPixelMap:
        PixelMapfv(a[0].u, a[1].i, data);
        break;
      case kOpBitmap:
        DrawBitmap(context, a[0].i, a[1].i, a[2].f, a[3].f, a[4].f, a[5].f,
                   data, &kPackedStore);
        break;
      case kOpDrawPixels:
        DrawPixelsFrom(context, a[0].i, a[1].i, a[2].u, a[3].u, data,
                       &kPackedStore);
        break;
      case kOpCopyPixels:
        CopyPixels(a[0].i, a[1].i, a[2].i, a[3].i, a[4].u);
        break;
      case kOpMap1:
        Map1f(a[0].u, a[1].f, a[2].f, a[3].i, a[4].i, data);
        break;
      case kOpMap2:
        Map2f(a[0].u, a[1].f, a[2].f, a[3].i, a[4].i, a[5].f, a[6].f, a[7].i,
              a[8].i, data);
        break;
      case kOpMapGrid1:
        MapGrid1f(a[0].i, a[1].f, a[2].f);
        break;
      case kOpMapGrid2:
        MapGrid2f(a[0].i, a[1].f, a[2].f, a[3].i, a[4].f, a[5].f);
        break;
      case kOpEvalCoord1:
        EvalCoord1f(a[0].f);
        break;
      case kOpEvalCoord2:
        EvalCoord2f(a[0].f, a[1].f);
        break;
      case kOpEvalPoint1:
        EvalPoint1(a[0].i);
        break;
      case kOpEvalPoint2:
        EvalPoint2(a[0].i, a[1].i);
        break;
      case kOpEvalMesh1:
        EvalMesh1(a[0].u, a[1].i, a[2].i);
        break;
      case kOpEvalMesh2:
        EvalMesh2(a[0].u, a[1].i, a[2].i, a[3].i, a[4].i);
        break;
      case kOpInitNames:
        InitNames();
        break;
      case kOpLoadName:
        LoadName(a[0].u);
        break;
      case kOpPushName:
        PushName(a[0].u);
        break;
      case kOpPopName:
        PopName();
        break;
      case kOpPassThrough:
        PassThrough(a[0].f);
        break;
      case kOpCallList:
        if (a[0].u == 0) {
          SetError(context, GL_INVALID_VALUE);
        } else {
          ExecuteList(context, a[0].u);
        }
        break;
      case kOpCallLists:
        if (a[0].i < 0) {
          SetError(context, GL_INVALID_VALUE);
        } else if (!data) {
          SetError(context, GL_INVALID_ENUM);
        } else {
          for (GLsizei n = 0; n < a[0].i; n++) {
            ExecuteList(context,
                        context->state.list_base + ((const GLuint*)data)[n]);
          }
        }
        break;
      case kOpListBase:
        ListBase(a[0].u);
        break;
      case kOpPushAttrib:
        PushAttrib(a[0].u);
        break;
      case kOpPopAttrib:
        PopAttrib();
        break;
    }
  }
  context->list_depth--;
}

static void APIENTRY CallList(GLuint list) {
  Context* context = current;
  if (Record(context, kOpCallList, (Arg[]){{.u = list}}, 1)) {
    return;
  }
  if (list == 0) {
    SetError(context, GL_INVALID_VALUE);
    return;
  }
  ExecuteList(context, list);
}

static GLuint* ListOffsets(GLsizei n, GLenum type, const GLvoid* lists) {
  int bytes = type == GL_2_BYTES ? 2 : type == GL_3_BYTES ? 3 : 4;
  bool sequence =
      type == GL_2_BYTES || type == GL_3_BYTES || type == GL_4_BYTES;
  if (n < 0 || (!sequence && type != GL_FLOAT && TypeSize(type) == 0) ||
      type == GL_DOUBLE) {
    return NULL;
  }
  GLuint* offsets = Allocate((size_t)n * sizeof(GLuint));
  for (GLsizei i = 0; i < n; i++) {
    switch (type) {
      case GL_BYTE:
        offsets[i] = (GLuint)((const GLbyte*)lists)[i];
        break;
      case GL_UNSIGNED_BYTE:
        offsets[i] = ((const GLubyte*)lists)[i];
        break;
      case GL_SHORT:
        offsets[i] = (GLuint)((const GLshort*)lists)[i];
        break;
      case GL_UNSIGNED_SHORT:
        offsets[i] = ((const GLushort*)lists)[i];
        break;
      case GL_INT:
        offsets[i] = (GLuint)((const GLint*)lists)[i];
        break;
      case GL_UNSIGNED_INT:
        offsets[i] = ((const GLuint*)lists)[i];
        break;
      case GL_FLOAT:
        offsets[i] = (GLuint)(GLint)((const GLfloat*)lists)[i];
        break;
      default: {
        const GLubyte* p = (const GLubyte*)lists + (size_t)i * (size_t)bytes;
        GLuint offset = 0;
        for (int b = 0; b < bytes; b++) {
          offset = (offset << 8) + p[b];
        }
        offsets[i] = offset;
      }
    }
  }
  return offsets;
}

static void APIENTRY CallLists(GLsizei n, GLenum type, const GLvoid* lists) {
  Context* context = current;
  GLuint* offsets = ListOffsets(n, type, lists);
  if (Compiling(context) &&
      RecordData(
          context, kOpCallLists, (Arg[]){{.i = n}}, 1,
          offsets ? Duplicate(offsets, (size_t)n * sizeof(GLuint)) : NULL)) {
    free(offsets);
    return;
  }
  if (n < 0) {
    SetError(context, GL_INVALID_VALUE);
  } else if (!offsets) {
    SetError(context, GL_INVALID_ENUM);
  } else {
    for (GLsizei i = 0; i < n; i++) {
      ExecuteList(context, context->state.list_base + offsets[i]);
    }
  }
  free(offsets);
}

static void APIENTRY NewList(GLuint list, GLenum mode) {
  Context* context = current;
  if (InsideBeginEnd(context)) {
    return;
  }
  if (list == 0) {
    SetError(context, GL_INVALID_VALUE);
    return;
  }
  if (mode != GL_COMPILE && mode != GL_COMPILE_AND_EXECUTE) {
    SetError(context, GL_INVALID_ENUM);
    return;
  }
  if (context->list) {
    SetError(context, GL_INVALID_OPERATION);
    return;
  }
  context->list = Allocate(sizeof(List));
  *context->list = (List){0};
  context->list_name = list;
  context->list_mode = mode;
}

static void APIENTRY EndList(void) {
  Context* context = current;
  if (InsideBeginEnd(context)) {
    return;
  }
  if (!context->list) {
    SetError(context, GL_INVALID_OPERATION);
    return;
  }
  Names* names = &context->shared->lists;
  Name* entry = FindName(names, context->list_name);
  if (entry) {
    FreeList(entry->object);
  }
  AddName(names, context->list_name, context->list);
  context->list = NULL;
}

static GLuint APIENTRY GenLists(GLsizei range) {
  Context* context = current;
  if (InsideBeginEnd(context)) {
    return 0;
  }
  if (range < 0) {
    SetError(context, GL_INVALID_VALUE);
    return 0;
  }
  Names* names = &context->shared->lists;
  GLuint first = range ? UnusedNames(names, (GLuint)range) : 0;
  for (GLsizei i = 0; first && i < range; i++) {
    List* list = Allocate(sizeof(List));
    *list = (List){0};
    AddName(names, first + (GLuint)i, list);
  }
  return first;
}

static void APIENTRY DeleteLists(GLuint list, GLsizei range) {
  Context* context = current;
  if (InsideBeginEnd(context)) {
    return;
  }
  if (range < 0) {
    SetError(context, GL_INVALID_VALUE);
    return;
  }
  Names* names = &context->shared->lists;
  int position = NamePosition(names, list);
  while (position < names->count &&
         names->entries[position].name - list < (GLuint)range) {
    FreeList(names->entries[position].object);
    RemoveName(names, names->entries[position].name);
  }
}

static GLboolean APIENTRY IsList(GLuint list) {
  Context* context = current;
  if (InsideBeginEnd(context)) {
    return GL_FALSE;
  }
  return FindName(&context->shared->lists, list) ? GL_TRUE : GL_FALSE;
}

static int Floats(double values[16], const float* source, int count) {
  for (int i = 0; i < count; i++) {
    values[i] = source[i];
  }
  return count;
}

static int Number(double values[16], double value) {
  values[0] = value;
  return 1;
}

static int Pair(double values[16], double first, double second) {
  values[0] = first;
  values[1] = second;
  return 2;
}

static int StateValues(Context* context, GLenum pname, double values[16],
                       bool* color) {
  State* s = &context->state;
  int active = s->texture.active;
  const ClientState* client = &context->client;
  const Array* arrays = client->arrays;
  *color = false;
  bool* flag = Capability(s, pname);
  if (flag) {
    return Number(values, *flag);
  }
  int array = ArrayIndex(context, pname);
  if (array >= 0) {
    return Number(values, arrays[array].enabled);
  }
  if (pname >= GL_PIXEL_MAP_I_TO_I_SIZE && pname <= GL_PIXEL_MAP_A_TO_A_SIZE) {
    return Number(values,
                  context->pixel_maps[pname - GL_PIXEL_MAP_I_TO_I_SIZE].size);
  }
  const PixelStore* store =
      pname >= GL_PACK_SWAP_BYTES && pname <= GL_PACK_ALIGNMENT
          ? &client->pack
          : &client->unpack;
  switch (pname) {
    case GL_CURRENT_COLOR:
      *color = true;
      return Floats(values, s->current.color, 4);
    case GL_CURRENT_INDEX:
      return Number(values, s->current.index);
    case GL_CURRENT_NORMAL:
      *color = true;
      return Floats(values, s->current.normal, 3);
    case GL_CURRENT_TEXTURE_COORDS:
      return Floats(values, s->current.texcoord[active], 4);
    case GL_CURRENT_RASTER_COLOR:
      *color = true;
      return Floats(values, s->current.raster.color, 4);
    case GL_CURRENT_RASTER_INDEX:
      return Number(values, s->current.raster.index);
    case GL_CURRENT_RASTER_TEXTURE_COORDS:
      return Floats(values, s->current.raster.texcoord[active], 4);
    case GL_CURRENT_RASTER_POSITION:
      return Floats(values, s->current.raster.window, 4);
    case GL_CURRENT_RASTER_POSITION_VALID:
      return Number(values, s->current.raster.valid);
    case GL_CURRENT_RASTER_DISTANCE:
      return Number(values, s->current.raster.distance);
    case GL_EDGE_FLAG:
      return Number(values, s->current.edge_flag);
    case GL_ACTIVE_TEXTURE_ARB:
      return Number(values, GL_TEXTURE0_ARB + active);
    case GL_CLIENT_ACTIVE_TEXTURE_ARB:
      return Number(values, GL_TEXTURE0_ARB + client->active_texture);
    case GL_VERTEX_ARRAY_SIZE:
      return Number(values, arrays[kVertexArray].size);
    case GL_VERTEX_ARRAY_TYPE:
      return Number(values, arrays[kVertexArray].type);
    case GL_VERTEX_ARRAY_STRIDE:
      return Number(values, arrays[kVertexArray].stride);
    case GL_NORMAL_ARRAY_TYPE:
      return Number(values, arrays[kNormalArray].type);
    case GL_NORMAL_ARRAY_STRIDE:
      return Number(values, arrays[kNormalArray].stride);
    case GL_COLOR_ARRAY_SIZE:
      return Number(values, arrays[kColorArray].size);
    case GL_COLOR_ARRAY_TYPE:
      return Number(values, arrays[kColorArray].type);
    case GL_COLOR_ARRAY_STRIDE:
      return Number(values, arrays[kColorArray].stride);
    case GL_INDEX_ARRAY_TYPE:
      return Number(values, arrays[kIndexArray].type);
    case GL_INDEX_ARRAY_STRIDE:
      return Number(values, arrays[kIndexArray].stride);
    case GL_TEXTURE_COORD_ARRAY_SIZE:
      return Number(values, arrays[TexCoordArray(context)].size);
    case GL_TEXTURE_COORD_ARRAY_TYPE:
      return Number(values, arrays[TexCoordArray(context)].type);
    case GL_TEXTURE_COORD_ARRAY_STRIDE:
      return Number(values, arrays[TexCoordArray(context)].stride);
    case GL_EDGE_FLAG_ARRAY_STRIDE:
      return Number(values, arrays[kEdgeFlagArray].stride);
    case GL_MODELVIEW_MATRIX:
      return Floats(values, TopMatrix(context, kModelview), 16);
    case GL_PROJECTION_MATRIX:
      return Floats(values, TopMatrix(context, kProjection), 16);
    case GL_TEXTURE_MATRIX:
      return Floats(values, TopMatrix(context, kTextureMatrix + active), 16);
    case GL_MODELVIEW_STACK_DEPTH:
      return Number(values, context->matrix_top[kModelview] + 1);
    case GL_PROJECTION_STACK_DEPTH:
      return Number(values, context->matrix_top[kProjection] + 1);
    case GL_TEXTURE_STACK_DEPTH:
      return Number(values, context->matrix_top[kTextureMatrix + active] + 1);
    case GL_VIEWPORT:
      for (int i = 0; i < 4; i++) {
        values[i] = s->viewport.box[i];
      }
      return 4;
    case GL_DEPTH_RANGE:
      *color = true;
      return Floats(values, s->viewport.depth_range, 2);
    case GL_MATRIX_MODE:
      return Number(values, s->transform.matrix_mode);
    case GL_FOG_COLOR:
      *color = true;
      return Floats(values, s->fog.color, 4);
    case GL_FOG_INDEX:
      return Number(values, s->fog.index);
    case GL_FOG_DENSITY:
      return Number(values, s->fog.density);
    case GL_FOG_START:
      return Number(values, s->fog.start);
    case GL_FOG_END:
      return Number(values, s->fog.end);
    case GL_FOG_MODE:
      return Number(values, s->fog.mode);
    case GL_SHADE_MODEL:
      return Number(values, s->lighting.shade_model);
    case GL_COLOR_MATERIAL_PARAMETER:
      return Number(values, s->lighting.color_material_mode);
    case GL_COLOR_MATERIAL_FACE:
      return Number(values, s->lighting.color_material_face);
    case GL_LIGHT_MODEL_AMBIENT:
      *color = true;
      return Floats(values, s->lighting.model_ambient, 4);
    case GL_LIGHT_MODEL_LOCAL_VIEWER:
      return Number(values, s->lighting.local_viewer);
    case GL_LIGHT_MODEL_TWO_SIDE:
      return Number(values, s->lighting.two_side);
    case GL_POINT_SIZE:
      return Number(values, s->point.size);
    case GL_LINE_WIDTH:
      return Number(values, s->line.width);
    case GL_LINE_STIPPLE_PATTERN:
      return Number(values, s->line.pattern);
    case GL_LINE_STIPPLE_REPEAT:
      return Number(values, s->line.repeat);
    case GL_CULL_FACE_MODE:
      return Number(values, s->polygon.cull_mode);
    case GL_FRONT_FACE:
      return Number(values, s->polygon.front_face);
    case GL_POLYGON_MODE:
      return Pair(values, s->polygon.mode[0], s->polygon.mode[1]);
    case GL_POLYGON_OFFSET_FACTOR:
      return Number(values, s->polygon.offset_factor);
    case GL_POLYGON_OFFSET_UNITS:
      return Number(values, s->polygon.offset_units);
    case GL_TEXTURE_BINDING_1D:
      return Number(values, s->texture.units[active].binding[kTexture1D]);
    case GL_TEXTURE_BINDING_2D:
      return Number(values, s->texture.units[active].binding[kTexture2D]);
    case GL_SCISSOR_BOX:
      for (int i = 0; i < 4; i++) {
        values[i] = s->scissor.box[i];
      }
      return 4;
    case GL_ALPHA_TEST_FUNC:
      return Number(values, s->color.alpha_func);
    case GL_ALPHA_TEST_REF:
      *color = true;
      return Number(values, s->color.alpha_ref);
    case GL_STENCIL_FUNC:
      return Number(values, s->stencil.func);
    case GL_STENCIL_VALUE_MASK:
      return Number(values, (GLint)s->stencil.value_mask);
    case GL_STENCIL_REF:
      return Number(values, s->stencil.ref);
    case GL_STENCIL_FAIL:
      return Number(values, s->stencil.fail);
    case GL_STENCIL_PASS_DEPTH_FAIL:
      return Number(values, s->stencil.zfail);
    case GL_STENCIL_PASS_DEPTH_PASS:
      return Number(values, s->stencil.zpass);
    case GL_STENCIL_WRITEMASK:
      return Number(values, (GLint)s->stencil.writemask);
    case GL_STENCIL_CLEAR_VALUE:
      return Number(values, s->stencil.clear);
    case GL_DEPTH_FUNC:
      return Number(values, s->depth.func);
    case GL_DEPTH_WRITEMASK:
      return Number(values, s->depth.mask);
    case GL_DEPTH_CLEAR_VALUE:
      *color = true;
      return Number(values, s->depth.clear);
    case GL_BLEND_SRC:
      return Number(values, s->color.blend_src);
    case GL_BLEND_DST:
      return Number(values, s->color.blend_dst);
    case GL_LOGIC_OP_MODE:
      return Number(values, s->color.logic_op);
    case GL_DRAW_BUFFER:
      return Number(values, s->color.draw_buffer);
    case GL_READ_BUFFER:
      return Number(values, s->pixel.read_buffer);
    case GL_INDEX_WRITEMASK:
      return Number(values, (GLint)s->color.index_writemask);
    case GL_COLOR_WRITEMASK:
      for (int i = 0; i < 4; i++) {
        values[i] = s->color.mask[i];
      }
      return 4;
    case GL_COLOR_CLEAR_VALUE:
      *color = true;
      return Floats(values, s->color.clear, 4);
    case GL_INDEX_CLEAR_VALUE:
      return Number(values, s->color.clear_index);
    case GL_ACCUM_CLEAR_VALUE:
      *color = true;
      return Floats(values, s->accum_clear, 4);
    case GL_UNPACK_SWAP_BYTES:
    case GL_PACK_SWAP_BYTES:
      return Number(values, store->swap_bytes);
    case GL_UNPACK_LSB_FIRST:
    case GL_PACK_LSB_FIRST:
      return Number(values, store->lsb_first);
    case GL_UNPACK_ROW_LENGTH:
    case GL_PACK_ROW_LENGTH:
      return Number(values, store->row_length);
    case GL_UNPACK_SKIP_ROWS:
    case GL_PACK_SKIP_ROWS:
      return Number(values, store->skip_rows);
    case GL_UNPACK_SKIP_PIXELS:
    case GL_PACK_SKIP_PIXELS:
      return Number(values, store->skip_pixels);
    case GL_UNPACK_ALIGNMENT:
    case GL_PACK_ALIGNMENT:
      return Number(values, store->alignment);
    case GL_MAP_COLOR:
      return Number(values, s->pixel.map_color);
    case GL_MAP_STENCIL:
      return Number(values, s->pixel.map_stencil);
    case GL_INDEX_SHIFT:
      return Number(values, s->pixel.index_shift);
    case GL_INDEX_OFFSET:
      return Number(values, s->pixel.index_offset);
    case GL_RED_SCALE:
      return Number(values, s->pixel.scale[0]);
    case GL_GREEN_SCALE:
      return Number(values, s->pixel.scale[1]);
    case GL_BLUE_SCALE:
      return Number(values, s->pixel.scale[2]);
    case GL_ALPHA_SCALE:
      return Number(values, s->pixel.scale[3]);
    case GL_RED_BIAS:
      return Number(values, s->pixel.bias[0]);
    case GL_GREEN_BIAS:
      return Number(values, s->pixel.bias[1]);
    case GL_BLUE_BIAS:
      return Number(values, s->pixel.bias[2]);
    case GL_ALPHA_BIAS:
      return Number(values, s->pixel.bias[3]);
    case GL_DEPTH_SCALE:
      return Number(values, s->pixel.depth_scale);
    case GL_DEPTH_BIAS:
      return Number(values, s->pixel.depth_bias);
    case GL_ZOOM_X:
      return Number(values, s->pixel.zoom[0]);
    case GL_ZOOM_Y:
      return Number(values, s->pixel.zoom[1]);
    case GL_MAP1_GRID_DOMAIN:
      return Floats(values, s->eval.domain1, 2);
    case GL_MAP2_GRID_DOMAIN:
      return Floats(values, s->eval.domain2, 4);
    case GL_MAP1_GRID_SEGMENTS:
      return Number(values, s->eval.segments1);
    case GL_MAP2_GRID_SEGMENTS:
      return Pair(values, s->eval.segments2[0], s->eval.segments2[1]);
    case GL_PERSPECTIVE_CORRECTION_HINT:
      return Number(values, s->hint.perspective);
    case GL_POINT_SMOOTH_HINT:
      return Number(values, s->hint.point);
    case GL_LINE_SMOOTH_HINT:
      return Number(values, s->hint.line);
    case GL_POLYGON_SMOOTH_HINT:
      return Number(values, s->hint.polygon);
    case GL_FOG_HINT:
      return Number(values, s->hint.fog);
    case GL_MAX_LIGHTS:
      return Number(values, kMaxLights);
    case GL_MAX_CLIP_PLANES:
      return Number(values, kMaxClipPlanes);
    case GL_MAX_MODELVIEW_STACK_DEPTH:
      return Number(values, kModelviewDepth);
    case GL_MAX_PROJECTION_STACK_DEPTH:
      return Number(values, kProjectionDepth);
    case GL_MAX_TEXTURE_STACK_DEPTH:
      return Number(values, kTextureDepth);
    case GL_SUBPIXEL_BITS:
      return Number(values, kSubpixelBits);
    case GL_MAX_TEXTURE_UNITS_ARB:
      return Number(values, kTextureUnits);
    case GL_MAX_TEXTURE_SIZE:
      return Number(values, kMaxTextureSize);
    case GL_MAX_PIXEL_MAP_TABLE:
      return Number(values, kMaxPixelMap);
    case GL_MAX_NAME_STACK_DEPTH:
      return Number(values, kMaxNameStack);
    case GL_MAX_LIST_NESTING:
      return Number(values, kMaxListNesting);
    case GL_MAX_EVAL_ORDER:
      return Number(values, kMaxEvalOrder);
    case GL_MAX_VIEWPORT_DIMS:
      return Pair(values, kMaxViewport, kMaxViewport);
    case GL_MAX_ATTRIB_STACK_DEPTH:
    case GL_MAX_CLIENT_ATTRIB_STACK_DEPTH:
      return Number(values, kMaxAttribDepth);
    case GL_AUX_BUFFERS:
    case GL_INDEX_MODE:
    case GL_STEREO:
    case GL_INDEX_BITS:
      return Number(values, 0);
    case GL_RGBA_MODE:
      return Number(values, 1);
    case GL_DOUBLEBUFFER:
      return Number(values, !context->framebuffer->single_buffered);
    case GL_POINT_SIZE_RANGE:
    case GL_LINE_WIDTH_RANGE:
      return Pair(values, 1, kMaxPointSize);
    case GL_POINT_SIZE_GRANULARITY:
    case GL_LINE_WIDTH_GRANULARITY:
      return Number(values, kPointGranularity);
    case GL_RED_BITS:
    case GL_GREEN_BITS:
    case GL_BLUE_BITS:
    case GL_ALPHA_BITS:
      return Number(values, kChannelBits);
    case GL_DEPTH_BITS:
      return Number(values, kDepthBits);
    case GL_STENCIL_BITS:
      return Number(values, kStencilBits);
    case GL_ACCUM_RED_BITS:
    case GL_ACCUM_GREEN_BITS:
    case GL_ACCUM_BLUE_BITS:
    case GL_ACCUM_ALPHA_BITS:
      return Number(values,
                    context->framebuffer->accumulation ? kAccumBits : 0);
    case GL_LIST_BASE:
      return Number(values, s->list_base);
    case GL_LIST_INDEX:
      return Number(values, context->list ? context->list_name : 0);
    case GL_LIST_MODE:
      return Number(values, context->list ? context->list_mode : 0);
    case GL_ATTRIB_STACK_DEPTH:
      return Number(values, context->attrib_depth);
    case GL_CLIENT_ATTRIB_STACK_DEPTH:
      return Number(values, context->client_depth);
    case GL_NAME_STACK_DEPTH:
      return Number(values, context->selection.depth);
    case GL_RENDER_MODE:
      return Number(values, context->render_mode);
    case GL_SELECTION_BUFFER_SIZE:
      return Number(values, context->selection.size);
    case GL_FEEDBACK_BUFFER_SIZE:
      return Number(values, context->feedback.size);
    case GL_FEEDBACK_BUFFER_TYPE:
      return Number(values, context->feedback.type);
    default:
      return 0;
  }
}

static int QueryState(GLenum pname, double values[16], bool* color) {
  Context* context = current;
  if (InsideBeginEnd(context)) {
    return 0;
  }
  int count = StateValues(context, pname, values, color);
  if (count == 0) {
    SetError(context, GL_INVALID_ENUM);
  }
  return count;
}

static void APIENTRY GetBooleanv(GLenum pname, GLboolean* params) {
  double values[16];
  bool color;
  int count = QueryState(pname, values, &color);
  for (int i = 0; i < count; i++) {
    params[i] = values[i] != 0.0 ? GL_TRUE : GL_FALSE;
  }
}

static void APIENTRY GetDoublev(GLenum pname, GLdouble* params) {
  double values[16];
  bool color;
  int count = QueryState(pname, values, &color);
  for (int i = 0; i < count; i++) {
    params[i] = values[i];
  }
}

static void APIENTRY GetFloatv(GLenum pname, GLfloat* params) {
  double values[16];
  bool color;
  int count = QueryState(pname, values, &color);
  for (int i = 0; i < count; i++) {
    params[i] = (GLfloat)values[i];
  }
}

static void APIENTRY GetIntegerv(GLenum pname, GLint* params) {
  double values[16];
  bool color;
  int count = QueryState(pname, values, &color);
  for (int i = 0; i < count; i++) {
    params[i] = color ? ColorToInt(values[i]) : ToInt(values[i]);
  }
}

static GLboolean APIENTRY IsEnabled(GLenum cap) {
  Context* context = current;
  if (InsideBeginEnd(context)) {
    return GL_FALSE;
  }
  bool* flag = Capability(&context->state, cap);
  int array = ArrayIndex(context, cap);
  if (!flag && array < 0) {
    SetError(context, GL_INVALID_ENUM);
    return GL_FALSE;
  }
  bool enabled = flag ? *flag : context->client.arrays[array].enabled;
  return enabled ? GL_TRUE : GL_FALSE;
}

static GLenum APIENTRY GetError(void) {
  Context* context = current;
  if (InsideBeginEnd(context)) {
    return GL_NO_ERROR;
  }
  GLenum error = context->error;
  context->error = GL_NO_ERROR;
  return error;
}

static const GLubyte* APIENTRY GetString(GLenum name) {
  Context* context = current;
  if (InsideBeginEnd(context)) {
    return NULL;
  }
  switch (name) {
    case GL_VENDOR:
    case GL_RENDERER:
      return (const GLubyte*)"softgpu";
    case GL_VERSION:
      return (const GLubyte*)"1.1";
    case GL_EXTENSIONS:
      return (const GLubyte*)"GL_ARB_multitexture GL_ARB_texture_env_add "
                             "GL_ARB_texture_env_combine "
                             "GL_EXT_texture_env_add WGL_EXT_swap_control";
    default:
      SetError(context, GL_INVALID_ENUM);
      return NULL;
  }
}

static void APIENTRY GetClipPlane(GLenum plane, GLdouble* equation) {
  Context* context = current;
  if (InsideBeginEnd(context)) {
    return;
  }
  if (plane < GL_CLIP_PLANE0 || plane >= GL_CLIP_PLANE0 + kMaxClipPlanes) {
    SetError(context, GL_INVALID_ENUM);
    return;
  }
  for (int i = 0; i < 4; i++) {
    equation[i] = context->state.transform.planes[plane - GL_CLIP_PLANE0][i];
  }
}

static int LightValues(GLenum light, GLenum pname, float values[4],
                       bool* color) {
  Context* context = current;
  if (InsideBeginEnd(context)) {
    return 0;
  }
  int count = LightCount(pname);
  if (light < GL_LIGHT0 || light >= GL_LIGHT0 + kMaxLights || count == 0) {
    SetError(context, GL_INVALID_ENUM);
    return 0;
  }
  const Light* source = &context->state.lighting.lights[light - GL_LIGHT0];
  *color = count == 4 && pname != GL_POSITION;
  switch (pname) {
    case GL_AMBIENT:
      memcpy(values, source->ambient, sizeof(source->ambient));
      break;
    case GL_DIFFUSE:
      memcpy(values, source->diffuse, sizeof(source->diffuse));
      break;
    case GL_SPECULAR:
      memcpy(values, source->specular, sizeof(source->specular));
      break;
    case GL_POSITION:
      memcpy(values, source->position, sizeof(source->position));
      break;
    case GL_SPOT_DIRECTION:
      memcpy(values, source->direction, sizeof(source->direction));
      break;
    case GL_SPOT_EXPONENT:
      values[0] = source->exponent;
      break;
    case GL_SPOT_CUTOFF:
      values[0] = source->cutoff;
      break;
    case GL_CONSTANT_ATTENUATION:
      values[0] = source->attenuation[0];
      break;
    case GL_LINEAR_ATTENUATION:
      values[0] = source->attenuation[1];
      break;
    default:
      values[0] = source->attenuation[2];
  }
  return count;
}

static void APIENTRY GetLightfv(GLenum light, GLenum pname, GLfloat* params) {
  float values[4];
  bool color;
  int count = LightValues(light, pname, values, &color);
  memcpy(params, values, (size_t)count * sizeof(GLfloat));
}

static void APIENTRY GetLightiv(GLenum light, GLenum pname, GLint* params) {
  float values[4];
  bool color;
  int count = LightValues(light, pname, values, &color);
  for (int i = 0; i < count; i++) {
    params[i] = color ? ColorToInt(values[i]) : ToInt(values[i]);
  }
}

static int MaterialValues(GLenum face, GLenum pname, float values[4]) {
  Context* context = current;
  if (InsideBeginEnd(context)) {
    return 0;
  }
  int count = MaterialCount(pname);
  if ((face != GL_FRONT && face != GL_BACK) || count == 0 ||
      pname == GL_AMBIENT_AND_DIFFUSE) {
    SetError(context, GL_INVALID_ENUM);
    return 0;
  }
  const Material* material =
      &context->state.lighting.material[face == GL_FRONT ? kFront : kBack];
  switch (pname) {
    case GL_AMBIENT:
      memcpy(values, material->ambient, sizeof(material->ambient));
      break;
    case GL_DIFFUSE:
      memcpy(values, material->diffuse, sizeof(material->diffuse));
      break;
    case GL_SPECULAR:
      memcpy(values, material->specular, sizeof(material->specular));
      break;
    case GL_EMISSION:
      memcpy(values, material->emission, sizeof(material->emission));
      break;
    case GL_SHININESS:
      values[0] = material->shininess;
      break;
    default:
      memcpy(values, material->indexes, sizeof(material->indexes));
  }
  return count;
}

static void APIENTRY GetMaterialfv(GLenum face, GLenum pname, GLfloat* params) {
  float values[4];
  int count = MaterialValues(face, pname, values);
  memcpy(params, values, (size_t)count * sizeof(GLfloat));
}

static void APIENTRY GetMaterialiv(GLenum face, GLenum pname, GLint* params) {
  float values[4];
  int count = MaterialValues(face, pname, values);
  for (int i = 0; i < count; i++) {
    params[i] = count == 4 ? ColorToInt(values[i]) : ToInt(values[i]);
  }
}

static int TexEnvValues(GLenum target, GLenum pname, float values[4]) {
  Context* context = current;
  if (InsideBeginEnd(context)) {
    return 0;
  }
  if (target != GL_TEXTURE_ENV) {
    SetError(context, GL_INVALID_ENUM);
    return 0;
  }
  TextureUnit* unit = ActiveUnit(context);
  switch (pname) {
    case GL_TEXTURE_ENV_MODE:
      values[0] = (float)unit->env_mode;
      return 1;
    case GL_TEXTURE_ENV_COLOR:
      memcpy(values, unit->env_color, 4 * sizeof(float));
      return 4;
    case GL_COMBINE_RGB_ARB:
      values[0] = (float)unit->rgb.function;
      return 1;
    case GL_COMBINE_ALPHA_ARB:
      values[0] = (float)unit->alpha.function;
      return 1;
    case GL_RGB_SCALE_ARB:
      values[0] = unit->rgb.scale;
      return 1;
    case GL_ALPHA_SCALE:
      values[0] = unit->alpha.scale;
      return 1;
    default: {
      int index;
      const Combiner* combiner = ArgumentCombiner(
          unit, pname, GL_SOURCE0_RGB_ARB, GL_SOURCE0_ALPHA_ARB, &index);
      if (combiner) {
        values[0] = (float)combiner->source[index];
        return 1;
      }
      combiner = ArgumentCombiner(unit, pname, GL_OPERAND0_RGB_ARB,
                                  GL_OPERAND0_ALPHA_ARB, &index);
      if (combiner) {
        values[0] = (float)combiner->operand[index];
        return 1;
      }
      SetError(context, GL_INVALID_ENUM);
      return 0;
    }
  }
}

static void APIENTRY GetTexEnvfv(GLenum target, GLenum pname, GLfloat* params) {
  float values[4];
  int count = TexEnvValues(target, pname, values);
  memcpy(params, values, (size_t)count * sizeof(GLfloat));
}

static void APIENTRY GetTexEnviv(GLenum target, GLenum pname, GLint* params) {
  float values[4];
  int count = TexEnvValues(target, pname, values);
  for (int i = 0; i < count; i++) {
    params[i] = count == 4 ? ColorToInt(values[i]) : ToInt(values[i]);
  }
}

static int TexGenValues(GLenum coord, GLenum pname, float values[4]) {
  Context* context = current;
  if (InsideBeginEnd(context)) {
    return 0;
  }
  if (coord < GL_S || coord > GL_Q) {
    SetError(context, GL_INVALID_ENUM);
    return 0;
  }
  const TexGen* gen = &ActiveUnit(context)->gen[coord - GL_S];
  switch (pname) {
    case GL_TEXTURE_GEN_MODE:
      values[0] = (float)gen->mode;
      return 1;
    case GL_OBJECT_PLANE:
      memcpy(values, gen->object, sizeof(gen->object));
      return 4;
    case GL_EYE_PLANE:
      memcpy(values, gen->eye, sizeof(gen->eye));
      return 4;
    default:
      SetError(context, GL_INVALID_ENUM);
      return 0;
  }
}

static void APIENTRY GetTexGendv(GLenum coord, GLenum pname, GLdouble* params) {
  float values[4];
  int count = TexGenValues(coord, pname, values);
  for (int i = 0; i < count; i++) {
    params[i] = values[i];
  }
}

static void APIENTRY GetTexGenfv(GLenum coord, GLenum pname, GLfloat* params) {
  float values[4];
  int count = TexGenValues(coord, pname, values);
  memcpy(params, values, (size_t)count * sizeof(GLfloat));
}

static void APIENTRY GetTexGeniv(GLenum coord, GLenum pname, GLint* params) {
  float values[4];
  int count = TexGenValues(coord, pname, values);
  for (int i = 0; i < count; i++) {
    params[i] = ToInt(values[i]);
  }
}

static int TexParameterValues(GLenum target, GLenum pname, float values[4]) {
  Context* context = current;
  if (InsideBeginEnd(context)) {
    return 0;
  }
  int index = TargetIndex(target);
  if (index < 0) {
    SetError(context, GL_INVALID_ENUM);
    return 0;
  }
  const TextureParameters* parameters =
      &BoundTexture(context, index)->parameters;
  switch (pname) {
    case GL_TEXTURE_MIN_FILTER:
      values[0] = (float)parameters->min_filter;
      return 1;
    case GL_TEXTURE_MAG_FILTER:
      values[0] = (float)parameters->mag_filter;
      return 1;
    case GL_TEXTURE_WRAP_S:
      values[0] = (float)parameters->wrap_s;
      return 1;
    case GL_TEXTURE_WRAP_T:
      values[0] = (float)parameters->wrap_t;
      return 1;
    case GL_TEXTURE_BORDER_COLOR:
      memcpy(values, parameters->border_color, 4 * sizeof(float));
      return 4;
    case GL_TEXTURE_PRIORITY:
      values[0] = parameters->priority;
      return 1;
    case GL_TEXTURE_RESIDENT:
      values[0] = 1.0f;
      return 1;
    default:
      SetError(context, GL_INVALID_ENUM);
      return 0;
  }
}

static void APIENTRY GetTexParameterfv(GLenum target, GLenum pname,
                                       GLfloat* params) {
  float values[4];
  int count = TexParameterValues(target, pname, values);
  memcpy(params, values, (size_t)count * sizeof(GLfloat));
}

static void APIENTRY GetTexParameteriv(GLenum target, GLenum pname,
                                       GLint* params) {
  float values[4];
  int count = TexParameterValues(target, pname, values);
  for (int i = 0; i < count; i++) {
    params[i] = count == 4 ? ColorToInt(values[i]) : ToInt(values[i]);
  }
}

static bool HasComponent(GLenum format, GLenum component) {
  switch (component) {
    case GL_TEXTURE_RED_SIZE:
    case GL_TEXTURE_GREEN_SIZE:
    case GL_TEXTURE_BLUE_SIZE:
      return format == GL_RGB || format == GL_RGBA;
    case GL_TEXTURE_ALPHA_SIZE:
      return format == GL_ALPHA || format == GL_LUMINANCE_ALPHA ||
             format == GL_RGBA;
    case GL_TEXTURE_LUMINANCE_SIZE:
      return format == GL_LUMINANCE || format == GL_LUMINANCE_ALPHA;
    default:
      return format == GL_INTENSITY;
  }
}

static bool LevelValue(GLenum target, GLint level, GLenum pname, GLint* value) {
  Context* context = current;
  if (InsideBeginEnd(context)) {
    return false;
  }
  int index = TargetIndex(target);
  bool proxy = target == GL_PROXY_TEXTURE_1D || target == GL_PROXY_TEXTURE_2D;
  if (index < 0 && !proxy) {
    SetError(context, GL_INVALID_ENUM);
    return false;
  }
  if (level < 0 || level >= kMaxLevels) {
    SetError(context, GL_INVALID_VALUE);
    return false;
  }
  const Level* source =
      proxy ? &context->proxies[target == GL_PROXY_TEXTURE_1D ? 0 : 1][level]
            : &BoundTexture(context, index)->levels[level];
  switch (pname) {
    case GL_TEXTURE_WIDTH:
      *value = source->width;
      return true;
    case GL_TEXTURE_HEIGHT:
      *value = source->height;
      return true;
    case GL_TEXTURE_BORDER:
      *value = source->border;
      return true;
    case GL_TEXTURE_INTERNAL_FORMAT:
      *value = source->internal_format ? source->internal_format : 1;
      return true;
    case GL_TEXTURE_RED_SIZE:
    case GL_TEXTURE_GREEN_SIZE:
    case GL_TEXTURE_BLUE_SIZE:
    case GL_TEXTURE_ALPHA_SIZE:
    case GL_TEXTURE_LUMINANCE_SIZE:
    case GL_TEXTURE_INTENSITY_SIZE:
      *value = source->width && HasComponent(source->format, pname)
                   ? kChannelBits
                   : 0;
      return true;
    default:
      SetError(context, GL_INVALID_ENUM);
      return false;
  }
}

static void APIENTRY GetTexLevelParameterfv(GLenum target, GLint level,
                                            GLenum pname, GLfloat* params) {
  GLint value;
  if (LevelValue(target, level, pname, &value)) {
    *params = (GLfloat)value;
  }
}

static void APIENTRY GetTexLevelParameteriv(GLenum target, GLint level,
                                            GLenum pname, GLint* params) {
  GLint value;
  if (LevelValue(target, level, pname, &value)) {
    *params = value;
  }
}

static void APIENTRY GetTexImage(GLenum target, GLint level, GLenum format,
                                 GLenum type, GLvoid* pixels) {
  Context* context = current;
  if (InsideBeginEnd(context)) {
    return;
  }
  int index = TargetIndex(target);
  if (index < 0 || FormatSize(format) == 0 || format == GL_COLOR_INDEX ||
      format == GL_STENCIL_INDEX || format == GL_DEPTH_COMPONENT ||
      !IsPixelType(type) || type == GL_BITMAP) {
    SetError(context, GL_INVALID_ENUM);
    return;
  }
  if (level < 0 || level >= kMaxLevels) {
    SetError(context, GL_INVALID_VALUE);
    return;
  }
  const Level* source = &BoundTexture(context, index)->levels[level];
  if (!source->texels) {
    return;
  }
  for (int j = 0; j < source->height; j++) {
    for (int i = 0; i < source->width; i++) {
      const uint8_t* texel =
          source->texels[(size_t)j * (size_t)source->width + (size_t)i];
      float rgba[4] = {(float)texel[0] / 255.0f, 0.0f, 0.0f, 1.0f};
      if (source->format == GL_RGB || source->format == GL_RGBA) {
        rgba[1] = (float)texel[1] / 255.0f;
        rgba[2] = (float)texel[2] / 255.0f;
      }
      if (source->format == GL_ALPHA) {
        rgba[0] = 0.0f;
      }
      if (source->format == GL_ALPHA || source->format == GL_LUMINANCE_ALPHA ||
          source->format == GL_RGBA) {
        rgba[3] = (float)texel[3] / 255.0f;
      }
      PackColorGroup(&context->client.pack, pixels, source->width, format, type,
                     i, j, rgba);
    }
  }
}

static int PixelMapValues(GLenum map, const PixelMap** source) {
  Context* context = current;
  if (InsideBeginEnd(context)) {
    return -1;
  }
  int index = PixelMapIndex(map);
  if (index < 0) {
    SetError(context, GL_INVALID_ENUM);
    return -1;
  }
  *source = &context->pixel_maps[index];
  return index;
}

static void APIENTRY GetPixelMapfv(GLenum map, GLfloat* values) {
  const PixelMap* source;
  if (PixelMapValues(map, &source) >= 0) {
    memcpy(values, source->values, (size_t)source->size * sizeof(GLfloat));
  }
}

static void APIENTRY GetPixelMapuiv(GLenum map, GLuint* values) {
  const PixelMap* source;
  int index = PixelMapValues(map, &source);
  for (int i = 0; index >= 0 && i < source->size; i++) {
    double value = source->values[i];
    values[i] =
        (GLuint)RoundClamp(index <= 1 ? value : value * kUintMax, 0, kUintMax);
  }
}

static void APIENTRY GetPixelMapusv(GLenum map, GLushort* values) {
  const PixelMap* source;
  int index = PixelMapValues(map, &source);
  for (int i = 0; index >= 0 && i < source->size; i++) {
    double value = source->values[i];
    values[i] =
        (GLushort)RoundClamp(index <= 1 ? value : value * 65535.0, 0, 65535.0);
  }
}

static int MapValues(GLenum target, GLenum query, double values[256]) {
  Context* context = current;
  if (InsideBeginEnd(context)) {
    return 0;
  }
  int index1 = MapIndex(target, GL_MAP1_COLOR_4);
  int index2 = MapIndex(target, GL_MAP2_COLOR_4);
  if (index1 < 0 && index2 < 0) {
    SetError(context, GL_INVALID_ENUM);
    return 0;
  }
  bool two = index2 >= 0;
  const Map* map = two ? &context->maps2[index2] : &context->maps1[index1];
  int size = kMapSizes[two ? index2 : index1];
  switch (query) {
    case GL_ORDER:
      values[0] = map->order[0];
      values[1] = map->order[1];
      return two ? 2 : 1;
    case GL_DOMAIN:
      for (int i = 0; i < 4; i++) {
        values[i] = map->domain[i];
      }
      return two ? 4 : 2;
    case GL_COEFF: {
      int count = map->order[0] * map->order[1] * size;
      for (int i = 0; i < count; i++) {
        values[i] = map->points[i];
      }
      return count;
    }
    default:
      SetError(context, GL_INVALID_ENUM);
      return 0;
  }
}

static void APIENTRY GetMapdv(GLenum target, GLenum query, GLdouble* v) {
  double values[kMaxEvalOrder * kMaxEvalOrder * 4];
  int count = MapValues(target, query, values);
  for (int i = 0; i < count; i++) {
    v[i] = values[i];
  }
}

static void APIENTRY GetMapfv(GLenum target, GLenum query, GLfloat* v) {
  double values[kMaxEvalOrder * kMaxEvalOrder * 4];
  int count = MapValues(target, query, values);
  for (int i = 0; i < count; i++) {
    v[i] = (GLfloat)values[i];
  }
}

static void APIENTRY GetMapiv(GLenum target, GLenum query, GLint* v) {
  double values[kMaxEvalOrder * kMaxEvalOrder * 4];
  int count = MapValues(target, query, values);
  for (int i = 0; i < count; i++) {
    v[i] = ToInt(values[i]);
  }
}

static void APIENTRY GetPolygonStipple(GLubyte* mask) {
  Context* context = current;
  if (InsideBeginEnd(context)) {
    return;
  }
  for (int j = 0; j < kStippleSize; j++) {
    for (int i = 0; i < kStippleSize; i++) {
      WriteBit(&context->client.pack, mask, kStippleSize, i, j,
               (context->state.stipple[j] >> i) & 1);
    }
  }
}

static void APIENTRY GetPointerv(GLenum pname, GLvoid** params) {
  Context* context = current;
  if (InsideBeginEnd(context)) {
    return;
  }
  const Array* arrays = context->client.arrays;
  switch (pname) {
    case GL_VERTEX_ARRAY_POINTER:
      *params = (GLvoid*)arrays[kVertexArray].pointer;
      break;
    case GL_NORMAL_ARRAY_POINTER:
      *params = (GLvoid*)arrays[kNormalArray].pointer;
      break;
    case GL_COLOR_ARRAY_POINTER:
      *params = (GLvoid*)arrays[kColorArray].pointer;
      break;
    case GL_INDEX_ARRAY_POINTER:
      *params = (GLvoid*)arrays[kIndexArray].pointer;
      break;
    case GL_TEXTURE_COORD_ARRAY_POINTER:
      *params = (GLvoid*)arrays[TexCoordArray(context)].pointer;
      break;
    case GL_EDGE_FLAG_ARRAY_POINTER:
      *params = (GLvoid*)arrays[kEdgeFlagArray].pointer;
      break;
    case GL_FEEDBACK_BUFFER_POINTER:
      *params = context->feedback.buffer;
      break;
    case GL_SELECTION_BUFFER_POINTER:
      *params = context->selection.buffer;
      break;
    default:
      SetError(context, GL_INVALID_ENUM);
  }
}

#define SOFTGPU_PROC(ret, name, params) [kGl##name] = (PROC)name,
static ProcTable table = {
    .count = kGlEntryCount,
    .entries = {SOFTGPU_GL11(SOFTGPU_PROC)},
};
#undef SOFTGPU_PROC

static void InitState(State* state) {
  static const Material kMaterial = {
      .ambient = {0.2f, 0.2f, 0.2f, 1.0f},
      .diffuse = {0.8f, 0.8f, 0.8f, 1.0f},
      .specular = {0.0f, 0.0f, 0.0f, 1.0f},
      .emission = {0.0f, 0.0f, 0.0f, 1.0f},
      .indexes = {0.0f, 1.0f, 1.0f},
  };
  static const Light kLight = {
      .ambient = {0.0f, 0.0f, 0.0f, 1.0f},
      .diffuse = {0.0f, 0.0f, 0.0f, 1.0f},
      .specular = {0.0f, 0.0f, 0.0f, 1.0f},
      .position = {0.0f, 0.0f, 1.0f, 0.0f},
      .direction = {0.0f, 0.0f, -1.0f},
      .cutoff = 180.0f,
      .attenuation = {1.0f, 0.0f, 0.0f},
  };
  static const TextureUnit kTextureUnit = {
      .env_mode = GL_MODULATE,
      .rgb =
          {
              .function = GL_MODULATE,
              .source = {GL_TEXTURE, GL_PREVIOUS_ARB, GL_CONSTANT_ARB},
              .operand = {GL_SRC_COLOR, GL_SRC_COLOR, GL_SRC_ALPHA},
              .scale = 1.0f,
          },
      .alpha =
          {
              .function = GL_MODULATE,
              .source = {GL_TEXTURE, GL_PREVIOUS_ARB, GL_CONSTANT_ARB},
              .operand = {GL_SRC_ALPHA, GL_SRC_ALPHA, GL_SRC_ALPHA},
              .scale = 1.0f,
          },
      .gen =
          {
              {.mode = GL_EYE_LINEAR,
               .object = {1.0f, 0.0f, 0.0f, 0.0f},
               .eye = {1.0f, 0.0f, 0.0f, 0.0f}},
              {.mode = GL_EYE_LINEAR,
               .object = {0.0f, 1.0f, 0.0f, 0.0f},
               .eye = {0.0f, 1.0f, 0.0f, 0.0f}},
              {.mode = GL_EYE_LINEAR},
              {.mode = GL_EYE_LINEAR},
          },
  };
  *state = (State){
      .current =
          {
              .color = {1.0f, 1.0f, 1.0f, 1.0f},
              .index = 1.0f,
              .normal = {0.0f, 0.0f, 1.0f},
              .edge_flag = true,
              .raster =
                  {
                      .window = {0.0f, 0.0f, 0.0f, 1.0f},
                      .valid = true,
                      .color = {1.0f, 1.0f, 1.0f, 1.0f},
                      .index = 1.0f,
                  },
          },
      .point = {.size = 1.0f},
      .line = {.width = 1.0f, .pattern = 0xFFFF, .repeat = 1},
      .polygon =
          {
              .cull_mode = GL_BACK,
              .front_face = GL_CCW,
              .mode = {GL_FILL, GL_FILL},
          },
      .pixel =
          {
              .scale = {1.0f, 1.0f, 1.0f, 1.0f},
              .depth_scale = 1.0f,
              .zoom = {1.0f, 1.0f},
              .read_buffer = GL_BACK,
          },
      .lighting =
          {
              .color_material_face = GL_FRONT_AND_BACK,
              .color_material_mode = GL_AMBIENT_AND_DIFFUSE,
              .material = {kMaterial, kMaterial},
              .model_ambient = {0.2f, 0.2f, 0.2f, 1.0f},
              .shade_model = GL_SMOOTH,
          },
      .fog = {.mode = GL_EXP, .density = 1.0f, .end = 1.0f},
      .depth = {.func = GL_LESS, .clear = 1.0f, .mask = true},
      .stencil =
          {
              .func = GL_ALWAYS,
              .value_mask = ~0u,
              .fail = GL_KEEP,
              .zfail = GL_KEEP,
              .zpass = GL_KEEP,
              .writemask = ~0u,
          },
      .viewport = {.depth_range = {0.0f, 1.0f}},
      .transform = {.matrix_mode = GL_MODELVIEW},
      .color =
          {
              .alpha_func = GL_ALWAYS,
              .blend_src = GL_ONE,
              .blend_dst = GL_ZERO,
              .dither = true,
              .logic_op = GL_COPY,
              .draw_buffer = GL_BACK,
              .index_writemask = ~0u,
              .mask = {true, true, true, true},
          },
      .hint =
          {
              .perspective = GL_DONT_CARE,
              .point = GL_DONT_CARE,
              .line = GL_DONT_CARE,
              .polygon = GL_DONT_CARE,
              .fog = GL_DONT_CARE,
          },
      .eval =
          {
              .segments1 = 1,
              .domain1 = {0.0f, 1.0f},
              .segments2 = {1, 1},
              .domain2 = {0.0f, 1.0f, 0.0f, 1.0f},
          },
  };
  for (int i = 0; i < kStippleSize; i++) {
    state->stipple[i] = UINT32_MAX;
  }
  for (int i = 0; i < kMaxLights; i++) {
    state->lighting.lights[i] = kLight;
  }
  float white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  memcpy(state->lighting.lights[0].diffuse, white, sizeof(white));
  memcpy(state->lighting.lights[0].specular, white, sizeof(white));
  for (int unit = 0; unit < kTextureUnits; unit++) {
    state->texture.units[unit] = kTextureUnit;
    state->current.texcoord[unit][3] = 1.0f;
    state->current.raster.texcoord[unit][3] = 1.0f;
  }
}

static void InitContext(Context* context) {
  static const float kMapDefaults[kMaps][4] = {
      {1, 1, 1, 1}, {1},          {0, 0, 1}, {0},          {0, 0},
      {0, 0, 0},    {0, 0, 0, 1}, {0, 0, 0}, {0, 0, 0, 1},
  };
  memset(context, 0, sizeof(*context));
  InitState(&context->state);
  for (int i = 0; i < kArrays; i++) {
    context->client.arrays[i] = (Array){
        .size = i == kNormalArray ? 3 : (i == kIndexArray ? 1 : 4),
        .type = GL_FLOAT,
    };
  }
  context->client.arrays[kEdgeFlagArray].size = 1;
  context->client.pack.alignment = 4;
  context->client.unpack.alignment = 4;
  for (int stack = 0; stack < kMatrixStacks; stack++) {
    memcpy(context->matrices[stack][0], kIdentity, sizeof(kIdentity));
  }
  context->normal_matrix_stale = true;
  for (int target = kTexture1D; target <= kTexture2D; target++) {
    context->default_textures[target] = (Texture){
        .target = target == kTexture1D ? GL_TEXTURE_1D : GL_TEXTURE_2D,
        .parameters = kDefaultParameters,
    };
    for (int unit = 0; unit < kTextureUnits; unit++) {
      context->bound[unit][target] = &context->default_textures[target];
    }
  }
  for (int i = 0; i < kPixelMaps; i++) {
    context->pixel_maps[i].size = 1;
  }
  for (int i = 0; i < kMaps; i++) {
    Map* maps[2] = {&context->maps1[i], &context->maps2[i]};
    for (int d = 0; d < 2; d++) {
      *maps[d] = (Map){.order = {1, 1}, .domain = {0.0f, 1.0f, 0.0f, 1.0f}};
      memcpy(maps[d]->points, kMapDefaults[i], sizeof(kMapDefaults[i]));
    }
  }
  context->render_mode = GL_RENDER;
  context->feedback.type = GL_2D;
  context->shared = Allocate(sizeof(Shared));
  *context->shared = (Shared){.references = 1};
}

static void ReleaseShared(Shared* shared) {
  if (--shared->references > 0) {
    return;
  }
  for (int i = 0; i < shared->textures.count; i++) {
    Texture* texture = shared->textures.entries[i].object;
    if (texture) {
      FreeLevels(texture);
      free(texture);
    }
  }
  for (int i = 0; i < shared->lists.count; i++) {
    FreeList(shared->lists.entries[i].object);
  }
  free(shared->textures.entries);
  free(shared->lists.entries);
  free(shared);
}

static void FreeFramebuffer(Framebuffer* framebuffer) {
  free(framebuffer->front);
  free(framebuffer->back);
  free(framebuffer->depth);
  free(framebuffer->stencil);
  free(framebuffer->accum);
  *framebuffer = (Framebuffer){.window = framebuffer->window,
                               .single_buffered = framebuffer->single_buffered,
                               .accumulation = framebuffer->accumulation,
                               .swap_interval = framebuffer->swap_interval};
}

static Context* Lookup(ULONG handle) {
  if (handle == 0 || handle > kMaxContexts || !atomic_load(&used[handle - 1])) {
    return NULL;
  }
  return &contexts[handle - 1];
}

static Framebuffer* FindFramebuffer(HWND window) {
  while (atomic_flag_test_and_set(&framebuffers_lock)) {
  }
  Framebuffer* found = NULL;
  Framebuffer* unused = NULL;
  for (int i = 0; i < kMaxFramebuffers; i++) {
    Framebuffer* framebuffer = &framebuffers[i];
    if (framebuffer->window && !IsWindow(framebuffer->window)) {
      FreeFramebuffer(framebuffer);
      framebuffer->window = NULL;
    }
    if (framebuffer->window == window) {
      found = framebuffer;
    } else if (!framebuffer->window && !unused) {
      unused = framebuffer;
    }
  }
  if (!found && unused) {
    found = unused;
    *found = (Framebuffer){.window = window, .swap_interval = 1};
  }
  atomic_flag_clear(&framebuffers_lock);
  return found;
}

static Framebuffer* AttachWindow(HDC hdc) {
  HWND window = WindowFromDC(hdc);
  RECT rect;
  if (!window || !GetClientRect(window, &rect)) {
    return NULL;
  }
  Framebuffer* framebuffer = FindFramebuffer(window);
  if (framebuffer && (rect.right != framebuffer->width ||
                      rect.bottom != framebuffer->height)) {
    FreeFramebuffer(framebuffer);
    framebuffer->width = rect.right;
    framebuffer->height = rect.bottom;
    size_t count = (size_t)rect.right * (size_t)rect.bottom;
    if (count) {
      framebuffer->front = Allocate(count * sizeof(uint32_t));
      framebuffer->back = Allocate(count * sizeof(uint32_t));
      framebuffer->depth = Allocate(count * sizeof(float));
      framebuffer->stencil = Allocate(count);
      memset(framebuffer->front, 0, count * sizeof(uint32_t));
      memset(framebuffer->back, 0, count * sizeof(uint32_t));
    }
  }
  return framebuffer;
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
  int accum_bits = format % 2 == 0 ? kAccumBits : 0;
  DWORD buffering =
      format < kSingleBufferedFormat ? PFD_DOUBLEBUFFER | PFD_SWAP_COPY : 0;
  *descriptor = (PIXELFORMATDESCRIPTOR){
      .nSize = sizeof(*descriptor),
      .nVersion = 1,
      .dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | buffering |
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
      .cAccumBits = 4 * accum_bits,
      .cAccumRedBits = accum_bits,
      .cAccumGreenBits = accum_bits,
      .cAccumBlueBits = accum_bits,
      .cAccumAlphaBits = accum_bits,
      .cDepthBits = kDepthBits,
      .cStencilBits = kStencilBits,
      .iLayerType = PFD_MAIN_PLANE,
  };
  return kPixelFormatCount;
}

BOOL APIENTRY DrvSetPixelFormat(HDC hdc, LONG format) {
  HWND window = WindowFromDC(hdc);
  Framebuffer* framebuffer = window ? FindFramebuffer(window) : NULL;
  if (!framebuffer || format < 1 || format > kPixelFormatCount) {
    return FALSE;
  }
  framebuffer->single_buffered = format >= kSingleBufferedFormat;
  framebuffer->accumulation = format % 2 == 0;
  return TRUE;
}

ULONG APIENTRY DrvCreateContext(HDC hdc) {
  for (ULONG i = 0; i < kMaxContexts; i++) {
    int expected = 0;
    if (atomic_compare_exchange_strong(&used[i], &expected, 1)) {
      InitContext(&contexts[i]);
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
  free(context->clip_buffer);
  free(context->corners);
  if (context->list) {
    FreeList(context->list);
  }
  for (int target = kTexture1D; target <= kTexture2D; target++) {
    FreeLevels(&context->default_textures[target]);
    for (int level = 0; level < kMaxLevels; level++) {
      free(context->proxies[target][level].texels);
    }
  }
  ReleaseShared(context->shared);
  memset(context, 0, sizeof(*context));
  atomic_store(&used[handle - 1], 0);
  return TRUE;
}

BOOL APIENTRY DrvCopyContext(ULONG source, ULONG destination, UINT mask) {
  Context* from = Lookup(source);
  Context* to = Lookup(destination);
  if (!from || !to) {
    return FALSE;
  }
  CopyGroups(&to->state, &from->state, mask);
  ResolveBindings(to);
  return TRUE;
}

BOOL APIENTRY DrvShareLists(ULONG first, ULONG second) {
  Context* source = Lookup(first);
  Context* target = Lookup(second);
  if (!source || !target) {
    return FALSE;
  }
  if (target->shared == source->shared) {
    return TRUE;
  }
  Shared* shared = target->shared;
  if (shared->references > 1 || shared->textures.count > 0 ||
      shared->lists.count > 0) {
    return FALSE;
  }
  ReleaseShared(shared);
  target->shared = source->shared;
  target->shared->references++;
  ResolveBindings(target);
  return TRUE;
}

ProcTable* APIENTRY DrvSetContext(HDC hdc, ULONG handle, PROC set_proc_table) {
  Context* context = Lookup(handle);
  Framebuffer* framebuffer = context ? AttachWindow(hdc) : NULL;
  if (!framebuffer) {
    return NULL;
  }
  context->hdc = hdc;
  context->framebuffer = framebuffer;
  if (!context->attached) {
    context->attached = true;
    context->state.viewport.box[2] = framebuffer->width;
    context->state.viewport.box[3] = framebuffer->height;
    context->state.scissor.box[2] = framebuffer->width;
    context->state.scissor.box[3] = framebuffer->height;
    if (framebuffer->single_buffered) {
      context->state.color.draw_buffer = GL_FRONT;
      context->state.pixel.read_buffer = GL_FRONT;
    }
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

static const char* WINAPI GetExtensionsStringARB(HDC hdc) {
  if (!GetObjectType(hdc)) {
    SetLastError(ERROR_DC_NOT_FOUND);
    return NULL;
  }
  return kWglExtensions;
}

static const char* WINAPI GetExtensionsStringEXT(void) {
  return kWglExtensions;
}

static BOOL WINAPI SwapIntervalEXT(int interval) {
  if (!current) {
    SetLastError(ERROR_DC_NOT_FOUND);
    return FALSE;
  }
  if (interval < 0) {
    SetLastError(ERROR_INVALID_DATA);
    return FALSE;
  }
  current->framebuffer->swap_interval = MinInt(interval, kMaxSwapInterval);
  return TRUE;
}

static int WINAPI GetSwapIntervalEXT(void) {
  return current ? current->framebuffer->swap_interval : 0;
}

#define SOFTGPU_PROC(ret, name, params) {"gl" #name, (PROC)name},
PROC APIENTRY DrvGetProcAddress(LPCSTR name) {
  static const struct {
    const char* name;
    PROC proc;
  } kProcs[] = {{"wglGetExtensionsStringARB", (PROC)GetExtensionsStringARB},
                {"wglGetExtensionsStringEXT", (PROC)GetExtensionsStringEXT},
                {"wglSwapIntervalEXT", (PROC)SwapIntervalEXT},
                {"wglGetSwapIntervalEXT", (PROC)GetSwapIntervalEXT},
                SOFTGPU_ARB_MULTITEXTURE(SOFTGPU_PROC)};
  for (size_t i = 0; name && i < sizeof(kProcs) / sizeof(kProcs[0]); i++) {
    if (strcmp(name, kProcs[i].name) == 0) {
      return kProcs[i].proc;
    }
  }
  return NULL;
}
#undef SOFTGPU_PROC

BOOL APIENTRY DrvSwapBuffers(HDC hdc) {
  HWND window = WindowFromDC(hdc);
  Framebuffer* framebuffer = window ? FindFramebuffer(window) : NULL;
  if (!framebuffer) {
    return FALSE;
  }
  if (framebuffer->back && !framebuffer->single_buffered) {
    memcpy(framebuffer->front, framebuffer->back,
           (size_t)framebuffer->width * (size_t)framebuffer->height *
               sizeof(uint32_t));
    Present(framebuffer, hdc, framebuffer->front);
    framebuffer->front_dirty = false;
  }
  for (int i = 0; i < framebuffer->swap_interval; i++) {
    if (FAILED(DwmFlush())) {
      break;
    }
  }
  return AttachWindow(hdc) != NULL;
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
