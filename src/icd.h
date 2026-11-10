#ifndef SOFTGPU_ICD_H
#define SOFTGPU_ICD_H

#include <windows.h>

#define SOFTGPU_GL11(X)                                                       \
  X(void, NewList, (GLuint list, GLenum mode), )                              \
  X(void, EndList, (void), )                                                  \
  X(void, CallList, (GLuint list), )                                          \
  X(void, CallLists, (GLsizei n, GLenum type, const GLvoid* lists), )         \
  X(void, DeleteLists, (GLuint list, GLsizei range), )                        \
  X(GLuint, GenLists, (GLsizei range), 0)                                     \
  X(void, ListBase, (GLuint base), )                                          \
  X(void, Begin, (GLenum mode), )                                             \
  X(void, Bitmap,                                                             \
    (GLsizei width, GLsizei height, GLfloat xorig, GLfloat yorig,             \
     GLfloat xmove, GLfloat ymove, const GLubyte* bitmap), )                  \
  X(void, Color3b, (GLbyte red, GLbyte green, GLbyte blue), )                 \
  X(void, Color3bv, (const GLbyte* v), )                                      \
  X(void, Color3d, (GLdouble red, GLdouble green, GLdouble blue), )           \
  X(void, Color3dv, (const GLdouble* v), )                                    \
  X(void, Color3f, (GLfloat red, GLfloat green, GLfloat blue), )              \
  X(void, Color3fv, (const GLfloat* v), )                                     \
  X(void, Color3i, (GLint red, GLint green, GLint blue), )                    \
  X(void, Color3iv, (const GLint* v), )                                       \
  X(void, Color3s, (GLshort red, GLshort green, GLshort blue), )              \
  X(void, Color3sv, (const GLshort* v), )                                     \
  X(void, Color3ub, (GLubyte red, GLubyte green, GLubyte blue), )             \
  X(void, Color3ubv, (const GLubyte* v), )                                    \
  X(void, Color3ui, (GLuint red, GLuint green, GLuint blue), )                \
  X(void, Color3uiv, (const GLuint* v), )                                     \
  X(void, Color3us, (GLushort red, GLushort green, GLushort blue), )          \
  X(void, Color3usv, (const GLushort* v), )                                   \
  X(void, Color4b, (GLbyte red, GLbyte green, GLbyte blue, GLbyte alpha), )   \
  X(void, Color4bv, (const GLbyte* v), )                                      \
  X(void, Color4d,                                                            \
    (GLdouble red, GLdouble green, GLdouble blue, GLdouble alpha), )          \
  X(void, Color4dv, (const GLdouble* v), )                                    \
  X(void, Color4f,                                                            \
    (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha), )              \
  X(void, Color4fv, (const GLfloat* v), )                                     \
  X(void, Color4i, (GLint red, GLint green, GLint blue, GLint alpha), )       \
  X(void, Color4iv, (const GLint* v), )                                       \
  X(void, Color4s,                                                            \
    (GLshort red, GLshort green, GLshort blue, GLshort alpha), )              \
  X(void, Color4sv, (const GLshort* v), )                                     \
  X(void, Color4ub,                                                           \
    (GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha), )              \
  X(void, Color4ubv, (const GLubyte* v), )                                    \
  X(void, Color4ui, (GLuint red, GLuint green, GLuint blue, GLuint alpha), )  \
  X(void, Color4uiv, (const GLuint* v), )                                     \
  X(void, Color4us,                                                           \
    (GLushort red, GLushort green, GLushort blue, GLushort alpha), )          \
  X(void, Color4usv, (const GLushort* v), )                                   \
  X(void, EdgeFlag, (GLboolean flag), )                                       \
  X(void, EdgeFlagv, (const GLboolean* flag), )                               \
  X(void, End, (void), )                                                      \
  X(void, Indexd, (GLdouble c), )                                             \
  X(void, Indexdv, (const GLdouble* c), )                                     \
  X(void, Indexf, (GLfloat c), )                                              \
  X(void, Indexfv, (const GLfloat* c), )                                      \
  X(void, Indexi, (GLint c), )                                                \
  X(void, Indexiv, (const GLint* c), )                                        \
  X(void, Indexs, (GLshort c), )                                              \
  X(void, Indexsv, (const GLshort* c), )                                      \
  X(void, Normal3b, (GLbyte nx, GLbyte ny, GLbyte nz), )                      \
  X(void, Normal3bv, (const GLbyte* v), )                                     \
  X(void, Normal3d, (GLdouble nx, GLdouble ny, GLdouble nz), )                \
  X(void, Normal3dv, (const GLdouble* v), )                                   \
  X(void, Normal3f, (GLfloat nx, GLfloat ny, GLfloat nz), )                   \
  X(void, Normal3fv, (const GLfloat* v), )                                    \
  X(void, Normal3i, (GLint nx, GLint ny, GLint nz), )                         \
  X(void, Normal3iv, (const GLint* v), )                                      \
  X(void, Normal3s, (GLshort nx, GLshort ny, GLshort nz), )                   \
  X(void, Normal3sv, (const GLshort* v), )                                    \
  X(void, RasterPos2d, (GLdouble x, GLdouble y), )                            \
  X(void, RasterPos2dv, (const GLdouble* v), )                                \
  X(void, RasterPos2f, (GLfloat x, GLfloat y), )                              \
  X(void, RasterPos2fv, (const GLfloat* v), )                                 \
  X(void, RasterPos2i, (GLint x, GLint y), )                                  \
  X(void, RasterPos2iv, (const GLint* v), )                                   \
  X(void, RasterPos2s, (GLshort x, GLshort y), )                              \
  X(void, RasterPos2sv, (const GLshort* v), )                                 \
  X(void, RasterPos3d, (GLdouble x, GLdouble y, GLdouble z), )                \
  X(void, RasterPos3dv, (const GLdouble* v), )                                \
  X(void, RasterPos3f, (GLfloat x, GLfloat y, GLfloat z), )                   \
  X(void, RasterPos3fv, (const GLfloat* v), )                                 \
  X(void, RasterPos3i, (GLint x, GLint y, GLint z), )                         \
  X(void, RasterPos3iv, (const GLint* v), )                                   \
  X(void, RasterPos3s, (GLshort x, GLshort y, GLshort z), )                   \
  X(void, RasterPos3sv, (const GLshort* v), )                                 \
  X(void, RasterPos4d, (GLdouble x, GLdouble y, GLdouble z, GLdouble w), )    \
  X(void, RasterPos4dv, (const GLdouble* v), )                                \
  X(void, RasterPos4f, (GLfloat x, GLfloat y, GLfloat z, GLfloat w), )        \
  X(void, RasterPos4fv, (const GLfloat* v), )                                 \
  X(void, RasterPos4i, (GLint x, GLint y, GLint z, GLint w), )                \
  X(void, RasterPos4iv, (const GLint* v), )                                   \
  X(void, RasterPos4s, (GLshort x, GLshort y, GLshort z, GLshort w), )        \
  X(void, RasterPos4sv, (const GLshort* v), )                                 \
  X(void, Rectd, (GLdouble x1, GLdouble y1, GLdouble x2, GLdouble y2), )      \
  X(void, Rectdv, (const GLdouble* v1, const GLdouble* v2), )                 \
  X(void, Rectf, (GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2), )          \
  X(void, Rectfv, (const GLfloat* v1, const GLfloat* v2), )                   \
  X(void, Recti, (GLint x1, GLint y1, GLint x2, GLint y2), )                  \
  X(void, Rectiv, (const GLint* v1, const GLint* v2), )                       \
  X(void, Rects, (GLshort x1, GLshort y1, GLshort x2, GLshort y2), )          \
  X(void, Rectsv, (const GLshort* v1, const GLshort* v2), )                   \
  X(void, TexCoord1d, (GLdouble s), )                                         \
  X(void, TexCoord1dv, (const GLdouble* v), )                                 \
  X(void, TexCoord1f, (GLfloat s), )                                          \
  X(void, TexCoord1fv, (const GLfloat* v), )                                  \
  X(void, TexCoord1i, (GLint s), )                                            \
  X(void, TexCoord1iv, (const GLint* v), )                                    \
  X(void, TexCoord1s, (GLshort s), )                                          \
  X(void, TexCoord1sv, (const GLshort* v), )                                  \
  X(void, TexCoord2d, (GLdouble s, GLdouble t), )                             \
  X(void, TexCoord2dv, (const GLdouble* v), )                                 \
  X(void, TexCoord2f, (GLfloat s, GLfloat t), )                               \
  X(void, TexCoord2fv, (const GLfloat* v), )                                  \
  X(void, TexCoord2i, (GLint s, GLint t), )                                   \
  X(void, TexCoord2iv, (const GLint* v), )                                    \
  X(void, TexCoord2s, (GLshort s, GLshort t), )                               \
  X(void, TexCoord2sv, (const GLshort* v), )                                  \
  X(void, TexCoord3d, (GLdouble s, GLdouble t, GLdouble r), )                 \
  X(void, TexCoord3dv, (const GLdouble* v), )                                 \
  X(void, TexCoord3f, (GLfloat s, GLfloat t, GLfloat r), )                    \
  X(void, TexCoord3fv, (const GLfloat* v), )                                  \
  X(void, TexCoord3i, (GLint s, GLint t, GLint r), )                          \
  X(void, TexCoord3iv, (const GLint* v), )                                    \
  X(void, TexCoord3s, (GLshort s, GLshort t, GLshort r), )                    \
  X(void, TexCoord3sv, (const GLshort* v), )                                  \
  X(void, TexCoord4d, (GLdouble s, GLdouble t, GLdouble r, GLdouble q), )     \
  X(void, TexCoord4dv, (const GLdouble* v), )                                 \
  X(void, TexCoord4f, (GLfloat s, GLfloat t, GLfloat r, GLfloat q), )         \
  X(void, TexCoord4fv, (const GLfloat* v), )                                  \
  X(void, TexCoord4i, (GLint s, GLint t, GLint r, GLint q), )                 \
  X(void, TexCoord4iv, (const GLint* v), )                                    \
  X(void, TexCoord4s, (GLshort s, GLshort t, GLshort r, GLshort q), )         \
  X(void, TexCoord4sv, (const GLshort* v), )                                  \
  X(void, Vertex2d, (GLdouble x, GLdouble y), )                               \
  X(void, Vertex2dv, (const GLdouble* v), )                                   \
  X(void, Vertex2f, (GLfloat x, GLfloat y), )                                 \
  X(void, Vertex2fv, (const GLfloat* v), )                                    \
  X(void, Vertex2i, (GLint x, GLint y), )                                     \
  X(void, Vertex2iv, (const GLint* v), )                                      \
  X(void, Vertex2s, (GLshort x, GLshort y), )                                 \
  X(void, Vertex2sv, (const GLshort* v), )                                    \
  X(void, Vertex3d, (GLdouble x, GLdouble y, GLdouble z), )                   \
  X(void, Vertex3dv, (const GLdouble* v), )                                   \
  X(void, Vertex3f, (GLfloat x, GLfloat y, GLfloat z), )                      \
  X(void, Vertex3fv, (const GLfloat* v), )                                    \
  X(void, Vertex3i, (GLint x, GLint y, GLint z), )                            \
  X(void, Vertex3iv, (const GLint* v), )                                      \
  X(void, Vertex3s, (GLshort x, GLshort y, GLshort z), )                      \
  X(void, Vertex3sv, (const GLshort* v), )                                    \
  X(void, Vertex4d, (GLdouble x, GLdouble y, GLdouble z, GLdouble w), )       \
  X(void, Vertex4dv, (const GLdouble* v), )                                   \
  X(void, Vertex4f, (GLfloat x, GLfloat y, GLfloat z, GLfloat w), )           \
  X(void, Vertex4fv, (const GLfloat* v), )                                    \
  X(void, Vertex4i, (GLint x, GLint y, GLint z, GLint w), )                   \
  X(void, Vertex4iv, (const GLint* v), )                                      \
  X(void, Vertex4s, (GLshort x, GLshort y, GLshort z, GLshort w), )           \
  X(void, Vertex4sv, (const GLshort* v), )                                    \
  X(void, ClipPlane, (GLenum plane, const GLdouble* equation), )              \
  X(void, ColorMaterial, (GLenum face, GLenum mode), )                        \
  X(void, CullFace, (GLenum mode), )                                          \
  X(void, Fogf, (GLenum pname, GLfloat param), )                              \
  X(void, Fogfv, (GLenum pname, const GLfloat* params), )                     \
  X(void, Fogi, (GLenum pname, GLint param), )                                \
  X(void, Fogiv, (GLenum pname, const GLint* params), )                       \
  X(void, FrontFace, (GLenum mode), )                                         \
  X(void, Hint, (GLenum target, GLenum mode), )                               \
  X(void, Lightf, (GLenum light, GLenum pname, GLfloat param), )              \
  X(void, Lightfv, (GLenum light, GLenum pname, const GLfloat* params), )     \
  X(void, Lighti, (GLenum light, GLenum pname, GLint param), )                \
  X(void, Lightiv, (GLenum light, GLenum pname, const GLint* params), )       \
  X(void, LightModelf, (GLenum pname, GLfloat param), )                       \
  X(void, LightModelfv, (GLenum pname, const GLfloat* params), )              \
  X(void, LightModeli, (GLenum pname, GLint param), )                         \
  X(void, LightModeliv, (GLenum pname, const GLint* params), )                \
  X(void, LineStipple, (GLint factor, GLushort pattern), )                    \
  X(void, LineWidth, (GLfloat width), )                                       \
  X(void, Materialf, (GLenum face, GLenum pname, GLfloat param), )            \
  X(void, Materialfv, (GLenum face, GLenum pname, const GLfloat* params), )   \
  X(void, Materiali, (GLenum face, GLenum pname, GLint param), )              \
  X(void, Materialiv, (GLenum face, GLenum pname, const GLint* params), )     \
  X(void, PointSize, (GLfloat size), )                                        \
  X(void, PolygonMode, (GLenum face, GLenum mode), )                          \
  X(void, PolygonStipple, (const GLubyte* mask), )                            \
  X(void, Scissor, (GLint x, GLint y, GLsizei width, GLsizei height), )       \
  X(void, ShadeModel, (GLenum mode), )                                        \
  X(void, TexParameterf, (GLenum target, GLenum pname, GLfloat param), )      \
  X(void, TexParameterfv,                                                     \
    (GLenum target, GLenum pname, const GLfloat* params), )                   \
  X(void, TexParameteri, (GLenum target, GLenum pname, GLint param), )        \
  X(void, TexParameteriv,                                                     \
    (GLenum target, GLenum pname, const GLint* params), )                     \
  X(void, TexImage1D,                                                         \
    (GLenum target, GLint level, GLint components, GLsizei width,             \
     GLint border, GLenum format, GLenum type, const GLvoid* pixels), )       \
  X(void, TexImage2D,                                                         \
    (GLenum target, GLint level, GLint components, GLsizei width,             \
     GLsizei height, GLint border, GLenum format, GLenum type,                \
     const GLvoid* pixels), )                                                 \
  X(void, TexEnvf, (GLenum target, GLenum pname, GLfloat param), )            \
  X(void, TexEnvfv, (GLenum target, GLenum pname, const GLfloat* params), )   \
  X(void, TexEnvi, (GLenum target, GLenum pname, GLint param), )              \
  X(void, TexEnviv, (GLenum target, GLenum pname, const GLint* params), )     \
  X(void, TexGend, (GLenum coord, GLenum pname, GLdouble param), )            \
  X(void, TexGendv, (GLenum coord, GLenum pname, const GLdouble* params), )   \
  X(void, TexGenf, (GLenum coord, GLenum pname, GLfloat param), )             \
  X(void, TexGenfv, (GLenum coord, GLenum pname, const GLfloat* params), )    \
  X(void, TexGeni, (GLenum coord, GLenum pname, GLint param), )               \
  X(void, TexGeniv, (GLenum coord, GLenum pname, const GLint* params), )      \
  X(void, FeedbackBuffer, (GLsizei size, GLenum type, GLfloat * buffer), )    \
  X(void, SelectBuffer, (GLsizei size, GLuint * buffer), )                    \
  X(GLint, RenderMode, (GLenum mode), 0)                                      \
  X(void, InitNames, (void), )                                                \
  X(void, LoadName, (GLuint name), )                                          \
  X(void, PassThrough, (GLfloat token), )                                     \
  X(void, PopName, (void), )                                                  \
  X(void, PushName, (GLuint name), )                                          \
  X(void, DrawBuffer, (GLenum mode), )                                        \
  X(void, Clear, (GLbitfield mask), )                                         \
  X(void, ClearAccum,                                                         \
    (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha), )              \
  X(void, ClearIndex, (GLfloat c), )                                          \
  X(void, ClearColor,                                                         \
    (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha), )          \
  X(void, ClearStencil, (GLint s), )                                          \
  X(void, ClearDepth, (GLclampd depth), )                                     \
  X(void, StencilMask, (GLuint mask), )                                       \
  X(void, ColorMask,                                                          \
    (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha), )      \
  X(void, DepthMask, (GLboolean flag), )                                      \
  X(void, IndexMask, (GLuint mask), )                                         \
  X(void, Accum, (GLenum op, GLfloat value), )                                \
  X(void, Disable, (GLenum cap), )                                            \
  X(void, Enable, (GLenum cap), )                                             \
  X(void, Finish, (void), )                                                   \
  X(void, Flush, (void), )                                                    \
  X(void, PopAttrib, (void), )                                                \
  X(void, PushAttrib, (GLbitfield mask), )                                    \
  X(void, Map1d,                                                              \
    (GLenum target, GLdouble u1, GLdouble u2, GLint stride, GLint order,      \
     const GLdouble* points), )                                               \
  X(void, Map1f,                                                              \
    (GLenum target, GLfloat u1, GLfloat u2, GLint stride, GLint order,        \
     const GLfloat* points), )                                                \
  X(void, Map2d,                                                              \
    (GLenum target, GLdouble u1, GLdouble u2, GLint ustride, GLint uorder,    \
     GLdouble v1, GLdouble v2, GLint vstride, GLint vorder,                   \
     const GLdouble* points), )                                               \
  X(void, Map2f,                                                              \
    (GLenum target, GLfloat u1, GLfloat u2, GLint ustride, GLint uorder,      \
     GLfloat v1, GLfloat v2, GLint vstride, GLint vorder,                     \
     const GLfloat* points), )                                                \
  X(void, MapGrid1d, (GLint un, GLdouble u1, GLdouble u2), )                  \
  X(void, MapGrid1f, (GLint un, GLfloat u1, GLfloat u2), )                    \
  X(void, MapGrid2d,                                                          \
    (GLint un, GLdouble u1, GLdouble u2, GLint vn, GLdouble v1,               \
     GLdouble v2), )                                                          \
  X(void, MapGrid2f,                                                          \
    (GLint un, GLfloat u1, GLfloat u2, GLint vn, GLfloat v1, GLfloat v2), )   \
  X(void, EvalCoord1d, (GLdouble u), )                                        \
  X(void, EvalCoord1dv, (const GLdouble* u), )                                \
  X(void, EvalCoord1f, (GLfloat u), )                                         \
  X(void, EvalCoord1fv, (const GLfloat* u), )                                 \
  X(void, EvalCoord2d, (GLdouble u, GLdouble v), )                            \
  X(void, EvalCoord2dv, (const GLdouble* u), )                                \
  X(void, EvalCoord2f, (GLfloat u, GLfloat v), )                              \
  X(void, EvalCoord2fv, (const GLfloat* u), )                                 \
  X(void, EvalMesh1, (GLenum mode, GLint i1, GLint i2), )                     \
  X(void, EvalPoint1, (GLint i), )                                            \
  X(void, EvalMesh2, (GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2), ) \
  X(void, EvalPoint2, (GLint i, GLint j), )                                   \
  X(void, AlphaFunc, (GLenum func, GLclampf ref), )                           \
  X(void, BlendFunc, (GLenum sfactor, GLenum dfactor), )                      \
  X(void, LogicOp, (GLenum opcode), )                                         \
  X(void, StencilFunc, (GLenum func, GLint ref, GLuint mask), )               \
  X(void, StencilOp, (GLenum fail, GLenum zfail, GLenum zpass), )             \
  X(void, DepthFunc, (GLenum func), )                                         \
  X(void, PixelZoom, (GLfloat xfactor, GLfloat yfactor), )                    \
  X(void, PixelTransferf, (GLenum pname, GLfloat param), )                    \
  X(void, PixelTransferi, (GLenum pname, GLint param), )                      \
  X(void, PixelStoref, (GLenum pname, GLfloat param), )                       \
  X(void, PixelStorei, (GLenum pname, GLint param), )                         \
  X(void, PixelMapfv, (GLenum map, GLint mapsize, const GLfloat* values), )   \
  X(void, PixelMapuiv, (GLenum map, GLint mapsize, const GLuint* values), )   \
  X(void, PixelMapusv, (GLenum map, GLint mapsize, const GLushort* values), ) \
  X(void, ReadBuffer, (GLenum mode), )                                        \
  X(void, CopyPixels,                                                         \
    (GLint x, GLint y, GLsizei width, GLsizei height, GLenum type), )         \
  X(void, ReadPixels,                                                         \
    (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format,          \
     GLenum type, GLvoid * pixels), )                                         \
  X(void, DrawPixels,                                                         \
    (GLsizei width, GLsizei height, GLenum format, GLenum type,               \
     const GLvoid* pixels), )                                                 \
  X(void, GetBooleanv, (GLenum pname, GLboolean * params), )                  \
  X(void, GetClipPlane, (GLenum plane, GLdouble * equation), )                \
  X(void, GetDoublev, (GLenum pname, GLdouble * params), )                    \
  X(GLenum, GetError, (void), 0)                                              \
  X(void, GetFloatv, (GLenum pname, GLfloat * params), )                      \
  X(void, GetIntegerv, (GLenum pname, GLint * params), )                      \
  X(void, GetLightfv, (GLenum light, GLenum pname, GLfloat * params), )       \
  X(void, GetLightiv, (GLenum light, GLenum pname, GLint * params), )         \
  X(void, GetMapdv, (GLenum target, GLenum query, GLdouble * v), )            \
  X(void, GetMapfv, (GLenum target, GLenum query, GLfloat * v), )             \
  X(void, GetMapiv, (GLenum target, GLenum query, GLint * v), )               \
  X(void, GetMaterialfv, (GLenum face, GLenum pname, GLfloat * params), )     \
  X(void, GetMaterialiv, (GLenum face, GLenum pname, GLint * params), )       \
  X(void, GetPixelMapfv, (GLenum map, GLfloat * values), )                    \
  X(void, GetPixelMapuiv, (GLenum map, GLuint * values), )                    \
  X(void, GetPixelMapusv, (GLenum map, GLushort * values), )                  \
  X(void, GetPolygonStipple, (GLubyte * mask), )                              \
  X(const GLubyte*, GetString, (GLenum name), 0)                              \
  X(void, GetTexEnvfv, (GLenum target, GLenum pname, GLfloat * params), )     \
  X(void, GetTexEnviv, (GLenum target, GLenum pname, GLint * params), )       \
  X(void, GetTexGendv, (GLenum coord, GLenum pname, GLdouble * params), )     \
  X(void, GetTexGenfv, (GLenum coord, GLenum pname, GLfloat * params), )      \
  X(void, GetTexGeniv, (GLenum coord, GLenum pname, GLint * params), )        \
  X(void, GetTexImage,                                                        \
    (GLenum target, GLint level, GLenum format, GLenum type,                  \
     GLvoid * pixels), )                                                      \
  X(void, GetTexParameterfv,                                                  \
    (GLenum target, GLenum pname, GLfloat * params), )                        \
  X(void, GetTexParameteriv, (GLenum target, GLenum pname, GLint * params), ) \
  X(void, GetTexLevelParameterfv,                                             \
    (GLenum target, GLint level, GLenum pname, GLfloat * params), )           \
  X(void, GetTexLevelParameteriv,                                             \
    (GLenum target, GLint level, GLenum pname, GLint * params), )             \
  X(GLboolean, IsEnabled, (GLenum cap), 0)                                    \
  X(GLboolean, IsList, (GLuint list), 0)                                      \
  X(void, DepthRange, (GLclampd z_near, GLclampd z_far), )                    \
  X(void, Frustum,                                                            \
    (GLdouble left, GLdouble right, GLdouble bottom, GLdouble top,            \
     GLdouble z_near, GLdouble z_far), )                                      \
  X(void, LoadIdentity, (void), )                                             \
  X(void, LoadMatrixf, (const GLfloat* m), )                                  \
  X(void, LoadMatrixd, (const GLdouble* m), )                                 \
  X(void, MatrixMode, (GLenum mode), )                                        \
  X(void, MultMatrixf, (const GLfloat* m), )                                  \
  X(void, MultMatrixd, (const GLdouble* m), )                                 \
  X(void, Ortho,                                                              \
    (GLdouble left, GLdouble right, GLdouble bottom, GLdouble top,            \
     GLdouble z_near, GLdouble z_far), )                                      \
  X(void, PopMatrix, (void), )                                                \
  X(void, PushMatrix, (void), )                                               \
  X(void, Rotated, (GLdouble angle, GLdouble x, GLdouble y, GLdouble z), )    \
  X(void, Rotatef, (GLfloat angle, GLfloat x, GLfloat y, GLfloat z), )        \
  X(void, Scaled, (GLdouble x, GLdouble y, GLdouble z), )                     \
  X(void, Scalef, (GLfloat x, GLfloat y, GLfloat z), )                        \
  X(void, Translated, (GLdouble x, GLdouble y, GLdouble z), )                 \
  X(void, Translatef, (GLfloat x, GLfloat y, GLfloat z), )                    \
  X(void, Viewport, (GLint x, GLint y, GLsizei width, GLsizei height), )      \
  X(void, ArrayElement, (GLint i), )                                          \
  X(void, BindTexture, (GLenum target, GLuint texture), )                     \
  X(void, ColorPointer,                                                       \
    (GLint size, GLenum type, GLsizei stride, const GLvoid* pointer), )       \
  X(void, DisableClientState, (GLenum array), )                               \
  X(void, DrawArrays, (GLenum mode, GLint first, GLsizei count), )            \
  X(void, DrawElements,                                                       \
    (GLenum mode, GLsizei count, GLenum type, const GLvoid* indices), )       \
  X(void, EdgeFlagPointer, (GLsizei stride, const GLvoid* pointer), )         \
  X(void, EnableClientState, (GLenum array), )                                \
  X(void, IndexPointer,                                                       \
    (GLenum type, GLsizei stride, const GLvoid* pointer), )                   \
  X(void, Indexub, (GLubyte c), )                                             \
  X(void, Indexubv, (const GLubyte* c), )                                     \
  X(void, InterleavedArrays,                                                  \
    (GLenum format, GLsizei stride, const GLvoid* pointer), )                 \
  X(void, NormalPointer,                                                      \
    (GLenum type, GLsizei stride, const GLvoid* pointer), )                   \
  X(void, PolygonOffset, (GLfloat factor, GLfloat units), )                   \
  X(void, TexCoordPointer,                                                    \
    (GLint size, GLenum type, GLsizei stride, const GLvoid* pointer), )       \
  X(void, VertexPointer,                                                      \
    (GLint size, GLenum type, GLsizei stride, const GLvoid* pointer), )       \
  X(GLboolean, AreTexturesResident,                                           \
    (GLsizei n, const GLuint* textures, GLboolean* residences), 0)            \
  X(void, CopyTexImage1D,                                                     \
    (GLenum target, GLint level, GLenum internal_format, GLint x, GLint y,    \
     GLsizei width, GLint border), )                                          \
  X(void, CopyTexImage2D,                                                     \
    (GLenum target, GLint level, GLenum internal_format, GLint x, GLint y,    \
     GLsizei width, GLsizei height, GLint border), )                          \
  X(void, CopyTexSubImage1D,                                                  \
    (GLenum target, GLint level, GLint xoffset, GLint x, GLint y,             \
     GLsizei width), )                                                        \
  X(void, CopyTexSubImage2D,                                                  \
    (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x,       \
     GLint y, GLsizei width, GLsizei height), )                               \
  X(void, DeleteTextures, (GLsizei n, const GLuint* textures), )              \
  X(void, GenTextures, (GLsizei n, GLuint * textures), )                      \
  X(void, GetPointerv, (GLenum pname, GLvoid * *params), )                    \
  X(GLboolean, IsTexture, (GLuint texture), 0)                                \
  X(void, PrioritizeTextures,                                                 \
    (GLsizei n, const GLuint* textures, const GLclampf* priorities), )        \
  X(void, TexSubImage1D,                                                      \
    (GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, \
     GLenum type, const GLvoid* pixels), )                                    \
  X(void, TexSubImage2D,                                                      \
    (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, \
     GLsizei height, GLenum format, GLenum type, const GLvoid* pixels), )     \
  X(void, PopClientAttrib, (void), )                                          \
  X(void, PushClientAttrib, (GLbitfield mask), )

#include <GL/gl.h>

#define SOFTGPU_ENTRY(ret, name, params, value) kGl##name,
typedef enum GlEntry { SOFTGPU_GL11(SOFTGPU_ENTRY) kGlEntryCount } GlEntry;
#undef SOFTGPU_ENTRY

typedef struct ProcTable {
  int count;
  PROC entries[kGlEntryCount];
} ProcTable;

BOOL APIENTRY DrvValidateVersion(ULONG version);
void APIENTRY DrvSetCallbackProcs(INT count, PROC* procs);
LONG APIENTRY DrvDescribePixelFormat(HDC hdc, INT format, ULONG size,
                                     PIXELFORMATDESCRIPTOR* descriptor);
BOOL APIENTRY DrvSetPixelFormat(HDC hdc, LONG format);
ULONG APIENTRY DrvCreateContext(HDC hdc);
ULONG APIENTRY DrvCreateLayerContext(HDC hdc, INT layer);
BOOL APIENTRY DrvDeleteContext(ULONG handle);
BOOL APIENTRY DrvCopyContext(ULONG source, ULONG destination, UINT mask);
BOOL APIENTRY DrvShareLists(ULONG first, ULONG second);
ProcTable* APIENTRY DrvSetContext(HDC hdc, ULONG handle, PROC set_proc_table);
BOOL APIENTRY DrvReleaseContext(ULONG handle);
PROC APIENTRY DrvGetProcAddress(LPCSTR name);
BOOL APIENTRY DrvSwapBuffers(HDC hdc);
BOOL APIENTRY DrvSwapLayerBuffers(HDC hdc, UINT planes);
BOOL APIENTRY DrvDescribeLayerPlane(HDC hdc, INT format, INT layer, UINT size,
                                    LAYERPLANEDESCRIPTOR* plane);
INT APIENTRY DrvSetLayerPaletteEntries(HDC hdc, INT layer, INT start, INT count,
                                       const COLORREF* colors);
INT APIENTRY DrvGetLayerPaletteEntries(HDC hdc, INT layer, INT start, INT count,
                                       COLORREF* colors);
BOOL APIENTRY DrvRealizeLayerPalette(HDC hdc, INT layer, BOOL realize);

#endif
