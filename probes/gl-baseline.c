/* Minimal surfaceless Mesa/Zink probe. No version or extension overrides. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pipe/p_screen.h"
#include "frontend/api.h"
#include "state_tracker/st_context.h"
#include "state_tracker/st_extensions.h"
#include "main/context.h"
#include "main/extensions.h"
#include "glapi/glapi/glapi.h"
#include "gallium/drivers/zink/zink_public.h"

int run_render_tests(int wrong);
int run_gl_xfb_tests(int wrong);
int run_gl_texel_tests(int rgb, int wrong);

int main(int argc, char **argv)
{
   setbuf(stdout, NULL);
   const char *forbidden[] = {"MESA_GL_VERSION_OVERRIDE", "MESA_GLSL_VERSION_OVERRIDE",
                              "MESA_EXTENSION_OVERRIDE", "DYLD_INSERT_LIBRARIES"};
   for (unsigned i = 0; i < sizeof(forbidden)/sizeof(forbidden[0]); i++) {
      if (getenv(forbidden[i])) {
         fprintf(stderr, "Refusing overridden baseline: %s\n", forbidden[i]);
         return 2;
      }
   }
   int requested = argc > 1 ? atoi(argv[1]) : 40;
   if (requested < 10 || requested > 46) return 2;
   struct pipe_screen *screen = zink_create_screen(NULL, NULL);
   if (!screen) { fprintf(stderr, "Zink screen creation failed\n"); return 1; }
   printf("Screen: %s\n", screen->get_name(screen));
   struct pipe_frontend_screen frontend = {.screen = screen};
   struct st_config_options options = {0};
   int core, compat, es1, es2;
   st_api_query_versions(&frontend, &options, &core, &compat, &es1, &es2);
   printf("Reported versions: core=%d compat=%d ES1=%d ES2=%d\n", core, compat, es1, es2);
   /* Inspect actual core capability gates independently of context creation. */
   struct gl_constants constants = {0};
   struct gl_extensions extensions = {0};
   _mesa_init_constants(&constants, API_OPENGL_CORE);
   _mesa_init_extensions(&extensions);
   st_init_limits(screen, &constants, &extensions, &options, API_OPENGL_CORE);
   st_init_extensions(screen, &constants, &extensions, &options, API_OPENGL_CORE);
   printf("Core limits: GLSL=%u color_attachments=%u samples=%u fake_msaa=%u\n",
          constants.GLSLVersion, constants.MaxColorAttachments,
          constants.MaxSamples, constants.FakeSWMSAA);
   printf("Core limits: texture_size=%u renderbuffer_size=%u cube_levels=%u 3D_levels=%u layers=%u\n",
          constants.MaxTextureSize, constants.MaxRenderbufferSize,
          constants.MaxCubeTextureLevels, constants.Max3DTextureLevels,
          constants.MaxArrayTextureLayers);
   printf("Core limits: VS_texture_units=%u VS_uniform_blocks=%u vertex_stride=%u\n",
          constants.Program[MESA_SHADER_VERTEX].MaxTextureImageUnits,
          constants.Program[MESA_SHADER_VERTEX].MaxUniformBlocks, constants.MaxVertexAttribStride);
#define CHECK_EXTENSION(field) if (!extensions.field) printf("Missing desktop version-gate extension: %s\n", #field)
#include "gl-version-requirements.h"
#undef CHECK_EXTENSION
   free(constants.SpirVExtensions);
   struct st_context_attribs attributes = {0};
   attributes.profile = requested < 30 ? API_OPENGL_COMPAT : API_OPENGL_CORE;
   attributes.major = requested / 10;
   attributes.minor = requested % 10;
   attributes.visual.color_format = PIPE_FORMAT_R8G8B8A8_UNORM;
   attributes.visual.buffer_mask = ST_ATTACHMENT_FRONT_LEFT_MASK;
   enum st_context_error error;
   printf("Requesting %d.%d %s context\n", attributes.major, attributes.minor,
          attributes.profile == API_OPENGL_CORE ? "core" : "compat");
   struct st_context *context = st_api_create_context(&frontend, &attributes, &error, NULL);
   if (!context) { fprintf(stderr, "Context creation failed: error=%d\n", error); return 1; }
   if (!st_api_make_current(context, NULL, NULL)) {
      fprintf(stderr, "Make-current failed\n"); return 1;
   }
   const GLubyte *(*get_string)(GLenum) = (void *)_mesa_glapi_get_proc_address("glGetString");
   GLenum (*get_error)(void) = (void *)_mesa_glapi_get_proc_address("glGetError");
   if (!get_string || !get_error) return 1;
   printf("GL_VERSION: %s\n", get_string(GL_VERSION));
   printf("GL_RENDERER: %s\n", get_string(GL_RENDERER));
   printf("GLSL_VERSION: %s\n", get_string(GL_SHADING_LANGUAGE_VERSION));
   GLenum gl_error = get_error();
   printf("GL error: 0x%x\n", gl_error);
   int render_failed = 0;
   if (argc > 2 && !strcmp(argv[2], "--render"))
      render_failed = run_render_tests(argc > 3 && !strcmp(argv[3], "--wrong-expected"));
   if (argc > 2 && !strcmp(argv[2], "--xfb"))
      render_failed = run_gl_xfb_tests(argc > 3 && !strcmp(argv[3], "--wrong-expected"));
   if (argc > 2 && !strcmp(argv[2], "--texel")) {
      int rgb = 1, wrong = 0;
      for (int i = 3; i < argc; i++) {
         if (!strcmp(argv[i], "--native-only")) rgb = 0;
         if (!strcmp(argv[i], "--wrong-expected")) wrong = 1;
      }
      render_failed = run_gl_texel_tests(rgb, wrong);
   }
   st_api_make_current(NULL, NULL, NULL);
   st_destroy_context(context);
   st_screen_destroy(&frontend);
   screen->destroy(screen);
   return gl_error != GL_NO_ERROR || render_failed;
}
