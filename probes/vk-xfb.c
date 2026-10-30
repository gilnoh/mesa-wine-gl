/* Vertex and explicit GS transform-feedback development probe. No GL version
 * overrides. Validates GPU output, byte counters, untouched guards,
 * pause/resume and bounds.
 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>
#define VK(call)                                                               \
  do {                                                                         \
    VkResult r = (call);                                                       \
    if (r != VK_SUCCESS) {                                                     \
      fprintf(stderr, "%s failed: %d at line %d\n", #call, r, __LINE__);       \
      exit(2);                                                                 \
    }                                                                          \
  } while (0)
static VkDevice dev;
static VkPhysicalDevice gpu;
static VkQueue queue;
static VkCommandPool pool;
static VkPipelineLayout layout;
static VkPipelineCache pipeline_cache;
static int render_mode, wrong_query, test_dynamic;
static VkImageView render_view;
static VkPrimitiveTopology test_topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
static int require_cache_hit, cache_miss_control;
static PFN_vkCmdBindTransformFeedbackBuffersEXT bind_xfb;
static PFN_vkCmdBeginTransformFeedbackEXT begin_xfb;
static PFN_vkCmdEndTransformFeedbackEXT end_xfb;
static PFN_vkCmdBeginQueryIndexedEXT begin_query_indexed;
static PFN_vkCmdEndQueryIndexedEXT end_query_indexed;
struct buffer {
  VkBuffer handle;
  VkDeviceMemory memory;
  unsigned char *map;
};
static struct buffer make_buffer(VkDeviceSize size, VkBufferUsageFlags usage) {
  struct buffer b = {0};
  VkBufferCreateInfo info = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                             .size = size,
                             .usage = usage};
  VK(vkCreateBuffer(dev, &info, NULL, &b.handle));
  VkMemoryRequirements req;
  vkGetBufferMemoryRequirements(dev, b.handle, &req);
  VkPhysicalDeviceMemoryProperties props;
  vkGetPhysicalDeviceMemoryProperties(gpu, &props);
  uint32_t type = UINT32_MAX;
  for (uint32_t i = 0; i < props.memoryTypeCount; i++)
    if ((req.memoryTypeBits & (1u << i)) &&
        (props.memoryTypes[i].propertyFlags &
         (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
            (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
      type = i;
      break;
    }
  if (type == UINT32_MAX) {
    fprintf(stderr, "No host-coherent memory\n");
    exit(2);
  }
  VkMemoryAllocateInfo alloc = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                .allocationSize = req.size,
                                .memoryTypeIndex = type};
  VK(vkAllocateMemory(dev, &alloc, NULL, &b.memory));
  VK(vkBindBufferMemory(dev, b.handle, b.memory, 0));
  VK(vkMapMemory(dev, b.memory, 0, VK_WHOLE_SIZE, 0, (void **)&b.map));
  return b;
}
static void free_buffer(struct buffer b) {
  vkUnmapMemory(dev, b.memory);
  vkDestroyBuffer(dev, b.handle, NULL);
  vkFreeMemory(dev, b.memory, NULL);
}
static VkShaderModule shader(const char *dir, const char *name) {
  char path[4096];
  snprintf(path, sizeof(path), "%s/%s.spv", dir, name);
  FILE *f = fopen(path, "rb");
  if (!f) {
    perror(path);
    exit(2);
  }
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  rewind(f);
  uint32_t *code = malloc((size_t)size);
  if (fread(code, 1, (size_t)size, f) != (size_t)size)
    exit(2);
  fclose(f);
  VkShaderModuleCreateInfo info = {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = (size_t)size,
      .pCode = code};
  VkShaderModule module;
  VK(vkCreateShaderModule(dev, &info, NULL, &module));
  free(code);
  return module;
}
static VkPipeline pipeline(const char *dir, const char *gs) {
  VkShaderModule vs =
      shader(dir, render_mode ? "render.vert"
                              : (gs ? "capture.vert" : "vertex-only.vert"));
  VkShaderModule geom = gs ? shader(dir, gs) : VK_NULL_HANDLE;
  VkShaderModule fs = render_mode ? shader(dir, "render.frag") : VK_NULL_HANDLE;
  VkPipelineShaderStageCreateInfo stages[] = {
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       .stage = VK_SHADER_STAGE_VERTEX_BIT,
       .module = vs,
       .pName = "main"},
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       .stage = VK_SHADER_STAGE_GEOMETRY_BIT,
       .module = geom,
       .pName = "main"},
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
       .module = fs,
       .pName = "main"}};
  if (!gs && render_mode)
    stages[1] = stages[2];
  VkPipelineVertexInputStateCreateInfo vi = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
  VkPipelineInputAssemblyStateCreateInfo ia = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = test_dynamic
                      ? (test_topology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP
                             ? VK_PRIMITIVE_TOPOLOGY_LINE_LIST
                             : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                      : test_topology};
  VkDynamicState dynamic = VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY;
  VkPipelineDynamicStateCreateInfo dyn = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .dynamicStateCount = 1,
      .pDynamicStates = &dynamic};
  VkViewport viewport = {0, 0, 8, 8, 0, 1};
  VkRect2D scissor = {{0, 0}, {8, 8}};
  VkPipelineViewportStateCreateInfo vp = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .pViewports = &viewport,
      .scissorCount = 1,
      .pScissors = &scissor};
  VkPipelineRasterizationStateCreateInfo rs = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .rasterizerDiscardEnable = !render_mode,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .lineWidth = 1};
  VkPipelineMultisampleStateCreateInfo ms = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};
  VkPipelineColorBlendAttachmentState blend = {.colorWriteMask = 15};
  VkPipelineColorBlendStateCreateInfo cb = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .attachmentCount = render_mode ? 1 : 0,
      .pAttachments = &blend};
  VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
  VkPipelineRenderingCreateInfo rendering = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
      .colorAttachmentCount = render_mode ? 1 : 0,
      .pColorAttachmentFormats = &format};
  VkPipelineCreationFeedback feedback = {0};
  VkPipelineCreationFeedbackCreateInfo feedback_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_CREATION_FEEDBACK_CREATE_INFO,
      .pPipelineCreationFeedback = &feedback};
  rendering.pNext = &feedback_info;
  VkGraphicsPipelineCreateInfo info = {
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .pNext = &rendering,
      .flags = (require_cache_hit || cache_miss_control)
                   ? VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT
                   : 0,
      .stageCount = (gs ? 2 : 1) + (render_mode ? 1 : 0),
      .pStages = stages,
      .pVertexInputState = &vi,
      .pDynamicState = test_dynamic ? &dyn : NULL,
      .pInputAssemblyState = &ia,
      .pViewportState = &vp,
      .pRasterizationState = &rs,
      .pMultisampleState = &ms,
      .pColorBlendState = &cb,
      .layout = layout};
  VkPipeline p = VK_NULL_HANDLE;
  VkResult result =
      vkCreateGraphicsPipelines(dev, pipeline_cache, 1, &info, NULL, &p);
  vkDestroyShaderModule(dev, vs, NULL);
  if (geom)
    vkDestroyShaderModule(dev, geom, NULL);
  if (fs)
    vkDestroyShaderModule(dev, fs, NULL);
  if (cache_miss_control) {
    if (result != VK_PIPELINE_COMPILE_REQUIRED) {
      fprintf(stderr, "Empty-cache control returned %d\n", result);
      exit(2);
    }
    puts("Empty-cache compile-required control: PASS");
    return VK_NULL_HANDLE;
  }
  VK(result);
  if (require_cache_hit &&
      (feedback.flags &
       (VK_PIPELINE_CREATION_FEEDBACK_VALID_BIT |
        VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT)) !=
          (VK_PIPELINE_CREATION_FEEDBACK_VALID_BIT |
           VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT)) {
    fprintf(stderr, "Expected application pipeline cache hit, flags=%u\n",
            feedback.flags);
    exit(2);
  }
  if (require_cache_hit)
    puts("Pipeline cache hit: PASS");
  return p;
}
static void barrier(VkCommandBuffer cmd) {
  VkMemoryBarrier b = {.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
                       .srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
                       .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT |
                                        VK_ACCESS_MEMORY_WRITE_BIT};
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                       VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &b, 0, NULL, 0,
                       NULL);
}
static void rendering_begin(VkCommandBuffer cmd) {
  VkRenderingAttachmentInfo att = {
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageView = render_view,
      .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue.color.float32 = {0, 0, 0, 0}};
  VkRenderingInfo info = {.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                          .renderArea = {{0, 0}, {8, 8}},
                          .layerCount = 1,
                          .colorAttachmentCount = render_mode ? 1 : 0,
                          .pColorAttachments = &att};
  vkCmdBeginRendering(cmd, &info);
}
static int run_case(VkPipeline p, const char *name, int dynamic, int resume,
                    int overflow, int flags, int wrong) {
  struct buffer output =
      make_buffer(512, VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT);
  struct buffer counter = make_buffer(
      16, VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_COUNTER_BUFFER_BIT_EXT);
  memset(output.map, 0xCD, 512);
  memset(counter.map, 0xCD, 16);
  if (resume == 2) {
    uint32_t n = 16;
    memcpy(counter.map + 4, &n, 4);
  }
  unsigned instances = (flags & 2) ? 2 : 1;
  VkQueryPool queries;
  VkQueryPoolCreateInfo qinfo = {
      .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
      .queryType = VK_QUERY_TYPE_TRANSFORM_FEEDBACK_STREAM_EXT,
      .queryCount = 3};
  VK(vkCreateQueryPool(dev, &qinfo, NULL, &queries));
  struct buffer query_copy = make_buffer(256, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
  memset(query_copy.map, 0xCD, 256);
  struct buffer indices = {0};
  if (flags & 1) {
    indices = make_buffer(24, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    uint32_t values[] = {99, 3, 1, 2, 0, 99};
    memcpy(indices.map, values, sizeof(values));
  }
  VkImage image = VK_NULL_HANDLE;
  VkDeviceMemory image_memory = VK_NULL_HANDLE;
  struct buffer pixels = {0};
  if (render_mode) {
    pixels = make_buffer(512, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    memset(pixels.map, 0xCD, 512);
    VkImageCreateInfo ii = {.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                            .imageType = VK_IMAGE_TYPE_2D,
                            .format = VK_FORMAT_R8G8B8A8_UNORM,
                            .extent = {8, 8, 1},
                            .mipLevels = 1,
                            .arrayLayers = 1,
                            .samples = VK_SAMPLE_COUNT_1_BIT,
                            .tiling = VK_IMAGE_TILING_OPTIMAL,
                            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                     VK_IMAGE_USAGE_TRANSFER_SRC_BIT};
    VK(vkCreateImage(dev, &ii, NULL, &image));
    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(dev, image, &req);
    uint32_t type = 0;
    while (!(req.memoryTypeBits & (1u << type)))
      type++;
    VkMemoryAllocateInfo ai = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                               .allocationSize = req.size,
                               .memoryTypeIndex = type};
    VK(vkAllocateMemory(dev, &ai, NULL, &image_memory));
    VK(vkBindImageMemory(dev, image, image_memory, 0));
    VkImageViewCreateInfo vi = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
    VK(vkCreateImageView(dev, &vi, NULL, &render_view));
  }
  VkCommandBufferAllocateInfo alloc = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1};
  VkCommandBuffer cmd;
  VK(vkAllocateCommandBuffers(dev, &alloc, &cmd));
  VkCommandBufferBeginInfo start = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
  VK(vkBeginCommandBuffer(cmd, &start));
  if (render_mode) {
    VkImageMemoryBarrier ib = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0,
                         NULL, 0, NULL, 1, &ib);
  }
  vkCmdResetQueryPool(cmd, queries, 0, 3);
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, p);
  if (test_dynamic)
    vkCmdSetPrimitiveTopology(cmd, test_topology);
  VkDeviceSize offset = 32, size = overflow ? 32 : 256, coff = 4;
  bind_xfb(cmd, 0, 1, &output.handle, &offset, &size);
  rendering_begin(cmd);
  if (flags & 1)
    begin_query_indexed(cmd, queries, 1, 0, 0);
  else
    vkCmdBeginQuery(cmd, queries, 1, 0);
  if (!(flags & 4))
    begin_xfb(cmd, 0, resume == 2 ? 1 : 0, resume == 2 ? &counter.handle : NULL,
              resume == 2 ? &coff : NULL);
  uint32_t first = resume ? 0 : 2, instance = resume ? 0 : 3;
  if (flags & 1) {
    vkCmdBindIndexBuffer(cmd, indices.handle, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, 4, instances, 1, 2, instance);
  } else if (flags & 8) {
    vkCmdDraw(cmd, 2, instances, first, instance);
    vkCmdDraw(cmd, 2, instances, first + 2, instance);
  } else
    vkCmdDraw(cmd, resume == 1 ? 2 : 4, instances, first, instance);
  if (!(flags & 4))
    end_xfb(cmd, 0, 1, &counter.handle, &coff);
  if (flags & 1)
    end_query_indexed(cmd, queries, 1, 0);
  else
    vkCmdEndQuery(cmd, queries, 1);
  vkCmdEndRendering(cmd);
  if (resume == 1) {
    barrier(cmd);
    rendering_begin(cmd);
    vkCmdBeginQuery(cmd, queries, 2, 0);
    begin_xfb(cmd, 0, 1, &counter.handle, &coff);
    vkCmdDraw(cmd, 2, 1, 2, 0);
    end_xfb(cmd, 0, 1, &counter.handle, &coff);
    vkCmdEndQuery(cmd, queries, 2);
    vkCmdEndRendering(cmd);
  }
  if (render_mode) {
    VkImageMemoryBarrier ib = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1,
                         &ib);
    VkBufferImageCopy copy = {
        .bufferOffset = 32,
        .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
        .imageExtent = {8, 8, 1}};
    vkCmdCopyImageToBuffer(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           pixels.handle, 1, &copy);
  }
  unsigned query_count = resume == 1 ? 2 : 1;
  vkCmdCopyQueryPoolResults(
      cmd, queries, 1, query_count, query_copy.handle, 32, 32,
      VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT |
          VK_QUERY_RESULT_WAIT_BIT);
  vkCmdCopyQueryPoolResults(
      cmd, queries, 1, query_count, query_copy.handle, 128, 16,
      VK_QUERY_RESULT_WITH_AVAILABILITY_BIT | VK_QUERY_RESULT_WAIT_BIT);
  VkMemoryBarrier host = {.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
                          .srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
                          .dstAccessMask = VK_ACCESS_HOST_READ_BIT};
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                       VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &host, 0, NULL, 0,
                       NULL);
  VK(vkEndCommandBuffer(cmd));
  VkSubmitInfo submit = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                         .commandBufferCount = 1,
                         .pCommandBuffers = &cmd};
  VkFenceCreateInfo finfo = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
  VkFence fence;
  VK(vkCreateFence(dev, &finfo, NULL, &fence));
  VK(vkQueueSubmit(queue, 1, &submit, fence));
  VK(vkWaitForFences(dev, 1, &fence, VK_TRUE, 10000000000ULL));
  unsigned char expected[512];
  memset(expected, 0xCD, sizeof(expected));
  unsigned count = 0, initial = resume == 2 ? 1 : 0;
  const unsigned index_values[] = {3, 1, 2, 0};
  const unsigned points[] = {0, 1, 2, 3};
  const unsigned lines[] = {0, 1, 1, 2, 2, 3};
  const unsigned triangles[] = {0, 1, 2, 1, 3, 2};
  const unsigned fan[] = {1, 2, 0, 2, 3, 0};
  const unsigned *order = points;
  unsigned vertices = 4, prim_width = 1;
  switch (test_topology) {
  case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
    prim_width = 2;
    break;
  case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
    order = lines;
    vertices = 6;
    prim_width = 2;
    break;
  case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
    vertices = 3;
    prim_width = 3;
    break;
  case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
    order = triangles;
    vertices = 6;
    prim_width = 3;
    break;
  case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
    order = fan;
    vertices = 6;
    prim_width = 3;
    break;
  default:
    break;
  }
  for (unsigned ins = 0; ins < instances && !(flags & 4); ins++)
    for (unsigned i = 0; i < vertices; i++) {
      /* Overflow must not capture a partial primitive. */
      if (i % prim_width == 0 && (initial + count + prim_width) * 16 > size)
        break;
      for (unsigned j = 0; j < (dynamic ? (i % 2 + 1) : 1); j++) {
        if ((initial + count + 1) * 16 > size)
          continue;
        float v[4] = {
            (float)(first + ((flags & 1) ? index_values[order[i]] : order[i])) +
                10.0f + (float)j * 100.0f,
            (float)(instance + ins) + 20.0f, 30, 1};
        memcpy(expected + 32 + (initial + count) * 16, v, 16);
        count++;
      }
    }
  if (wrong)
    expected[32 + initial * 16] ^= 1;
  unsigned mismatches = 0;
  for (unsigned i = 0; i < 512; i++)
    if (output.map[i] != expected[i])
      mismatches++;
  uint32_t bytes;
  memcpy(&bytes, counter.map + 4, 4);
  unsigned expected_bytes = (flags & 4) ? 0xCDCDCDCDu : (initial + count) * 16;
  unsigned counter_guard = 0;
  for (unsigned i = 0; i < 16; i++)
    if ((i < 4 || i >= 8) && counter.map[i] != 0xCD)
      counter_guard++;
  unsigned query_errors = 0;
  uint64_t unavailable[3] = {0xCDCDCDCDCDCDCDCDull, 0xCDCDCDCDCDCDCDCDull, 99};
  VkResult unready = vkGetQueryPoolResults(
      dev, queries, 0, 1, sizeof(unavailable), unavailable, sizeof(unavailable),
      VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
  if (unready != VK_NOT_READY || unavailable[2] != 0 ||
      unavailable[0] != 0xCDCDCDCDCDCDCDCDull ||
      unavailable[1] != 0xCDCDCDCDCDCDCDCDull)
    query_errors++;
  unsigned char expected_queries[256];
  memset(expected_queries, 0xCD, sizeof(expected_queries));
  uint64_t needed =
      (flags & 4) ? 0 : (dynamic ? 6 : vertices / prim_width) * instances;
  uint64_t written = count / prim_width;
  for (unsigned q = 0; q < query_count; q++) {
    uint64_t want[3] = {resume == 1 ? 2 : written, resume == 1 ? 2 : needed, 1};
    if (wrong_query)
      want[0]++;
    uint64_t got[3] = {0};
    VK(vkGetQueryPoolResults(
        dev, queries, q + 1, 1, sizeof(got), got, sizeof(got),
        VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT));
    if (memcmp(got, want, sizeof(got)))
      query_errors++;
    uint32_t got32[3] = {0},
             want32[3] = {(uint32_t)want[0], (uint32_t)want[1], 1};
    VK(vkGetQueryPoolResults(dev, queries, q + 1, 1, sizeof(got32), got32,
                             sizeof(got32),
                             VK_QUERY_RESULT_WITH_AVAILABILITY_BIT));
    if (memcmp(got32, want32, sizeof(got32)))
      query_errors++;
    memcpy(expected_queries + 32 + q * 32, want, sizeof(want));
    memcpy(expected_queries + 128 + q * 16, want32, sizeof(want32));
    printf("  Query %u: written=%llu needed=%llu available=%llu (expected "
           "%llu/%llu/1)\n",
           q + 1, (unsigned long long)got[0], (unsigned long long)got[1],
           (unsigned long long)got[2], (unsigned long long)want[0],
           (unsigned long long)want[1]);
  }
  for (unsigned i = 0; i < sizeof(expected_queries); i++)
    if (query_copy.map[i] != expected_queries[i])
      query_errors++;
  printf("  Query host/copy/availability/guards: %s errors=%u\n",
         query_errors ? "FAIL" : "PASS", query_errors);
  unsigned pixel_errors = 0;
  if (render_mode) {
    const unsigned char color[] = {64, 128, 191, 255};
    for (unsigned i = 0; i < 512; i++) {
      if (i >= 32 && i < 288) {
        int expected = color[(i - 32) % 4];
        if (wrong && i == 32)
          expected = 0;
        if (abs((int)pixels.map[i] - expected) > 1)
          pixel_errors++;
      } else if (pixels.map[i] != 0xCD)
        pixel_errors++;
    }
    printf("  Render readback: %s, mismatched bytes=%u\n",
           pixel_errors ? "FAIL" : "PASS", pixel_errors);
  }
  int failed = mismatches || counter_guard || bytes != expected_bytes ||
               pixel_errors || query_errors;
  printf("%s: %s bytes=%u expected=%u mismatched_output_bytes=%u "
         "counter_guard=%u\n",
         name, failed ? "FAIL" : "PASS", bytes, expected_bytes, mismatches,
         counter_guard);
  if (failed) {
    for (unsigned i = 0; i < 8; i++) {
      float v[4];
      memcpy(v, output.map + 32 + i * 16, 16);
      printf("  output[%u] = %.3f %.3f %.3f %.3f\n", i, v[0], v[1], v[2], v[3]);
    }
  }
  vkDestroyFence(dev, fence, NULL);
  vkFreeCommandBuffers(dev, pool, 1, &cmd);
  free_buffer(output);
  free_buffer(counter);
  free_buffer(query_copy);
  vkDestroyQueryPool(dev, queries, NULL);
  if (indices.handle)
    free_buffer(indices);
  if (render_mode) {
    free_buffer(pixels);
    vkDestroyImageView(dev, render_view, NULL);
    vkDestroyImage(dev, image, NULL);
    vkFreeMemory(dev, image_memory, NULL);
    render_view = VK_NULL_HANDLE;
  }
  return failed;
}
int main(int argc, char **argv) {
  setbuf(stdout, NULL);
  if (argc < 2) {
    fprintf(stderr, "usage: vk-xfb <shader-directory> [--wrong-expected]\n");
    return 2;
  }
  int wrong = 0;
  const char *cache_write = NULL, *cache_read = NULL;
  for (int i = 2; i < argc; i++) {
    if (!strcmp(argv[i], "--wrong-expected"))
      wrong = 1;
    else if (!strcmp(argv[i], "--wrong-query"))
      wrong_query = 1;
    else if (!strcmp(argv[i], "--cache-write") && i + 1 < argc)
      cache_write = argv[++i];
    else if (!strcmp(argv[i], "--cache-read") && i + 1 < argc)
      cache_read = argv[++i];
    else if (!strcmp(argv[i], "--cache-miss-control"))
      cache_miss_control = 1;
    else
      return 2;
  }
  require_cache_hit = cache_read != NULL;
  VkApplicationInfo app = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                           .pApplicationName = "KK XFB probe",
                           .apiVersion = VK_API_VERSION_1_3};
  VkInstanceCreateInfo ici = {.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                              .pApplicationInfo = &app};
  VkInstance inst;
  VK(vkCreateInstance(&ici, NULL, &inst));
  uint32_t n = 1;
  VK(vkEnumeratePhysicalDevices(inst, &n, &gpu));
  if (!n)
    return 2;
  VkPhysicalDeviceProperties props;
  vkGetPhysicalDeviceProperties(gpu, &props);
  printf("Device: %s\n", props.deviceName);
  VkPhysicalDeviceTransformFeedbackFeaturesEXT tf = {
      .sType =
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TRANSFORM_FEEDBACK_FEATURES_EXT};
  VkPhysicalDeviceFeatures2 features = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, .pNext = &tf};
  vkGetPhysicalDeviceFeatures2(gpu, &features);
  if (!tf.transformFeedback) {
    puts("UNSUPPORTED: transformFeedback (expected for baseline)");
    vkDestroyInstance(inst, NULL);
    return 77;
  }
  uint32_t count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(gpu, &count, NULL);
  VkQueueFamilyProperties *qp = calloc(count, sizeof(*qp));
  vkGetPhysicalDeviceQueueFamilyProperties(gpu, &count, qp);
  uint32_t family = UINT32_MAX;
  for (uint32_t i = 0; i < count; i++)
    if (qp[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      family = i;
      break;
    }
  free(qp);
  if (family == UINT32_MAX)
    return 2;
  float priority = 1;
  VkDeviceQueueCreateInfo qi = {.sType =
                                    VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                .queueFamilyIndex = family,
                                .queueCount = 1,
                                .pQueuePriorities = &priority};
  VkPhysicalDeviceVulkan13Features f13 = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
      .dynamicRendering = VK_TRUE,
      .synchronization2 = VK_TRUE};
  /* Pipeline creation cache control is part of Vulkan 1.3. */
  f13.pipelineCreationCacheControl = VK_TRUE;
  tf.pNext = &f13;
  tf.geometryStreams = VK_FALSE;
  VkPhysicalDeviceFeatures enabled = {
      .geometryShader = VK_TRUE,
      .shaderTessellationAndGeometryPointSize = VK_TRUE,
      .vertexPipelineStoresAndAtomics = VK_TRUE};
  const char *extensions[] = {VK_EXT_TRANSFORM_FEEDBACK_EXTENSION_NAME};
  VkDeviceCreateInfo di = {.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                           .pNext = &tf,
                           .queueCreateInfoCount = 1,
                           .pQueueCreateInfos = &qi,
                           .pEnabledFeatures = &enabled,
                           .enabledExtensionCount = 1,
                           .ppEnabledExtensionNames = extensions};
  VK(vkCreateDevice(gpu, &di, NULL, &dev));
  vkGetDeviceQueue(dev, family, 0, &queue);
  bind_xfb =
      (void *)vkGetDeviceProcAddr(dev, "vkCmdBindTransformFeedbackBuffersEXT");
  begin_xfb =
      (void *)vkGetDeviceProcAddr(dev, "vkCmdBeginTransformFeedbackEXT");
  end_xfb = (void *)vkGetDeviceProcAddr(dev, "vkCmdEndTransformFeedbackEXT");
  begin_query_indexed =
      (void *)vkGetDeviceProcAddr(dev, "vkCmdBeginQueryIndexedEXT");
  end_query_indexed =
      (void *)vkGetDeviceProcAddr(dev, "vkCmdEndQueryIndexedEXT");
  if (!bind_xfb || !begin_xfb || !end_xfb || !begin_query_indexed ||
      !end_query_indexed)
    return 2;
  VkCommandPoolCreateInfo pi = {.sType =
                                    VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                .queueFamilyIndex = family};
  VK(vkCreateCommandPool(dev, &pi, NULL, &pool));
  VkPipelineLayoutCreateInfo li = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  VK(vkCreatePipelineLayout(dev, &li, NULL, &layout));
  VkPipelineCacheCreateInfo pci = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
  void *cache_data = NULL;
  if (cache_read) {
    FILE *f = fopen(cache_read, "rb");
    if (!f)
      return 2;
    fseek(f, 0, SEEK_END);
    pci.initialDataSize = ftell(f);
    rewind(f);
    cache_data = malloc(pci.initialDataSize);
    if (fread(cache_data, 1, pci.initialDataSize, f) != pci.initialDataSize)
      return 2;
    fclose(f);
    pci.pInitialData = cache_data;
  }
  VK(vkCreatePipelineCache(dev, &pci, NULL, &pipeline_cache));
  free(cache_data);
  VkPipeline p = pipeline(argv[1], NULL);
  if (cache_miss_control) {
    vkDestroyPipelineCache(dev, pipeline_cache, NULL);
    vkDestroyPipelineLayout(dev, layout, NULL);
    vkDestroyCommandPool(dev, pool, NULL);
    vkDestroyDevice(dev, NULL);
    vkDestroyInstance(inst, NULL);
    return 0;
  }
  int failed = 0;
  failed +=
      run_case(p, "vertex capture/base vertex+instance", 0, 0, 0, 0, wrong);
  failed += run_case(p, "vertex pause/resume", 0, 1, 0, 0, wrong);
  failed += run_case(p, "vertex nonzero resume offset", 0, 2, 0, 0, wrong);
  failed += run_case(p, "vertex overflow guard", 0, 0, 1, 0, wrong);
  failed += run_case(p, "vertex instanced capture", 0, 0, 0, 2, wrong);
  failed += run_case(p, "vertex indexed capture", 0, 0, 0, 1, wrong);
  failed += run_case(p, "vertex capture disabled", 0, 0, 0, 4, wrong);
  failed +=
      run_case(p, "vertex two draws/query accumulation", 0, 0, 0, 8, wrong);
  vkDestroyPipeline(dev, p, NULL);
  p = pipeline(argv[1], "static.geom");
  failed +=
      run_case(p, "static capture/base vertex+instance", 0, 0, 0, 0, wrong);
  failed += run_case(p, "pause/resume", 0, 1, 0, 0, wrong);
  failed += run_case(p, "nonzero resume offset", 0, 2, 0, 0, wrong);
  failed += run_case(p, "buffer overflow guard", 0, 0, 1, 0, wrong);
  failed += run_case(p, "static instanced capture", 0, 0, 0, 2, wrong);
  failed += run_case(p, "static indexed capture", 0, 0, 0, 1, wrong);
  failed += run_case(p, "capture disabled", 0, 0, 0, 4, wrong);
  vkDestroyPipeline(dev, p, NULL);
  p = pipeline(argv[1], "dynamic.geom");
  failed += run_case(p, "dynamic GS counts/prefix sum", 1, 0, 0, 0, wrong);
  failed += run_case(p, "dynamic GS overflow guard", 1, 0, 1, 0, wrong);
  failed += run_case(p, "dynamic instanced capture", 1, 0, 0, 2, wrong);
  failed += run_case(p, "dynamic indexed capture", 1, 0, 0, 1, wrong);
  vkDestroyPipeline(dev, p, NULL);
  const VkPrimitiveTopology topologies[] = {
      VK_PRIMITIVE_TOPOLOGY_LINE_LIST, VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,
      VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
      VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN};
  const char *names[] = {"line list", "line strip", "triangle list",
                         "triangle strip", "triangle fan"};
  for (unsigned t = 0; t < 5; t++) {
    test_topology = topologies[t];
    p = pipeline(argv[1], NULL);
    char name[128];
    snprintf(name, sizeof(name), "vertex %s", names[t]);
    failed += run_case(p, name, 0, 0, 0, 0, wrong);
    snprintf(name, sizeof(name), "vertex %s indexed", names[t]);
    failed += run_case(p, name, 0, 0, 0, 1, wrong);
    snprintf(name, sizeof(name), "vertex %s whole-primitive overflow",
             names[t]);
    failed += run_case(p, name, 0, 0, 1, 0, wrong);
    vkDestroyPipeline(dev, p, NULL);
  }
  test_dynamic = 1;
  test_topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
  p = pipeline(argv[1], NULL);
  failed +=
      run_case(p, "dynamic line list-to-strip capture", 0, 0, 0, 0, wrong);
  vkDestroyPipeline(dev, p, NULL);
  test_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
  p = pipeline(argv[1], NULL);
  failed +=
      run_case(p, "dynamic triangle list-to-strip capture", 0, 0, 0, 0, wrong);
  test_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
  failed += run_case(p, "dynamic triangle strip-to-fan indexed capture", 0, 0,
                     0, 1, wrong);
  vkDestroyPipeline(dev, p, NULL);
  test_dynamic = 0;
  render_mode = 1;
  test_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  p = pipeline(argv[1], NULL);
  failed += run_case(p, "triangle capture while rendering", 0, 0, 0, 0, wrong);
  failed += run_case(p, "triangle rendering with capture disabled", 0, 0, 0, 4,
                     wrong);
  vkDestroyPipeline(dev, p, NULL);
  if (cache_write) {
    size_t size = 0;
    VK(vkGetPipelineCacheData(dev, pipeline_cache, &size, NULL));
    void *data = malloc(size);
    VK(vkGetPipelineCacheData(dev, pipeline_cache, &size, data));
    FILE *f = fopen(cache_write, "wb");
    if (!f || fwrite(data, 1, size, f) != size)
      return 2;
    fclose(f);
    free(data);
    printf("Pipeline cache saved: %zu bytes\n", size);
  }
  vkDestroyPipelineCache(dev, pipeline_cache, NULL);
  vkDestroyPipelineLayout(dev, layout, NULL);
  vkDestroyCommandPool(dev, pool, NULL);
  vkDestroyDevice(dev, NULL);
  vkDestroyInstance(inst, NULL);
  printf("XFB checks: %d/39 passed%s\n", 39 - failed,
         wrong ? " (deliberately wrong expectations)" : "");
  return failed ? 1 : 0;
}
