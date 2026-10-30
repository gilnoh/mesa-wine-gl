/* GL/Zink texel-buffer test with integer framebuffer readback. */
#include "glapi/glapi/glapi.h"
#include "util/glheader.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define FN(type, name) type name = (type)_mesa_glapi_get_proc_address(#name)
#define RAW(ret, name, args)                                                   \
  ret(*name) args = (void *)_mesa_glapi_get_proc_address(#name)
static uint32_t texel_value(unsigned kind, unsigned word, unsigned round) {
  if (kind == 0) {
    float f = (float)(word * 7 + round * 1000) * 0.25f - 12.5f;
    uint32_t u;
    memcpy(&u, &f, 4);
    return u;
  }
  if (kind == 1)
    return (uint32_t)(-1000000001 - (int32_t)word * 19 - (int32_t)round * 1000);
  return 3000000001u + word * 19 + round * 1000;
}
int run_gl_texel_tests(int rgb, int wrong);
int run_gl_texel_tests(int rgb, int wrong) {
  FN(PFNGLCREATESHADERPROC, glCreateShader);
  FN(PFNGLSHADERSOURCEPROC, glShaderSource);
  FN(PFNGLCOMPILESHADERPROC, glCompileShader);
  FN(PFNGLGETSHADERIVPROC, glGetShaderiv);
  FN(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog);
  FN(PFNGLCREATEPROGRAMPROC, glCreateProgram);
  FN(PFNGLATTACHSHADERPROC, glAttachShader);
  FN(PFNGLLINKPROGRAMPROC, glLinkProgram);
  FN(PFNGLGETPROGRAMIVPROC, glGetProgramiv);
  FN(PFNGLGETPROGRAMINFOLOGPROC, glGetProgramInfoLog);
  FN(PFNGLUSEPROGRAMPROC, glUseProgram);
  FN(PFNGLGETUNIFORMLOCATIONPROC, glGetUniformLocation);
  FN(PFNGLUNIFORM1IPROC, glUniform1i);
  FN(PFNGLDELETESHADERPROC, glDeleteShader);
  FN(PFNGLDELETEPROGRAMPROC, glDeleteProgram);
  FN(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays);
  FN(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray);
  FN(PFNGLDELETEVERTEXARRAYSPROC, glDeleteVertexArrays);
  FN(PFNGLGENBUFFERSPROC, glGenBuffers);
  FN(PFNGLBINDBUFFERPROC, glBindBuffer);
  FN(PFNGLBUFFERDATAPROC, glBufferData);
  FN(PFNGLBUFFERSUBDATAPROC, glBufferSubData);
  FN(PFNGLDELETEBUFFERSPROC, glDeleteBuffers);
  FN(PFNGLTEXBUFFERRANGEPROC, glTexBufferRange);
  FN(PFNGLGENFRAMEBUFFERSPROC, glGenFramebuffers);
  FN(PFNGLBINDFRAMEBUFFERPROC, glBindFramebuffer);
  FN(PFNGLFRAMEBUFFERTEXTURE2DPROC, glFramebufferTexture2D);
  FN(PFNGLCHECKFRAMEBUFFERSTATUSPROC, glCheckFramebufferStatus);
  FN(PFNGLDELETEFRAMEBUFFERSPROC, glDeleteFramebuffers);
  RAW(void, glGenTextures, (GLsizei, GLuint *));
  RAW(void, glBindTexture, (GLenum, GLuint));
  RAW(void, glTexImage2D,
      (GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum,
       const void *));
  RAW(void, glDeleteTextures, (GLsizei, const GLuint *));
  RAW(void, glViewport, (GLint, GLint, GLsizei, GLsizei));
  RAW(void, glDrawArrays, (GLenum, GLint, GLsizei));
  RAW(void, glReadPixels,
      (GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *));
  RAW(GLenum, glGetError, (void));
  RAW(void, glGetIntegerv, (GLenum, GLint *));
  const char *vs_source = "#version 330\nvoid main(){vec2 "
                          "p[3]=vec2[3](vec2(-1,-1),vec2(3,-1),vec2(-1,3));gl_"
                          "Position=vec4(p[gl_VertexID],0,1);}";
  GLuint vs = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vs, 1, &vs_source, NULL);
  glCompileShader(vs);
  GLint ok = 0;
  char log[2048];
  glGetShaderiv(vs, GL_COMPILE_STATUS, &ok);
  if (!ok)
    return 1;
  GLuint vao, buf, tex, target, fbo;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);
  glGenTextures(1, &target);
  glBindTexture(GL_TEXTURE_2D, target);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32UI, 5, 1, 0, GL_RGBA_INTEGER,
               GL_UNSIGNED_INT, NULL);
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         target, 0);
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    return 1;
  glViewport(0, 0, 5, 1);
  glGenBuffers(1, &buf);
  glBindBuffer(GL_TEXTURE_BUFFER, buf);
  glBufferData(GL_TEXTURE_BUFFER, 16384, NULL, GL_DYNAMIC_DRAW);
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_BUFFER, tex);
  GLint alignment = 0;
  glGetIntegerv(GL_TEXTURE_BUFFER_OFFSET_ALIGNMENT, &alignment);
  if (alignment <= 0 || alignment * 3 + 112 > 16384)
    return 1;
  const GLenum formats[3][4] = {{GL_R32F, GL_RG32F, GL_RGB32F, GL_RGBA32F},
                                {GL_R32I, GL_RG32I, GL_RGB32I, GL_RGBA32I},
                                {GL_R32UI, GL_RG32UI, GL_RGB32UI, GL_RGBA32UI}};
  const char *samplers[] = {"samplerBuffer", "isamplerBuffer",
                            "usamplerBuffer"};
  int failed = 0, total = 0;
  for (unsigned kind = 0; kind < 3; kind++) {
    char fs_source[2048];
    snprintf(fs_source, sizeof(fs_source),
             "#version 330\nuniform %s values;layout(location=0) out uvec4 "
             "result;void main(){int n=textureSize(values);int "
             "i=int(gl_FragCoord.x);int "
             "coords[4]=int[4](n-1,0,n/"
             "2,1);if(i==4)result=uvec4(n,123,456,789);else "
             "result=%s(texelFetch(values,coords[i]));}",
             samplers[kind], kind == 0 ? "floatBitsToUint" : "uvec4");
    const char *source = fs_source;
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &source, NULL);
    glCompileShader(fs);
    glGetShaderiv(fs, GL_COMPILE_STATUS, &ok);
    if (!ok) {
      glGetShaderInfoLog(fs, sizeof(log), NULL, log);
      puts(log);
      return 1;
    }
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
      glGetProgramInfoLog(program, sizeof(log), NULL, log);
      puts(log);
      return 1;
    }
    glUseProgram(program);
    glUniform1i(glGetUniformLocation(program, "values"), 0);
    for (unsigned c = 1; c <= 4; c++) {
      if (c == 3 && !rgb)
        continue;
      for (unsigned range = 0; range < 2; range++) {
        unsigned offset = range ? alignment * 3 : 0, n = range ? 7 : 4;
        glTexBufferRange(GL_TEXTURE_BUFFER, formats[kind][c - 1], buf, offset,
                         n * c * 4);
        for (unsigned round = 0; round < 2; round++) {
          uint32_t input[4096];
          for (unsigned i = 0; i < 4096; i++)
            input[i] = texel_value(kind, i, round);
          glBufferSubData(GL_TEXTURE_BUFFER, 0, sizeof(input), input);
          glDrawArrays(GL_TRIANGLES, 0, 3);
          unsigned char actual[512], expected[512];
          memset(actual, 0xCD, 512);
          memset(expected, 0xCD, 512);
          glReadPixels(0, 0, 5, 1, GL_RGBA_INTEGER, GL_UNSIGNED_INT,
                       actual + 32);
          unsigned coords[] = {n - 1, 0, n / 2, 1};
          for (unsigned i = 0; i < 4; i++) {
            uint32_t v[4] = {0, 0, 0, kind == 0 ? 0x3f800000u : 1u};
            for (unsigned j = 0; j < c; j++)
              v[j] = input[offset / 4 + coords[i] * c + j];
            memcpy(expected + 32 + i * 16, v, 16);
          }
          uint32_t size[4] = {n, 123, 456, 789};
          memcpy(expected + 96, size, 16);
          if (wrong)
            expected[32] ^= 1;
          GLenum err = glGetError();
          unsigned mismatches = 0;
          for (unsigned i = 0; i < 512; i++)
            mismatches += actual[i] != expected[i];
          int bad = mismatches || err;
          failed += bad;
          total++;
          printf("GL texel components=%u kind=%u offset=%u elements=%u "
                 "update=%u: %s mismatch_bytes=%u error=0x%x\n",
                 c, kind, offset, n, round, bad ? "FAIL" : "PASS", mismatches,
                 err);
        }
      }
    }
    glUseProgram(0);
    glDeleteProgram(program);
    glDeleteShader(fs);
  }
  glDeleteShader(vs);
  glDeleteBuffers(1, &buf);
  glDeleteTextures(1, &tex);
  glDeleteTextures(1, &target);
  glDeleteFramebuffers(1, &fbo);
  glDeleteVertexArrays(1, &vao);
  printf("GL texel checks: %d/%d passed\n", total - failed, total);
  return failed != 0;
}
