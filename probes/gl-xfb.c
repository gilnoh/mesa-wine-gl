/* Small GL -> Zink capture check, independent of the Vulkan probe. */
#include "glapi/glapi/glapi.h"
#include "util/glheader.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define FN(type, name) type name = (type)_mesa_glapi_get_proc_address(#name)
int run_gl_xfb_tests(int wrong);
int run_gl_xfb_tests(int wrong) {
  FN(PFNGLGENFRAMEBUFFERSPROC, glGenFramebuffers);
  FN(PFNGLBINDFRAMEBUFFERPROC, glBindFramebuffer);
  FN(PFNGLGENRENDERBUFFERSPROC, glGenRenderbuffers);
  FN(PFNGLBINDRENDERBUFFERPROC, glBindRenderbuffer);
  FN(PFNGLRENDERBUFFERSTORAGEPROC, glRenderbufferStorage);
  FN(PFNGLFRAMEBUFFERRENDERBUFFERPROC, glFramebufferRenderbuffer);
  FN(PFNGLCHECKFRAMEBUFFERSTATUSPROC, glCheckFramebufferStatus);
  FN(PFNGLDELETEFRAMEBUFFERSPROC, glDeleteFramebuffers);
  FN(PFNGLDELETERENDERBUFFERSPROC, glDeleteRenderbuffers);
  FN(PFNGLCREATESHADERPROC, glCreateShader);
  FN(PFNGLSHADERSOURCEPROC, glShaderSource);
  FN(PFNGLCOMPILESHADERPROC, glCompileShader);
  FN(PFNGLGETSHADERIVPROC, glGetShaderiv);
  FN(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog);
  FN(PFNGLCREATEPROGRAMPROC, glCreateProgram);
  FN(PFNGLATTACHSHADERPROC, glAttachShader);
  FN(PFNGLTRANSFORMFEEDBACKVARYINGSPROC, glTransformFeedbackVaryings);
  FN(PFNGLLINKPROGRAMPROC, glLinkProgram);
  FN(PFNGLGETPROGRAMIVPROC, glGetProgramiv);
  FN(PFNGLGETPROGRAMINFOLOGPROC, glGetProgramInfoLog);
  FN(PFNGLUSEPROGRAMPROC, glUseProgram);
  FN(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays);
  FN(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray);
  FN(PFNGLGENBUFFERSPROC, glGenBuffers);
  FN(PFNGLBINDBUFFERPROC, glBindBuffer);
  FN(PFNGLBUFFERDATAPROC, glBufferData);
  FN(PFNGLBINDBUFFERRANGEPROC, glBindBufferRange);
  FN(PFNGLBEGINTRANSFORMFEEDBACKPROC, glBeginTransformFeedback);
  FN(PFNGLENDTRANSFORMFEEDBACKPROC, glEndTransformFeedback);
  FN(PFNGLGETBUFFERSUBDATAPROC, glGetBufferSubData);
  FN(PFNGLGENQUERIESPROC, glGenQueries);
  FN(PFNGLBEGINQUERYPROC, glBeginQuery);
  FN(PFNGLENDQUERYPROC, glEndQuery);
  FN(PFNGLGETQUERYOBJECTUIVPROC, glGetQueryObjectuiv);
  FN(PFNGLDELETEQUERIESPROC, glDeleteQueries);
  FN(PFNGLDELETEBUFFERSPROC, glDeleteBuffers);
  FN(PFNGLDELETEVERTEXARRAYSPROC, glDeleteVertexArrays);
  FN(PFNGLDELETEPROGRAMPROC, glDeleteProgram);
  FN(PFNGLDELETESHADERPROC, glDeleteShader);
  void (*enable)(GLenum) = (void *)_mesa_glapi_get_proc_address("glEnable");
  void (*disable)(GLenum) = (void *)_mesa_glapi_get_proc_address("glDisable");
  void (*draw)(GLenum, GLint, GLsizei) =
      (void *)_mesa_glapi_get_proc_address("glDrawArrays");
  GLenum (*error)(void) = (void *)_mesa_glapi_get_proc_address("glGetError");
  const char *source = "#version 330\nout vec4 captured;\nvoid "
                       "main(){captured=vec4(float(gl_VertexID)+10,20,30,1);gl_"
                       "Position=vec4(0,0,0,1);gl_PointSize=1;}";
  GLuint shader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(shader, 1, &source, NULL);
  glCompileShader(shader);
  GLint ok = 0;
  char log[2048];
  glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    glGetShaderInfoLog(shader, sizeof(log), NULL, log);
    puts(log);
    return 1;
  }
  GLuint program = glCreateProgram();
  glAttachShader(program, shader);
  const char *varying = "captured";
  glTransformFeedbackVaryings(program, 1, &varying, GL_INTERLEAVED_ATTRIBS);
  glLinkProgram(program);
  glGetProgramiv(program, GL_LINK_STATUS, &ok);
  if (!ok) {
    glGetProgramInfoLog(program, sizeof(log), NULL, log);
    puts(log);
    return 1;
  }
  /* Surfaceless contexts have no complete default framebuffer, even when
   * rasterizer discard is enabled. Supply a valid draw framebuffer. */
  GLuint fbo, rb;
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glGenRenderbuffers(1, &rb);
  glBindRenderbuffer(GL_RENDERBUFFER, rb);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 8, 8);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                            GL_RENDERBUFFER, rb);
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    return 1;
  GLuint vao, buffer, query;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);
  glGenBuffers(1, &buffer);
  glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, buffer);
  glGenQueries(1, &query);
  glUseProgram(program);
  enable(GL_RASTERIZER_DISCARD);
  int failed = 0;
  for (unsigned test = 0; test < 2; test++) {
    unsigned char initial[512], actual[512], expected[512];
    memset(initial, 0xCD, sizeof(initial));
    memcpy(expected, initial, sizeof(initial));
    glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, sizeof(initial), initial,
                 GL_DYNAMIC_READ);
    glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, buffer, 32,
                      test ? 32 : 256);
    glBeginQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, query);
    glBeginTransformFeedback(GL_POINTS);
    draw(GL_POINTS, 2, 4);
    glEndTransformFeedback();
    glEndQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN);
    GLuint written = 99;
    glGetQueryObjectuiv(query, GL_QUERY_RESULT, &written);
    glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0, sizeof(actual), actual);
    unsigned count = test ? 2 : 4;
    for (unsigned i = 0; i < count; i++) {
      float v[4] = {12.0f + i, 20, 30, 1};
      memcpy(expected + 32 + 16 * i, v, 16);
    }
    if (wrong)
      expected[32] ^= 1;
    GLenum err = error();
    int bad =
        memcmp(actual, expected, sizeof(actual)) || written != count || err;
    printf("GL XFB %s: %s written=%u expected=%u error=0x%x\n",
           test ? "overflow" : "capture", bad ? "FAIL" : "PASS", written, count,
           err);
    failed += bad;
  }
  disable(GL_RASTERIZER_DISCARD);
  glUseProgram(0);
  glDeleteQueries(1, &query);
  glDeleteBuffers(1, &buffer);
  glDeleteVertexArrays(1, &vao);
  glDeleteProgram(program);
  glDeleteShader(shader);
  glDeleteFramebuffers(1, &fbo);
  glDeleteRenderbuffers(1, &rb);
  printf("GL XFB checks: %d/2 passed\n", 2 - failed);
  return failed != 0;
}
