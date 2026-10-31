/*
 * Mesa Wine Bridge API
 *
 * Simple C API between Wine's winemac.drv and Mesa's Gallium state tracker.
 * This hides all Mesa internal types behind opaque pointers.
 *
 * Copyright 2026 mesa_gl_to_metal project
 * SPDX-License-Identifier: MIT
 */

#ifndef MESA_WINE_API_H
#define MESA_WINE_API_H

#include <stdbool.h>
#include <stdint.h>

#define MESA_WINE_ABI_VERSION 1
unsigned mesa_wine_get_abi_version(void);

/* Opaque handles */
typedef struct mesa_wine_screen *mesa_wine_screen_t;
typedef struct mesa_wine_ctx *mesa_wine_ctx_t;
typedef struct mesa_wine_draw *mesa_wine_draw_t;

/* Profile constants (matching WGL) */
#define MESA_WINE_PROFILE_COMPAT  0
#define MESA_WINE_PROFILE_CORE    1

/* Context flags */
#define MESA_WINE_FLAG_FORWARD_COMPATIBLE  (1 << 0)
#define MESA_WINE_FLAG_DEBUG               (1 << 1)

/*
 * Pixel format descriptor — returned by mesa_wine_get_pixel_format_info().
 * Provides all information needed to fill Wine's wgl_pixel_format struct.
 */
struct mesa_wine_pixel_format_info {
    int color_bits;       /* total color bits (e.g. 32) */
    int red_bits;         /* per-channel bits */
    int green_bits;
    int blue_bits;
    int alpha_bits;
    int red_shift;        /* bit shifts */
    int green_shift;
    int blue_shift;
    int alpha_shift;
    int depth_bits;       /* depth buffer bits (0, 16, 24, 32) */
    int stencil_bits;     /* stencil buffer bits (0, 8) */
    int samples;          /* MSAA sample count (0 = no MSAA) */
    bool doublebuffer;    /* double-buffered */
    bool srgb;            /* sRGB-capable framebuffer */
    bool pbuffer;         /* usable as pbuffer */
    bool float_components; /* float color components (RGBA16F) */
};

/*
 * Initialize Mesa: create Zink pipe_screen, set up frontend screen.
 * Must be called once before any other function.
 * Returns screen handle, or NULL on failure.
 */
mesa_wine_screen_t mesa_wine_init(void);

/*
 * Shut down Mesa and free the screen.
 */
void mesa_wine_fini(mesa_wine_screen_t screen);

/*
 * Query the number of supported pixel formats.
 * Sets *onscreen_count to the number of onscreen-capable formats.
 * Returns total count (onscreen + pbuffer-only).
 * Onscreen formats are listed first (indices 1..onscreen_count),
 * then pbuffer-only formats (onscreen_count+1..total).
 */
int mesa_wine_get_pixel_format_count(mesa_wine_screen_t screen,
                                     int *onscreen_count);

/*
 * Get info about a pixel format (1-based index).
 * Returns true on success, false if index is out of range.
 */
bool mesa_wine_get_pixel_format_info(mesa_wine_screen_t screen,
                                     int index,
                                     struct mesa_wine_pixel_format_info *info);

/*
 * Create a GL context.
 *   screen: from mesa_wine_init()
 *   format: 1-based pixel format index (visual must match drawable)
 *   major, minor: requested GL version (e.g. 4, 6)
 *   profile: MESA_WINE_PROFILE_COMPAT or MESA_WINE_PROFILE_CORE
 *   flags: bitmask of MESA_WINE_FLAG_*
 *   share: shared context, or NULL
 * Returns context handle, or NULL on failure.
 */
mesa_wine_ctx_t mesa_wine_create_context(mesa_wine_screen_t screen,
                                         int format,
                                         int major, int minor,
                                         int profile, unsigned flags,
                                         mesa_wine_ctx_t share);

/*
 * Destroy a GL context.
 */
void mesa_wine_destroy_context(mesa_wine_ctx_t ctx);

/*
 * Create an offscreen drawable for rendering.
 *   screen: from mesa_wine_init()
 *   format: 1-based pixel format index (from mesa_wine_get_pixel_format_count)
 *   width, height: initial dimensions (> 0)
 * Returns drawable handle, or NULL on failure.
 */
mesa_wine_draw_t mesa_wine_create_drawable(mesa_wine_screen_t screen,
                                           int format,
                                           int width, int height);

/*
 * Destroy a drawable.
 */
void mesa_wine_destroy_drawable(mesa_wine_draw_t draw);

/*
 * Resize a drawable's render targets. Existing contents are lost.
 */
void mesa_wine_resize_drawable(mesa_wine_draw_t draw, int width, int height);

/*
 * Make a context current with draw/read surfaces.
 * Pass NULL for ctx to unbind the current context.
 * draw and read may be the same.
 * Returns true on success.
 */
bool mesa_wine_make_current(mesa_wine_ctx_t ctx,
                            mesa_wine_draw_t draw,
                            mesa_wine_draw_t read);

/*
 * Look up a GL function by name.
 * Returns function pointer, or NULL if not found.
 */
void *mesa_wine_get_proc_address(const char *name);

/*
 * Query the GL version string of the current context.
 * Returns NULL if no context is current.
 */
const char *mesa_wine_get_gl_version(void);

/*
 * Flush the current context.
 */
void mesa_wine_flush(void);

/*
 * Present the drawable back buffer; its contents are undefined after swapping.
 * If presentation is attached, blits to swapchain and presents.
 */
bool mesa_wine_swap_buffers(mesa_wine_draw_t draw);

/*
 * Attach a Vulkan swapchain to a drawable for on-screen display.
 *   screen: from mesa_wine_init()
 *   draw: target drawable
 *   metal_layer: a CAMetalLayer* (passed as void* to avoid ObjC dependency)
 * Returns true on success.
 */
bool mesa_wine_attach_presentation(mesa_wine_screen_t screen,
                                    mesa_wine_draw_t draw,
                                    void *metal_layer);

/*
 * Detach the presentation swapchain from a drawable.
 * Safe to call if no presentation is attached.
 */
void mesa_wine_detach_presentation(mesa_wine_draw_t draw);

#endif /* MESA_WINE_API_H */
