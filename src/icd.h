#ifndef SOFTGPU_ICD_H
#define SOFTGPU_ICD_H

#include <windows.h>

#ifdef SOFTGPU_BUILD
#define SOFTGPU_API __declspec(dllexport)
#else
#define SOFTGPU_API __declspec(dllimport)
#endif

#include <GL/gl.h>

#define SOFTGPU_GL11(X)     \
  X(NewList)                \
  X(EndList)                \
  X(CallList)               \
  X(CallLists)              \
  X(DeleteLists)            \
  X(GenLists)               \
  X(ListBase)               \
  X(Begin)                  \
  X(Bitmap)                 \
  X(Color3b)                \
  X(Color3bv)               \
  X(Color3d)                \
  X(Color3dv)               \
  X(Color3f)                \
  X(Color3fv)               \
  X(Color3i)                \
  X(Color3iv)               \
  X(Color3s)                \
  X(Color3sv)               \
  X(Color3ub)               \
  X(Color3ubv)              \
  X(Color3ui)               \
  X(Color3uiv)              \
  X(Color3us)               \
  X(Color3usv)              \
  X(Color4b)                \
  X(Color4bv)               \
  X(Color4d)                \
  X(Color4dv)               \
  X(Color4f)                \
  X(Color4fv)               \
  X(Color4i)                \
  X(Color4iv)               \
  X(Color4s)                \
  X(Color4sv)               \
  X(Color4ub)               \
  X(Color4ubv)              \
  X(Color4ui)               \
  X(Color4uiv)              \
  X(Color4us)               \
  X(Color4usv)              \
  X(EdgeFlag)               \
  X(EdgeFlagv)              \
  X(End)                    \
  X(Indexd)                 \
  X(Indexdv)                \
  X(Indexf)                 \
  X(Indexfv)                \
  X(Indexi)                 \
  X(Indexiv)                \
  X(Indexs)                 \
  X(Indexsv)                \
  X(Normal3b)               \
  X(Normal3bv)              \
  X(Normal3d)               \
  X(Normal3dv)              \
  X(Normal3f)               \
  X(Normal3fv)              \
  X(Normal3i)               \
  X(Normal3iv)              \
  X(Normal3s)               \
  X(Normal3sv)              \
  X(RasterPos2d)            \
  X(RasterPos2dv)           \
  X(RasterPos2f)            \
  X(RasterPos2fv)           \
  X(RasterPos2i)            \
  X(RasterPos2iv)           \
  X(RasterPos2s)            \
  X(RasterPos2sv)           \
  X(RasterPos3d)            \
  X(RasterPos3dv)           \
  X(RasterPos3f)            \
  X(RasterPos3fv)           \
  X(RasterPos3i)            \
  X(RasterPos3iv)           \
  X(RasterPos3s)            \
  X(RasterPos3sv)           \
  X(RasterPos4d)            \
  X(RasterPos4dv)           \
  X(RasterPos4f)            \
  X(RasterPos4fv)           \
  X(RasterPos4i)            \
  X(RasterPos4iv)           \
  X(RasterPos4s)            \
  X(RasterPos4sv)           \
  X(Rectd)                  \
  X(Rectdv)                 \
  X(Rectf)                  \
  X(Rectfv)                 \
  X(Recti)                  \
  X(Rectiv)                 \
  X(Rects)                  \
  X(Rectsv)                 \
  X(TexCoord1d)             \
  X(TexCoord1dv)            \
  X(TexCoord1f)             \
  X(TexCoord1fv)            \
  X(TexCoord1i)             \
  X(TexCoord1iv)            \
  X(TexCoord1s)             \
  X(TexCoord1sv)            \
  X(TexCoord2d)             \
  X(TexCoord2dv)            \
  X(TexCoord2f)             \
  X(TexCoord2fv)            \
  X(TexCoord2i)             \
  X(TexCoord2iv)            \
  X(TexCoord2s)             \
  X(TexCoord2sv)            \
  X(TexCoord3d)             \
  X(TexCoord3dv)            \
  X(TexCoord3f)             \
  X(TexCoord3fv)            \
  X(TexCoord3i)             \
  X(TexCoord3iv)            \
  X(TexCoord3s)             \
  X(TexCoord3sv)            \
  X(TexCoord4d)             \
  X(TexCoord4dv)            \
  X(TexCoord4f)             \
  X(TexCoord4fv)            \
  X(TexCoord4i)             \
  X(TexCoord4iv)            \
  X(TexCoord4s)             \
  X(TexCoord4sv)            \
  X(Vertex2d)               \
  X(Vertex2dv)              \
  X(Vertex2f)               \
  X(Vertex2fv)              \
  X(Vertex2i)               \
  X(Vertex2iv)              \
  X(Vertex2s)               \
  X(Vertex2sv)              \
  X(Vertex3d)               \
  X(Vertex3dv)              \
  X(Vertex3f)               \
  X(Vertex3fv)              \
  X(Vertex3i)               \
  X(Vertex3iv)              \
  X(Vertex3s)               \
  X(Vertex3sv)              \
  X(Vertex4d)               \
  X(Vertex4dv)              \
  X(Vertex4f)               \
  X(Vertex4fv)              \
  X(Vertex4i)               \
  X(Vertex4iv)              \
  X(Vertex4s)               \
  X(Vertex4sv)              \
  X(ClipPlane)              \
  X(ColorMaterial)          \
  X(CullFace)               \
  X(Fogf)                   \
  X(Fogfv)                  \
  X(Fogi)                   \
  X(Fogiv)                  \
  X(FrontFace)              \
  X(Hint)                   \
  X(Lightf)                 \
  X(Lightfv)                \
  X(Lighti)                 \
  X(Lightiv)                \
  X(LightModelf)            \
  X(LightModelfv)           \
  X(LightModeli)            \
  X(LightModeliv)           \
  X(LineStipple)            \
  X(LineWidth)              \
  X(Materialf)              \
  X(Materialfv)             \
  X(Materiali)              \
  X(Materialiv)             \
  X(PointSize)              \
  X(PolygonMode)            \
  X(PolygonStipple)         \
  X(Scissor)                \
  X(ShadeModel)             \
  X(TexParameterf)          \
  X(TexParameterfv)         \
  X(TexParameteri)          \
  X(TexParameteriv)         \
  X(TexImage1D)             \
  X(TexImage2D)             \
  X(TexEnvf)                \
  X(TexEnvfv)               \
  X(TexEnvi)                \
  X(TexEnviv)               \
  X(TexGend)                \
  X(TexGendv)               \
  X(TexGenf)                \
  X(TexGenfv)               \
  X(TexGeni)                \
  X(TexGeniv)               \
  X(FeedbackBuffer)         \
  X(SelectBuffer)           \
  X(RenderMode)             \
  X(InitNames)              \
  X(LoadName)               \
  X(PassThrough)            \
  X(PopName)                \
  X(PushName)               \
  X(DrawBuffer)             \
  X(Clear)                  \
  X(ClearAccum)             \
  X(ClearIndex)             \
  X(ClearColor)             \
  X(ClearStencil)           \
  X(ClearDepth)             \
  X(StencilMask)            \
  X(ColorMask)              \
  X(DepthMask)              \
  X(IndexMask)              \
  X(Accum)                  \
  X(Disable)                \
  X(Enable)                 \
  X(Finish)                 \
  X(Flush)                  \
  X(PopAttrib)              \
  X(PushAttrib)             \
  X(Map1d)                  \
  X(Map1f)                  \
  X(Map2d)                  \
  X(Map2f)                  \
  X(MapGrid1d)              \
  X(MapGrid1f)              \
  X(MapGrid2d)              \
  X(MapGrid2f)              \
  X(EvalCoord1d)            \
  X(EvalCoord1dv)           \
  X(EvalCoord1f)            \
  X(EvalCoord1fv)           \
  X(EvalCoord2d)            \
  X(EvalCoord2dv)           \
  X(EvalCoord2f)            \
  X(EvalCoord2fv)           \
  X(EvalMesh1)              \
  X(EvalPoint1)             \
  X(EvalMesh2)              \
  X(EvalPoint2)             \
  X(AlphaFunc)              \
  X(BlendFunc)              \
  X(LogicOp)                \
  X(StencilFunc)            \
  X(StencilOp)              \
  X(DepthFunc)              \
  X(PixelZoom)              \
  X(PixelTransferf)         \
  X(PixelTransferi)         \
  X(PixelStoref)            \
  X(PixelStorei)            \
  X(PixelMapfv)             \
  X(PixelMapuiv)            \
  X(PixelMapusv)            \
  X(ReadBuffer)             \
  X(CopyPixels)             \
  X(ReadPixels)             \
  X(DrawPixels)             \
  X(GetBooleanv)            \
  X(GetClipPlane)           \
  X(GetDoublev)             \
  X(GetError)               \
  X(GetFloatv)              \
  X(GetIntegerv)            \
  X(GetLightfv)             \
  X(GetLightiv)             \
  X(GetMapdv)               \
  X(GetMapfv)               \
  X(GetMapiv)               \
  X(GetMaterialfv)          \
  X(GetMaterialiv)          \
  X(GetPixelMapfv)          \
  X(GetPixelMapuiv)         \
  X(GetPixelMapusv)         \
  X(GetPolygonStipple)      \
  X(GetString)              \
  X(GetTexEnvfv)            \
  X(GetTexEnviv)            \
  X(GetTexGendv)            \
  X(GetTexGenfv)            \
  X(GetTexGeniv)            \
  X(GetTexImage)            \
  X(GetTexParameterfv)      \
  X(GetTexParameteriv)      \
  X(GetTexLevelParameterfv) \
  X(GetTexLevelParameteriv) \
  X(IsEnabled)              \
  X(IsList)                 \
  X(DepthRange)             \
  X(Frustum)                \
  X(LoadIdentity)           \
  X(LoadMatrixf)            \
  X(LoadMatrixd)            \
  X(MatrixMode)             \
  X(MultMatrixf)            \
  X(MultMatrixd)            \
  X(Ortho)                  \
  X(PopMatrix)              \
  X(PushMatrix)             \
  X(Rotated)                \
  X(Rotatef)                \
  X(Scaled)                 \
  X(Scalef)                 \
  X(Translated)             \
  X(Translatef)             \
  X(Viewport)               \
  X(ArrayElement)           \
  X(BindTexture)            \
  X(ColorPointer)           \
  X(DisableClientState)     \
  X(DrawArrays)             \
  X(DrawElements)           \
  X(EdgeFlagPointer)        \
  X(EnableClientState)      \
  X(IndexPointer)           \
  X(Indexub)                \
  X(Indexubv)               \
  X(InterleavedArrays)      \
  X(NormalPointer)          \
  X(PolygonOffset)          \
  X(TexCoordPointer)        \
  X(VertexPointer)          \
  X(AreTexturesResident)    \
  X(CopyTexImage1D)         \
  X(CopyTexImage2D)         \
  X(CopyTexSubImage1D)      \
  X(CopyTexSubImage2D)      \
  X(DeleteTextures)         \
  X(GenTextures)            \
  X(GetPointerv)            \
  X(IsTexture)              \
  X(PrioritizeTextures)     \
  X(TexSubImage1D)          \
  X(TexSubImage2D)          \
  X(PopClientAttrib)        \
  X(PushClientAttrib)

#define SOFTGPU_ENTRY(name) kGl##name,
typedef enum GlEntry { SOFTGPU_GL11(SOFTGPU_ENTRY) kGlEntryCount } GlEntry;
#undef SOFTGPU_ENTRY

typedef struct ProcTable {
  int count;
  PROC entries[kGlEntryCount];
} ProcTable;

SOFTGPU_API BOOL APIENTRY DrvValidateVersion(ULONG version);
SOFTGPU_API void APIENTRY DrvSetCallbackProcs(INT count, PROC* procs);
SOFTGPU_API LONG APIENTRY DrvDescribePixelFormat(
    HDC hdc, INT format, ULONG size, PIXELFORMATDESCRIPTOR* descriptor);
SOFTGPU_API BOOL APIENTRY DrvSetPixelFormat(HDC hdc, LONG format);
SOFTGPU_API ULONG APIENTRY DrvCreateContext(HDC hdc);
SOFTGPU_API ULONG APIENTRY DrvCreateLayerContext(HDC hdc, INT layer);
SOFTGPU_API BOOL APIENTRY DrvDeleteContext(ULONG handle);
SOFTGPU_API BOOL APIENTRY DrvCopyContext(ULONG source, ULONG destination,
                                         UINT mask);
SOFTGPU_API BOOL APIENTRY DrvShareLists(ULONG first, ULONG second);
SOFTGPU_API ProcTable* APIENTRY DrvSetContext(HDC hdc, ULONG handle,
                                              PROC set_proc_table);
SOFTGPU_API BOOL APIENTRY DrvReleaseContext(ULONG handle);
SOFTGPU_API PROC APIENTRY DrvGetProcAddress(LPCSTR name);
SOFTGPU_API BOOL APIENTRY DrvSwapBuffers(HDC hdc);
SOFTGPU_API BOOL APIENTRY DrvSwapLayerBuffers(HDC hdc, UINT planes);
SOFTGPU_API BOOL APIENTRY DrvDescribeLayerPlane(HDC hdc, INT format, INT layer,
                                                UINT size,
                                                LAYERPLANEDESCRIPTOR* plane);
SOFTGPU_API INT APIENTRY DrvSetLayerPaletteEntries(HDC hdc, INT layer,
                                                   INT start, INT count,
                                                   const COLORREF* colors);
SOFTGPU_API INT APIENTRY DrvGetLayerPaletteEntries(HDC hdc, INT layer,
                                                   INT start, INT count,
                                                   COLORREF* colors);
SOFTGPU_API BOOL APIENTRY DrvRealizeLayerPalette(HDC hdc, INT layer,
                                                 BOOL realize);

#endif
