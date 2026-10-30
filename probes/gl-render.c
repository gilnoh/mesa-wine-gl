/* Native rendering/readback controls. These run in the real, unforced context.
 */
#include "glapi/glapi/glapi.h"
#include "util/glheader.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FUNCTIONS(F)                                                           \
  F(void, glGenTextures, (GLsizei, GLuint *))                                  \
  F(void, glBindTexture, (GLenum, GLuint))                                     \
  F(void, glTexParameteri, (GLenum, GLenum, GLint))                            \
  F(void, glTexImage2D,                                                        \
    (GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum,            \
     const void *))                                                            \
  F(void, glTexSubImage2D,                                                     \
    (GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum,            \
     const void *))                                                            \
  F(void, glDeleteTextures, (GLsizei, const GLuint *))                         \
  F(void, glGenFramebuffers, (GLsizei, GLuint *))                              \
  F(void, glBindFramebuffer, (GLenum, GLuint))                                 \
  F(void, glFramebufferTexture2D, (GLenum, GLenum, GLenum, GLuint, GLint))     \
  F(GLenum, glCheckFramebufferStatus, (GLenum))                                \
  F(void, glDeleteFramebuffers, (GLsizei, const GLuint *))                     \
  F(void, glViewport, (GLint, GLint, GLsizei, GLsizei))                        \
  F(void, glClearColor, (GLfloat, GLfloat, GLfloat, GLfloat))                  \
  F(void, glClear, (GLbitfield))                                               \
  F(void, glReadPixels,                                                        \
    (GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *))                  \
  F(GLuint, glCreateShader, (GLenum))                                          \
  F(void, glShaderSource,                                                      \
    (GLuint, GLsizei, const GLchar *const *, const GLint *))                   \
  F(void, glCompileShader, (GLuint))                                           \
  F(void, glGetShaderiv, (GLuint, GLenum, GLint *))                            \
  F(void, glGetShaderInfoLog, (GLuint, GLsizei, GLsizei *, GLchar *))          \
  F(GLuint, glCreateProgram, (void))                                           \
  F(void, glAttachShader, (GLuint, GLuint))                                    \
  F(void, glBindAttribLocation, (GLuint, GLuint, const GLchar *))              \
  F(void, glLinkProgram, (GLuint))                                             \
  F(void, glGetProgramiv, (GLuint, GLenum, GLint *))                           \
  F(void, glGetProgramInfoLog, (GLuint, GLsizei, GLsizei *, GLchar *))         \
  F(void, glUseProgram, (GLuint))                                              \
  F(GLint, glGetUniformLocation, (GLuint, const GLchar *))                     \
  F(void, glUniform1i, (GLint, GLint))                                         \
  F(void, glUniform2f, (GLint, GLfloat, GLfloat))                              \
  F(void, glDeleteShader, (GLuint))                                            \
  F(void, glDeleteProgram, (GLuint))                                           \
  F(void, glGenBuffers, (GLsizei, GLuint *))                                   \
  F(void, glBindBuffer, (GLenum, GLuint))                                      \
  F(void, glBufferData, (GLenum, GLsizeiptr, const void *, GLenum))            \
  F(void, glDeleteBuffers, (GLsizei, const GLuint *))                          \
  F(void, glVertexAttribPointer,                                               \
    (GLuint, GLint, GLenum, GLboolean, GLsizei, const void *))                 \
  F(void, glEnableVertexAttribArray, (GLuint))                                 \
  F(void, glDrawArrays, (GLenum, GLint, GLsizei))                              \
  F(void, glDisable, (GLenum))                                                 \
  F(GLsync, glFenceSync, (GLenum, GLbitfield))                                 \
  F(GLenum, glClientWaitSync, (GLsync, GLbitfield, GLuint64))                  \
  F(void, glDeleteSync, (GLsync))                                              \
  F(GLenum, glGetError, (void))                                                \
  F(const GLubyte *, glGetString, (GLenum))
#define DECL(ret, name, args) static ret(*p##name) args;
FUNCTIONS(DECL)
#undef DECL

static GLuint compile(GLenum type, const char *source) {
  GLuint shader = pglCreateShader(type);
  pglShaderSource(shader, 1, &source, NULL);
  pglCompileShader(shader);
  GLint ok;
  pglGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[4096];
    pglGetShaderInfoLog(shader, sizeof(log), NULL, log);
    fprintf(stderr, "Shader compile failure: %s\n", log);
    pglDeleteShader(shader);
    return 0;
  }
  return shader;
}

static int verify(const char *name, const unsigned char *expected, int wrong) {
  unsigned char got[8 * 8 * 4];
  memset(got, 0xCD, sizeof(got));
  pglReadPixels(0, 0, 8, 8, GL_RGBA, GL_UNSIGNED_BYTE, got);
  GLenum error = pglGetError();
  unsigned mismatch = 0;
  for (unsigned i = 0; i < sizeof(got); i++) {
    int wanted = expected[i];
    if (wrong && i == 0)
      wanted = (wanted + 127) % 256;
    if (abs((int)got[i] - wanted) > 1) {
      if (!mismatch)
        fprintf(stderr, "%s: first mismatch byte=%u actual=%u expected=%d\n",
                name, i, got[i], wanted);
      mismatch++;
    }
  }
  printf("%s: %s mismatch_bytes=%u GL_error=0x%x\n", name,
         mismatch || error ? "FAIL" : "PASS", mismatch, error);
  return mismatch || error;
}

static void pixel(unsigned char *out, int x, int y, int level) {
  out[0] = (x * 13 + y * 5 + level * 31) % 256;
  out[1] = (y * 37 + x * 3 + level * 17) % 256;
  out[2] = (x * 7 + y * 11 + level * 47) % 256;
  out[3] = 255;
}

int run_render_tests(int wrong);
int run_render_tests(int wrong) {
#define LOAD(ret, name, args)                                                  \
  p##name = (void *)_mesa_glapi_get_proc_address(#name);                       \
  if (!p##name) {                                                              \
    fprintf(stderr, "Missing entrypoint %s\n", #name);                         \
    return 1;                                                                  \
  }
  FUNCTIONS(LOAD)
#undef LOAD
  const char *extensions = (const char *)pglGetString(GL_EXTENSIONS);
  if (!extensions || !strstr(extensions, "GL_ARB_framebuffer_object") ||
      !strstr(extensions, "GL_ARB_sync") ||
      !strstr(extensions, "GL_ARB_texture_float")) {
    fprintf(stderr, "Required probe extensions unavailable\n");
    return 1;
  }
  GLuint target, fbo, input, buffer;
  pglGenTextures(1, &target);
  pglBindTexture(GL_TEXTURE_2D, target);
  pglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                NULL);
  pglGenFramebuffers(1, &fbo);
  pglBindFramebuffer(GL_FRAMEBUFFER, fbo);
  pglFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                          target, 0);
  GLenum status = pglCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    fprintf(stderr, "Incomplete FBO: 0x%x\n", status);
    return 1;
  }
  pglViewport(0, 0, 8, 8);
  pglDisable(GL_DITHER);
  pglClearColor(1, 0, 1, 1);
  pglClear(GL_COLOR_BUFFER_BIT);
  unsigned char expected[8 * 8 * 4];
  for (int i = 0; i < 64; i++) {
    expected[i * 4] = 255;
    expected[i * 4 + 1] = 0;
    expected[i * 4 + 2] = 255;
    expected[i * 4 + 3] = 255;
  }
  int failed = verify("FBO clear/readback", expected, wrong);
  const char *vs =
      "#version 120\nattribute vec2 position; varying vec2 uv; void "
      "main(){gl_Position=vec4(position,0,1);uv=position*0.5+0.5;}";
  const char *fs =
      "#version 120\nuniform sampler2D image; uniform vec2 sourceSize; uniform "
      "int texelCenters; varying vec2 uv; void main(){vec2 coord=uv; "
      "if(texelCenters!=0)coord=(floor(gl_FragCoord.xy*sourceSize/"
      "8.0)+vec2(0.5))/sourceSize; gl_FragColor=texture2D(image,coord);}";
  GLuint vert = compile(GL_VERTEX_SHADER, vs),
         frag = compile(GL_FRAGMENT_SHADER, fs);
  if (!vert || !frag)
    return 1;
  GLuint program = pglCreateProgram();
  pglAttachShader(program, vert);
  pglAttachShader(program, frag);
  pglBindAttribLocation(program, 0, "position");
  pglLinkProgram(program);
  GLint ok;
  pglGetProgramiv(program, GL_LINK_STATUS, &ok);
  if (!ok) {
    char log[4096];
    pglGetProgramInfoLog(program, sizeof(log), NULL, log);
    fprintf(stderr, "Link failure: %s\n", log);
    return 1;
  }
  pglUseProgram(program);
  pglUniform1i(pglGetUniformLocation(program, "image"), 0);
  const float vertices[] = {-1, -1, 3, -1, -1, 3};
  pglGenBuffers(1, &buffer);
  pglBindBuffer(GL_ARRAY_BUFFER, buffer);
  pglBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
  pglVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
  pglEnableVertexAttribArray(0);
  pglGenTextures(1, &input);
  pglBindTexture(GL_TEXTURE_2D, input);
  pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  for (int round = 0; round < 3; round++) {
    unsigned char pixels[16];
    for (int y = 0; y < 2; y++)
      for (int x = 0; x < 2; x++)
        pixel(pixels + (y * 2 + x) * 4, x, y, round);
    if (round == 0)
      pglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA,
                    GL_UNSIGNED_BYTE, pixels);
    else
      pglTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE,
                       pixels);
    pglClear(GL_COLOR_BUFFER_BIT);
    pglDrawArrays(GL_TRIANGLES, 0, 3);
    GLsync fence = pglFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    GLenum wait =
        pglClientWaitSync(fence, GL_SYNC_FLUSH_COMMANDS_BIT, 5000000000ULL);
    pglDeleteSync(fence);
    if (wait != GL_ALREADY_SIGNALED && wait != GL_CONDITION_SATISFIED) {
      fprintf(stderr, "Fence wait failed: 0x%x\n", wait);
      failed++;
    }
    for (int y = 0; y < 8; y++)
      for (int x = 0; x < 8; x++)
        pixel(expected + (y * 8 + x) * 4, x / 4, y / 4, round);
    char name[80];
    snprintf(name, sizeof(name), "Texture draw/subimage/fence round %d", round);
    failed += verify(name, expected, wrong);
  }
  /* Sample texel centres: interpolated boundary coordinates can round either
   * way. */
  pglUniform1i(pglGetUniformLocation(program, "texelCenters"), 1);
  for (int half = 0; half < 2; half++) {
    for (int level = 0; level < 11; level++) {
      int w = 1024 >> level, h = 32 >> level;
      if (h < 1)
        h = 1;
      unsigned char *pixels = malloc((size_t)w * h * 4);
      for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
          pixel(pixels + (y * w + x) * 4, x, y, level);
      pglTexImage2D(GL_TEXTURE_2D, level, half ? GL_RGBA16F : GL_RGBA8, w, h, 0,
                    GL_RGBA, GL_UNSIGNED_BYTE, pixels);
      free(pixels);
    }
    for (int level = 0; level < 11; level++) {
      int w = 1024 >> level, h = 32 >> level;
      if (h < 1)
        h = 1;
      pglUniform2f(pglGetUniformLocation(program, "sourceSize"), (float)w,
                   (float)h);
      pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, level);
      pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, level);
      pglClear(GL_COLOR_BUFFER_BIT);
      pglDrawArrays(GL_TRIANGLES, 0, 3);
      for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++)
          pixel(expected + (y * 8 + x) * 4, ((2 * x + 1) * w) / 16,
                ((2 * y + 1) * h) / 16, level);
      char name[80];
      snprintf(name, sizeof(name), "1024x32 mip chain %s level %d",
               half ? "RGBA16F" : "RGBA8", level);
      failed += verify(name, expected, wrong);
    }
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1000);
  }
  pglDeleteBuffers(1, &buffer);
  pglDeleteTextures(1, &input);
  pglDeleteTextures(1, &target);
  pglDeleteFramebuffers(1, &fbo);
  pglDeleteProgram(program);
  pglDeleteShader(vert);
  pglDeleteShader(frag);
  printf("Rendering checks: %d/26 passed%s\n", 26 - failed,
         wrong ? " (deliberately wrong expectations)" : "");
  return failed != 0;
}
