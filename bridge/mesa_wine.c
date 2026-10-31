/*
 * Mesa Wine Bridge — Implementation
 *
 * Wraps Mesa's Gallium state tracker (Zink + KosmicKrisp) behind
 * a simple C API that Wine's winemac.drv can dlopen/dlsym.
 *
 * Copyright 2026 mesa_gl_to_metal project
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Mesa headers */
#include "pipe/p_screen.h"
#include "pipe/p_context.h"
#include "pipe/p_state.h"
#include "pipe/p_defines.h"
#include "frontend/api.h"
#include "state_tracker/st_context.h"
#include "util/u_memory.h"
#include "util/u_inlines.h"
#include "util/format/u_formats.h"
#include "glapi/glapi/glapi.h"

/* Zink public API */
#include "gallium/drivers/zink/zink_public.h"

/* Zink internals — needed to access VkDevice, VkQueue, VkImage from pipe objects */
#include "gallium/drivers/zink/zink_types.h"

/* Vulkan headers for presentation */
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_metal.h>

/* Our public API */
#define MESA_WINE_EXPORT __attribute__((visibility("default")))
#include "mesa_wine_api.h"


/* ================================================================
 *  Internal structures
 * ================================================================ */

struct mesa_wine_screen {
    struct pipe_screen         *pipe_screen;
    struct pipe_frontend_screen fscreen;
};

struct mesa_wine_ctx {
    struct st_context *st;
    mesa_wine_screen_t screen;
};

/* Presentation state: Vulkan swapchain attached to a CAMetalLayer */
struct mesa_wine_present {
    VkSurfaceKHR     surface;
    VkSwapchainKHR   swapchain;
    VkImage         *swapchain_images;
    uint32_t         image_count;
    VkCommandPool    cmd_pool;
    VkCommandBuffer  cmd_buf;
    VkSemaphore      acquire_sem;
    VkSemaphore      render_sem;
    VkFence          fence;
    VkFence          present_fence;
    bool             present_pending;
    uint32_t         width, height;
    uint32_t         requested_width, requested_height;
    VkFormat         format;
    /* Vulkan function pointers loaded once at attach time */
    PFN_vkCreateMetalSurfaceEXT pfn_vkCreateMetalSurfaceEXT;
    bool failed;
    unsigned frames;
};

struct mesa_wine_draw {
    struct pipe_frontend_drawable base;
    struct st_visual              stvis;
    struct pipe_resource         *textures[ST_ATTACHMENT_COUNT];
    unsigned                      width;
    unsigned                      height;
    struct pipe_screen           *pipe_screen;
    uint32_t                      id;
    struct mesa_wine_present     *present;
};

static uint32_t next_drawable_id;


/* ================================================================
 *  Pixel format enumeration
 *
 *  Probes the single non-MSAA window format implemented by this milestone.
 * ================================================================ */

/* Internal format record — maps 1-based index to Gallium formats */
struct pixel_format_entry {
    enum pipe_format color_format;
    enum pipe_format depth_stencil_format;  /* PIPE_FORMAT_NONE = no depth */
    int              samples;               /* 0 = no MSAA */
    bool             doublebuffer;
    bool             pbuffer_only;
    /* Derived fields for quick lookup */
    int              color_bits, red_bits, green_bits, blue_bits, alpha_bits;
    int              red_shift, green_shift, blue_shift, alpha_shift;
    int              depth_bits, stencil_bits;
    bool             srgb;
    bool             float_components;
};

#define MAX_PIXEL_FORMATS 256
static struct pixel_format_entry pixel_formats[MAX_PIXEL_FORMATS];
static int num_pixel_formats = 0;
static int num_onscreen_formats = 0;

/* Initial window milestone: only the formats whose drawable path exists. */
static void enumerate_pixel_formats(struct pipe_screen *pscreen)
{
    const enum pipe_format color = PIPE_FORMAT_B8G8R8A8_UNORM;
    const enum pipe_format depth = PIPE_FORMAT_Z24_UNORM_S8_UINT;
    if (!pscreen->is_format_supported(pscreen, color, PIPE_TEXTURE_2D, 1, 1, PIPE_BIND_RENDER_TARGET) ||
        !pscreen->is_format_supported(pscreen, depth, PIPE_TEXTURE_2D, 1, 1, PIPE_BIND_DEPTH_STENCIL)) return;
    pixel_formats[0] = (struct pixel_format_entry){
        .color_format=color, .depth_stencil_format=depth, .doublebuffer=true,
        .color_bits=32, .red_bits=8, .green_bits=8, .blue_bits=8, .alpha_bits=8,
        .red_shift=16, .green_shift=8, .blue_shift=0, .alpha_shift=24,
        .depth_bits=24, .stencil_bits=8,
    };
    num_pixel_formats = num_onscreen_formats = 1;
}

/* ================================================================
 *  pipe_frontend_screen callbacks
 * ================================================================ */

/* ================================================================
 *  pipe_frontend_drawable callbacks — offscreen rendering
 * ================================================================ */

static bool
wine_drawable_flush_front(struct st_context *st,
                          struct pipe_frontend_drawable *drawable,
                          enum st_attachment_type statt)
{
    /* Offscreen: nothing to present */
    return true;
}

static bool
wine_drawable_validate(struct st_context *st,
                       struct pipe_frontend_drawable *drawable,
                       const enum st_attachment_type *statts,
                       unsigned count,
                       struct pipe_resource **out,
                       struct pipe_resource **resolve)
{
    struct mesa_wine_draw *mwd = (struct mesa_wine_draw *)drawable;
    unsigned i;

    for (i = 0; i < count; i++) {
        enum st_attachment_type att = statts[i];
        enum pipe_format format;
        unsigned bind;

        /* Already allocated at current size? */
        if (mwd->textures[att] &&
            mwd->textures[att]->width0 == mwd->width &&
            mwd->textures[att]->height0 == mwd->height)
        {
            pipe_resource_reference(&out[i], mwd->textures[att]);
            continue;
        }

        /* Release old texture if size changed */
        pipe_resource_reference(&mwd->textures[att], NULL);

        switch (att) {
        case ST_ATTACHMENT_FRONT_LEFT:
        case ST_ATTACHMENT_BACK_LEFT:
            format = mwd->stvis.color_format;
            bind = PIPE_BIND_RENDER_TARGET | PIPE_BIND_SAMPLER_VIEW;
            break;
        case ST_ATTACHMENT_DEPTH_STENCIL:
            format = mwd->stvis.depth_stencil_format;
            bind = PIPE_BIND_DEPTH_STENCIL;
            break;
        default:
            out[i] = NULL;
            continue;
        }

        struct pipe_resource templ;
        memset(&templ, 0, sizeof(templ));
        templ.target = PIPE_TEXTURE_2D;
        templ.format = format;
        templ.width0 = mwd->width;
        templ.height0 = mwd->height;
        templ.depth0 = 1;
        templ.array_size = 1;
        templ.last_level = 0;
        templ.nr_samples = 1;
        templ.nr_storage_samples = 1;
        templ.bind = bind;

        mwd->textures[att] =
            mwd->pipe_screen->resource_create(mwd->pipe_screen, &templ);

        if (!mwd->textures[att]) {
            fprintf(stderr, "mesa_wine: failed to create texture for attachment %d\n", att);
            out[i] = NULL;
            return false;
        }

        pipe_resource_reference(&out[i], mwd->textures[att]);
    }

    if (resolve) {
        /* No MSAA for now */
        *resolve = NULL;
    }

    return true;
}

static bool
wine_drawable_flush_swapbuffers(struct st_context *st,
                                struct pipe_frontend_drawable *drawable)
{
    return true;
}


/* ================================================================
 *  Presentation helpers — Vulkan swapchain blit
 * ================================================================ */

/* Get zink_screen from pipe_screen (static cast — Zink is our only driver) */
static inline struct zink_screen *
get_zink_screen(struct pipe_screen *pscreen)
{
    return (struct zink_screen *)pscreen;
}

static VkSemaphore
create_semaphore(struct zink_screen *screen)
{
    VkSemaphoreCreateInfo sci = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };
    VkSemaphore sem;
    VkResult res = VKSCR(CreateSemaphore)(screen->dev, &sci, NULL, &sem);
    return (res == VK_SUCCESS) ? sem : VK_NULL_HANDLE;
}

static VkFence
create_fence(struct zink_screen *screen)
{
    VkFenceCreateInfo fci = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };
    VkFence fence;
    VkResult res = VKSCR(CreateFence)(screen->dev, &fci, NULL, &fence);
    return (res == VK_SUCCESS) ? fence : VK_NULL_HANDLE;
}

static bool
create_swapchain(struct mesa_wine_present *p, struct zink_screen *screen,
                 uint32_t width, uint32_t height, VkSwapchainKHR old_swapchain)
{
    VkSurfaceCapabilitiesKHR caps;
    VkResult res;
    uint32_t requested_width = width, requested_height = height;

    res = VKSCR(GetPhysicalDeviceSurfaceCapabilitiesKHR)(screen->pdev, p->surface, &caps);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "mesa_wine: GetPhysicalDeviceSurfaceCapabilitiesKHR failed (%d)\n", res);
        return false;
    }

    uint32_t format_count = 0;
    if (VKSCR(GetPhysicalDeviceSurfaceFormatsKHR)(screen->pdev, p->surface, &format_count, NULL) != VK_SUCCESS) return false;
    VkSurfaceFormatKHR *formats = calloc(format_count, sizeof(*formats));
    if (!formats) return false;
    bool supported = false;
    if (VKSCR(GetPhysicalDeviceSurfaceFormatsKHR)(screen->pdev, p->surface, &format_count, formats) == VK_SUCCESS)
        for (unsigned i = 0; i < format_count; i++)
            supported |= formats[i].format == VK_FORMAT_B8G8R8A8_UNORM && formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    free(formats);
    if (!supported || !(caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) ||
        !(caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)) return false;
    if (caps.currentExtent.width != UINT32_MAX) {
        width = caps.currentExtent.width; height = caps.currentExtent.height;
    }

    /* Clamp to surface capabilities */
    if (width < caps.minImageExtent.width) width = caps.minImageExtent.width;
    if (width > caps.maxImageExtent.width) width = caps.maxImageExtent.width;
    if (height < caps.minImageExtent.height) height = caps.minImageExtent.height;
    if (height > caps.maxImageExtent.height) height = caps.maxImageExtent.height;

    uint32_t image_count = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && image_count > caps.maxImageCount)
        image_count = caps.maxImageCount;

    VkSwapchainCreateInfoKHR sci = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = p->surface,
        .minImageCount = image_count,
        .imageFormat = VK_FORMAT_B8G8R8A8_UNORM,
        .imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
        .imageExtent = { width, height },
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = caps.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
        .clipped = VK_TRUE,
        .oldSwapchain = old_swapchain,
    };

    VkSwapchainKHR new_swapchain;
    res = VKSCR(CreateSwapchainKHR)(screen->dev, &sci, NULL, &new_swapchain);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "mesa_wine: vkCreateSwapchainKHR failed (%d)\n", res);
        return false;
    }

    p->swapchain = new_swapchain;

    /* Destroy old swapchain after creating new one */
    if (old_swapchain != VK_NULL_HANDLE)
        VKSCR(DestroySwapchainKHR)(screen->dev, old_swapchain, NULL);

    /* Get swapchain images */
    if (VKSCR(GetSwapchainImagesKHR)(screen->dev, p->swapchain, &p->image_count, NULL) != VK_SUCCESS) return false;
    free(p->swapchain_images);
    p->swapchain_images = calloc(p->image_count, sizeof(VkImage));
    if (!p->swapchain_images || VKSCR(GetSwapchainImagesKHR)(screen->dev, p->swapchain, &p->image_count, p->swapchain_images) != VK_SUCCESS) return false;

    p->width = width;
    p->height = height;
    p->requested_width = requested_width;
    p->requested_height = requested_height;
    p->format = VK_FORMAT_B8G8R8A8_UNORM;

    fprintf(stderr, "mesa_wine: swapchain created %ux%u (%u images)\n",
            width, height, p->image_count);
    return true;
}

static void
destroy_presentation(struct mesa_wine_present *p, struct zink_screen *screen)
{
    if (!p)
        return;

    VKSCR(DeviceWaitIdle)(screen->dev);

    if (p->present_pending)
        VKSCR(WaitForFences)(screen->dev, 1, &p->present_fence, VK_TRUE, UINT64_MAX);
    if (p->present_fence != VK_NULL_HANDLE)
        VKSCR(DestroyFence)(screen->dev, p->present_fence, NULL);
    if (p->fence != VK_NULL_HANDLE)
        VKSCR(DestroyFence)(screen->dev, p->fence, NULL);
    if (p->acquire_sem != VK_NULL_HANDLE)
        VKSCR(DestroySemaphore)(screen->dev, p->acquire_sem, NULL);
    if (p->render_sem != VK_NULL_HANDLE)
        VKSCR(DestroySemaphore)(screen->dev, p->render_sem, NULL);
    if (p->cmd_pool != VK_NULL_HANDLE)
        VKSCR(DestroyCommandPool)(screen->dev, p->cmd_pool, NULL);
    free(p->swapchain_images);
    if (p->swapchain != VK_NULL_HANDLE)
        VKSCR(DestroySwapchainKHR)(screen->dev, p->swapchain, NULL);
    if (p->surface != VK_NULL_HANDLE)
        VKSCR(DestroySurfaceKHR)(screen->instance, p->surface, NULL);

    free(p);
}

/* Blit the rendered back buffer to the swapchain image and present */
static bool
present_frame(struct mesa_wine_draw *mwd)
{
    struct mesa_wine_present *p = mwd->present;
    if (!p || p->failed || p->swapchain == VK_NULL_HANDLE)
        return false;

    struct zink_screen *screen = get_zink_screen(mwd->pipe_screen);

    /* Get the back buffer's VkImage from Zink */
    struct pipe_resource *back = mwd->textures[ST_ATTACHMENT_BACK_LEFT];
    if (!back)
        back = mwd->textures[ST_ATTACHMENT_FRONT_LEFT];
    if (!back)
        return false;

    struct zink_resource *zres = zink_resource(back);
    VkImage src_image = zres->obj->image;

    /* A submit fence alone does not cover the presentation semaphore lifetime.
     * https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html */
    if (p->present_pending) {
        if (VKSCR(WaitForFences)(screen->dev, 1, &p->present_fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) return false;
        p->present_pending = false;
    }
    if (VKSCR(WaitForFences)(screen->dev, 1, &p->fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) return false;

    if (p->requested_width != mwd->width || p->requested_height != mwd->height)
        if (!create_swapchain(p, screen, mwd->width, mwd->height, p->swapchain)) return false;

    /* Acquire swapchain image */
    uint32_t image_idx;
    VkResult res = VKSCR(AcquireNextImageKHR)(screen->dev, p->swapchain,
                                               UINT64_MAX, p->acquire_sem,
                                               VK_NULL_HANDLE, &image_idx);
    if (res == VK_ERROR_OUT_OF_DATE_KHR) {
        /* Recreate swapchain at current drawable size */
        if (!create_swapchain(p, screen, mwd->width, mwd->height, p->swapchain))
            return false;
        /* Retry acquire */
        res = VKSCR(AcquireNextImageKHR)(screen->dev, p->swapchain,
                                          UINT64_MAX, p->acquire_sem,
                                          VK_NULL_HANDLE, &image_idx);
    }
    if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
        fprintf(stderr, "mesa_wine: AcquireNextImageKHR failed (%d)\n", res);
        return false;
    }

    VkImage dst_image = p->swapchain_images[image_idx];

    /* Record blit command buffer */
    VkCommandBufferBeginInfo cbbi = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    if (VKSCR(ResetCommandPool)(screen->dev, p->cmd_pool, 0) != VK_SUCCESS) { p->failed = true; return false; }
    if (VKSCR(BeginCommandBuffer)(p->cmd_buf, &cbbi) != VK_SUCCESS) { p->failed = true; return false; }

    /* Transition source image (back buffer) from its current layout to TRANSFER_SRC */
    VkImageMemoryBarrier barriers[2];
    memset(barriers, 0, sizeof(barriers));

    barriers[0] = (VkImageMemoryBarrier){
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
        .oldLayout = zres->layout,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = src_image,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
    };

    /* Transition swapchain image from UNDEFINED to TRANSFER_DST */
    barriers[1] = (VkImageMemoryBarrier){
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = dst_image,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
    };

    VKSCR(CmdPipelineBarrier)(p->cmd_buf,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, NULL, 0, NULL, 2, barriers);

    /* Blit from back buffer to swapchain image (handles size mismatch) */
    VkImageBlit region = {
        .srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .srcOffsets = {
            { 0, 0, 0 },
            { (int32_t)mwd->width, (int32_t)mwd->height, 1 },
        },
        .dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .dstOffsets = {
            { 0, 0, 0 },
            { (int32_t)p->width, (int32_t)p->height, 1 },
        },
    };

    VKSCR(CmdBlitImage)(p->cmd_buf,
        src_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        dst_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &region, VK_FILTER_LINEAR);

    /* Transition source back to its original layout */
    barriers[0].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barriers[0].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    barriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barriers[0].newLayout = zres->layout;

    /* Transition swapchain image to PRESENT_SRC */
    barriers[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barriers[1].dstAccessMask = 0;
    barriers[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barriers[1].newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VKSCR(CmdPipelineBarrier)(p->cmd_buf,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0, 0, NULL, 0, NULL, 2, barriers);

    if (VKSCR(EndCommandBuffer)(p->cmd_buf) != VK_SUCCESS) { p->failed = true; return false; }

    /* Submit with semaphore synchronization */
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkSubmitInfo si = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &p->acquire_sem,
        .pWaitDstStageMask = &wait_stage,
        .commandBufferCount = 1,
        .pCommandBuffers = &p->cmd_buf,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &p->render_sem,
    };

    if (VKSCR(ResetFences)(screen->dev, 1, &p->fence) != VK_SUCCESS) { p->failed = true; return false; }
    simple_mtx_lock(screen->queue_lock);
    res = VKSCR(QueueSubmit)(screen->queue, 1, &si, p->fence);
    simple_mtx_unlock(screen->queue_lock);

    if (res != VK_SUCCESS) {
        p->failed = true;
        fprintf(stderr, "mesa_wine: QueueSubmit (present) failed (%d)\n", res);
        return false;
    }

    /* Present fences make reuse and swapchain teardown explicit. */
    if (VKSCR(ResetFences)(screen->dev, 1, &p->present_fence) != VK_SUCCESS) return false;
    VkSwapchainPresentFenceInfoKHR fence_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_KHR,
        .swapchainCount = 1, .pFences = &p->present_fence,
    };
    VkPresentInfoKHR pi = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = &fence_info,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &p->render_sem,
        .swapchainCount = 1,
        .pSwapchains = &p->swapchain,
        .pImageIndices = &image_idx,
    };

    simple_mtx_lock(screen->queue_lock);
    res = VKSCR(QueuePresentKHR)(screen->queue, &pi);
    simple_mtx_unlock(screen->queue_lock);

    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
        /* Will recreate swapchain on next frame */
    } else if (res != VK_SUCCESS) {
        fprintf(stderr, "mesa_wine: QueuePresentKHR failed (%d)\n", res);
    }

    p->present_pending = res == VK_SUCCESS || res == VK_SUBOPTIMAL_KHR;
    if (p->present_pending) p->frames++;
    if (p->present_pending && p->frames == 1) fprintf(stderr, "mesa_wine: first GPU frame presented\n");
    return res == VK_SUCCESS || res == VK_SUBOPTIMAL_KHR;

}


/* ================================================================
 *  Public API implementation
 * ================================================================ */

MESA_WINE_EXPORT mesa_wine_screen_t
mesa_wine_init(void)
{
    struct mesa_wine_screen *ws;
    struct pipe_screen *pscreen;

    const char *forbidden[] = {"MESA_GL_VERSION_OVERRIDE", "MESA_GLSL_VERSION_OVERRIDE",
                               "MESA_EXTENSION_OVERRIDE", "DYLD_INSERT_LIBRARIES"};
    for (unsigned i = 0; i < sizeof(forbidden) / sizeof(forbidden[0]); i++)
        if (getenv(forbidden[i])) { fprintf(stderr, "mesa_wine: refusing %s\n", forbidden[i]); return NULL; }

    /* Create Zink pipe_screen (no sw_winsys needed for Vulkan) */
    pscreen = zink_create_screen(NULL, NULL);
    if (!pscreen) {
        fprintf(stderr, "mesa_wine: zink_create_screen() failed\n");
        return NULL;
    }

    fprintf(stderr, "mesa_wine: Zink screen created: %s\n",
            pscreen->get_name(pscreen));

    ws = calloc(1, sizeof(*ws));
    if (!ws) {
        pscreen->destroy(pscreen);
        return NULL;
    }

    ws->pipe_screen = pscreen;
    ws->fscreen.screen = pscreen;
    ws->fscreen.broken_invalidate = true;
    /* get_egl_image, validate_egl_image, set_background_context: not needed */

    /* Enumerate supported pixel formats */
    enumerate_pixel_formats(pscreen);

    return ws;
}


MESA_WINE_EXPORT void
mesa_wine_fini(mesa_wine_screen_t screen)
{
    if (!screen)
        return;

    st_screen_destroy(&screen->fscreen);

    if (screen->pipe_screen)
        screen->pipe_screen->destroy(screen->pipe_screen);

    free(screen);
}


MESA_WINE_EXPORT mesa_wine_ctx_t
mesa_wine_create_context(mesa_wine_screen_t screen,
                         int format,
                         int major, int minor,
                         int profile, unsigned flags,
                         mesa_wine_ctx_t share)
{
    struct mesa_wine_ctx *ctx;
    struct st_context_attribs attribs;
    enum st_context_error error;
    const struct pixel_format_entry *e;

    if (!screen)
        return NULL;
    if (format < 1 || format > num_pixel_formats)
        return NULL;

    e = &pixel_formats[format - 1];

    ctx = calloc(1, sizeof(*ctx));
    if (!ctx)
        return NULL;

    ctx->screen = screen;

    memset(&attribs, 0, sizeof(attribs));
    attribs.major = major;
    attribs.minor = minor;
    attribs.profile = (profile == MESA_WINE_PROFILE_CORE)
                      ? API_OPENGL_CORE : API_OPENGL_COMPAT;

    if (flags & MESA_WINE_FLAG_FORWARD_COMPATIBLE)
        attribs.flags |= ST_CONTEXT_FLAG_FORWARD_COMPATIBLE;
    if (flags & MESA_WINE_FLAG_DEBUG)
        attribs.flags |= ST_CONTEXT_FLAG_DEBUG;

    /* Set up visual from pixel format — must match drawable visual */
    attribs.visual.color_format = e->color_format;
    attribs.visual.depth_stencil_format = e->depth_stencil_format;
    attribs.visual.accum_format = PIPE_FORMAT_NONE;
    attribs.visual.samples = e->samples;
    attribs.visual.buffer_mask = ST_ATTACHMENT_FRONT_LEFT_MASK;
    if (e->depth_stencil_format != PIPE_FORMAT_NONE)
        attribs.visual.buffer_mask |= ST_ATTACHMENT_DEPTH_STENCIL_MASK;
    if (e->doublebuffer)
        attribs.visual.buffer_mask |= ST_ATTACHMENT_BACK_LEFT_MASK;


    ctx->st = st_api_create_context(
        &screen->fscreen, &attribs, &error,
        share ? share->st : NULL);

    if (!ctx->st) {
        fprintf(stderr, "mesa_wine: st_api_create_context failed (error=%d, format=%d)\n",
                error, format);
        free(ctx);
        return NULL;
    }

    fprintf(stderr, "mesa_wine: GL context created (version %d.%d, profile %s, format %d)\n",
            major, minor,
            profile == MESA_WINE_PROFILE_CORE ? "core" : "compat",
            format);

    return ctx;
}


MESA_WINE_EXPORT void
mesa_wine_destroy_context(mesa_wine_ctx_t ctx)
{
    if (!ctx)
        return;

    if (ctx->st) {
        /* Unbind first if this context is current */
        if (st_api_get_current() == ctx->st)
            st_api_make_current(NULL, NULL, NULL);

        st_destroy_context(ctx->st);
    }

    free(ctx);
}


MESA_WINE_EXPORT int
mesa_wine_get_pixel_format_count(mesa_wine_screen_t screen, int *onscreen_count)
{
    (void)screen;
    if (onscreen_count)
        *onscreen_count = num_onscreen_formats;
    return num_pixel_formats;
}


MESA_WINE_EXPORT bool
mesa_wine_get_pixel_format_info(mesa_wine_screen_t screen, int index,
                                struct mesa_wine_pixel_format_info *info)
{
    const struct pixel_format_entry *e;
    (void)screen;

    if (index < 1 || index > num_pixel_formats || !info)
        return false;

    e = &pixel_formats[index - 1];  /* 1-based */

    info->color_bits = e->color_bits;
    info->red_bits = e->red_bits;
    info->green_bits = e->green_bits;
    info->blue_bits = e->blue_bits;
    info->alpha_bits = e->alpha_bits;
    info->red_shift = e->red_shift;
    info->green_shift = e->green_shift;
    info->blue_shift = e->blue_shift;
    info->alpha_shift = e->alpha_shift;
    info->depth_bits = e->depth_bits;
    info->stencil_bits = e->stencil_bits;
    info->samples = e->samples;
    info->doublebuffer = e->doublebuffer;
    info->srgb = e->srgb;
    info->pbuffer = e->pbuffer_only;
    info->float_components = e->float_components;

    return true;
}


MESA_WINE_EXPORT mesa_wine_draw_t
mesa_wine_create_drawable(mesa_wine_screen_t screen,
                          int format,
                          int width, int height)
{
    struct mesa_wine_draw *mwd;
    const struct pixel_format_entry *e;

    if (!screen || width <= 0 || height <= 0)
        return NULL;
    if (format < 1 || format > num_pixel_formats)
        return NULL;

    e = &pixel_formats[format - 1];

    mwd = calloc(1, sizeof(*mwd));
    if (!mwd)
        return NULL;

    mwd->width = width;
    mwd->height = height;
    mwd->pipe_screen = screen->pipe_screen;

    /* Set up visual from pixel format */
    mwd->stvis.color_format = e->color_format;
    mwd->stvis.depth_stencil_format = e->depth_stencil_format;
    mwd->stvis.accum_format = PIPE_FORMAT_NONE;
    mwd->stvis.samples = e->samples;
    mwd->stvis.buffer_mask = ST_ATTACHMENT_FRONT_LEFT_MASK;
    if (e->depth_stencil_format != PIPE_FORMAT_NONE)
        mwd->stvis.buffer_mask |= ST_ATTACHMENT_DEPTH_STENCIL_MASK;
    if (e->doublebuffer)
        mwd->stvis.buffer_mask |= ST_ATTACHMENT_BACK_LEFT_MASK;

    /* Set up the pipe_frontend_drawable interface */
    mwd->base.fscreen = &screen->fscreen;
    mwd->base.visual = &mwd->stvis;
    mwd->base.flush_front = wine_drawable_flush_front;
    mwd->base.validate = wine_drawable_validate;
    mwd->base.flush_swapbuffers = wine_drawable_flush_swapbuffers;
    mwd->id = p_atomic_inc_return(&next_drawable_id);
    mwd->base.ID = mwd->id;

    return mwd;
}


MESA_WINE_EXPORT void
mesa_wine_destroy_drawable(mesa_wine_draw_t draw)
{
    unsigned i;

    if (!draw)
        return;

    /* Clean up presentation if attached */
    if (draw->present) {
        struct zink_screen *screen = get_zink_screen(draw->pipe_screen);
        destroy_presentation(draw->present, screen);
        draw->present = NULL;
    }

    /* Notify Mesa that this drawable is being destroyed */
    st_api_destroy_drawable(&draw->base);

    for (i = 0; i < ST_ATTACHMENT_COUNT; i++)
        pipe_resource_reference(&draw->textures[i], NULL);

    free(draw);
}


MESA_WINE_EXPORT void
mesa_wine_resize_drawable(mesa_wine_draw_t draw, int width, int height)
{
    unsigned i;

    if (!draw || width <= 0 || height <= 0)
        return;

    if ((unsigned)width == draw->width && (unsigned)height == draw->height)
        return;

    draw->width = width;
    draw->height = height;

    /* Release existing textures — they'll be recreated in validate */
    for (i = 0; i < ST_ATTACHMENT_COUNT; i++)
        pipe_resource_reference(&draw->textures[i], NULL);

    /* Bump the stamp so Mesa knows to re-validate */
    p_atomic_inc(&draw->base.stamp);
}


MESA_WINE_EXPORT bool
mesa_wine_make_current(mesa_wine_ctx_t ctx,
                       mesa_wine_draw_t draw,
                       mesa_wine_draw_t read)
{
    if (!ctx) {
        /* Unbind */
        return st_api_make_current(NULL, NULL, NULL);
    }

    if (!draw || !read)
        return false;

    return st_api_make_current(ctx->st, &draw->base, &read->base);
}


MESA_WINE_EXPORT void *
mesa_wine_get_proc_address(const char *name)
{
    if (!name)
        return NULL;

    return (void *)_mesa_glapi_get_proc_address(name);
}


MESA_WINE_EXPORT const char *
mesa_wine_get_gl_version(void)
{
    typedef const unsigned char *(*GetStringFunc)(unsigned int);
    GetStringFunc get_string;

    get_string = (GetStringFunc)_mesa_glapi_get_proc_address("glGetString");
    if (!get_string)
        return NULL;

    /* GL_VERSION = 0x1F02 */
    return (const char *)get_string(0x1F02);
}


MESA_WINE_EXPORT void
mesa_wine_flush(void)
{
    struct st_context *st = st_api_get_current();
    if (st)
        st_context_flush(st, ST_FLUSH_FRONT, NULL, NULL, NULL);
}


MESA_WINE_EXPORT unsigned mesa_wine_get_abi_version(void) { return MESA_WINE_ABI_VERSION; }

MESA_WINE_EXPORT bool
mesa_wine_swap_buffers(mesa_wine_draw_t draw)
{
    struct st_context *st = st_api_get_current();
    if (!st || !draw || !draw->present) return false;
    /* Request an actual render fence: ST_FLUSH_WAIT with a NULL fence output
     * does not wait in st_context_flush. Complete Mesa work before touching
     * its VkImage outside Zink, and propagate a failed wait. */
    struct pipe_fence_handle *render_fence = NULL;
    st_context_flush(st, ST_FLUSH_END_OF_FRAME, &render_fence, NULL, NULL);
    if (!render_fence) return false;
    bool completed = draw->pipe_screen->fence_finish(draw->pipe_screen, st->pipe,
                                                    render_fence, UINT64_MAX);
    draw->pipe_screen->fence_reference(draw->pipe_screen, &render_fence, NULL);
    if (!completed) return false;
    if (!draw->present->frames) fprintf(stderr, "mesa_wine: render fence completed before presentation\n");
    if (!present_frame(draw)) return false;
    struct pipe_resource *front = draw->textures[ST_ATTACHMENT_FRONT_LEFT];
    draw->textures[ST_ATTACHMENT_FRONT_LEFT] = draw->textures[ST_ATTACHMENT_BACK_LEFT];
    draw->textures[ST_ATTACHMENT_BACK_LEFT] = front;
    p_atomic_inc(&draw->base.stamp);
    /* The drawable stamp is checked only when framebuffer state is validated.
     * Like Mesa's WGL frontend, explicitly dirty that state after exchanging
     * resources so cached draws target the new back buffer without a rebind. */
    st_context_invalidate_state(st, ST_INVALIDATE_FB_STATE);
    return true;
}

/* ================================================================
 *  Presentation API — attach/detach swapchain
 * ================================================================ */

MESA_WINE_EXPORT bool
mesa_wine_attach_presentation(mesa_wine_screen_t screen_handle,
                               mesa_wine_draw_t draw,
                               void *metal_layer)
{
    if (!screen_handle || !draw || !metal_layer)
        return false;

    struct zink_screen *screen = get_zink_screen(screen_handle->pipe_screen);

    if (!screen->info.have_KHR_swapchain_maintenance1) {
        fprintf(stderr, "mesa_wine: presentation fences are required by this experiment\n");
        return false;
    }

    /* Detach existing presentation if any */
    if (draw->present) {
        destroy_presentation(draw->present, screen);
        draw->present = NULL;
    }

    struct mesa_wine_present *p = calloc(1, sizeof(*p));
    if (!p)
        return false;

    /* Load VK_EXT_metal_surface function */
    p->pfn_vkCreateMetalSurfaceEXT = (PFN_vkCreateMetalSurfaceEXT)
        screen->vk_GetInstanceProcAddr(screen->instance, "vkCreateMetalSurfaceEXT");
    if (!p->pfn_vkCreateMetalSurfaceEXT) {
        fprintf(stderr, "mesa_wine: vkCreateMetalSurfaceEXT not available\n");
        free(p);
        return false;
    }

    /* Create VkSurfaceKHR from CAMetalLayer */
    VkMetalSurfaceCreateInfoEXT msci = {
        .sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT,
        .pLayer = (const CAMetalLayer *)metal_layer,
    };
    VkResult res = p->pfn_vkCreateMetalSurfaceEXT(screen->instance, &msci, NULL, &p->surface);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "mesa_wine: vkCreateMetalSurfaceEXT failed (%d)\n", res);
        free(p);
        return false;
    }

    /* Verify surface support on our physical device */
    VkBool32 supported = VK_FALSE;
    VKSCR(GetPhysicalDeviceSurfaceSupportKHR)(screen->pdev, screen->gfx_queue,
                                               p->surface, &supported);
    if (!supported) {
        fprintf(stderr, "mesa_wine: surface not supported on physical device\n");
        VKSCR(DestroySurfaceKHR)(screen->instance, p->surface, NULL);
        free(p);
        return false;
    }

    /* Create swapchain */
    p->swapchain = VK_NULL_HANDLE;
    if (!create_swapchain(p, screen, draw->width, draw->height, VK_NULL_HANDLE)) {
        VKSCR(DestroySurfaceKHR)(screen->instance, p->surface, NULL);
        free(p);
        return false;
    }

    /* Create command pool and buffer */
    VkCommandPoolCreateInfo cpci = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = screen->gfx_queue,
    };
    res = VKSCR(CreateCommandPool)(screen->dev, &cpci, NULL, &p->cmd_pool);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "mesa_wine: vkCreateCommandPool failed (%d)\n", res);
        goto fail;
    }

    VkCommandBufferAllocateInfo cbai = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = p->cmd_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    res = VKSCR(AllocateCommandBuffers)(screen->dev, &cbai, &p->cmd_buf);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "mesa_wine: vkAllocateCommandBuffers failed (%d)\n", res);
        goto fail;
    }

    /* Create synchronization objects */
    p->acquire_sem = create_semaphore(screen);
    p->render_sem = create_semaphore(screen);
    p->fence = create_fence(screen);
    p->present_fence = create_fence(screen);

    if (p->acquire_sem == VK_NULL_HANDLE ||
        p->render_sem == VK_NULL_HANDLE ||
        p->fence == VK_NULL_HANDLE || p->present_fence == VK_NULL_HANDLE)
    {
        fprintf(stderr, "mesa_wine: failed to create sync objects\n");
        goto fail;
    }

    draw->present = p;
    fprintf(stderr, "mesa_wine: presentation attached (%ux%u)\n",
            p->width, p->height);
    return true;

fail:
    destroy_presentation(p, screen);
    return false;
}


MESA_WINE_EXPORT void
mesa_wine_detach_presentation(mesa_wine_draw_t draw)
{
    if (!draw || !draw->present)
        return;

    struct zink_screen *screen = get_zink_screen(draw->pipe_screen);
    destroy_presentation(draw->present, screen);
    draw->present = NULL;
}
