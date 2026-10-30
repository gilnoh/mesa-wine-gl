/* Explicit GS transform-feedback development probe. No GL version overrides.
 * Validates GPU output, byte counters, untouched guards, pause/resume and
 * bounds.
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
static PFN_vkCmdBindTransformFeedbackBuffersEXT bind_xfb;
static PFN_vkCmdBeginTransformFeedbackEXT begin_xfb;
static PFN_vkCmdEndTransformFeedbackEXT end_xfb;
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
  VkShaderModule vs = shader(dir, gs ? "capture.vert" : "vertex-only.vert");
  VkShaderModule geom = gs ? shader(dir, gs) : VK_NULL_HANDLE;
  VkPipelineShaderStageCreateInfo stages[] = {
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       .stage = VK_SHADER_STAGE_VERTEX_BIT,
       .module = vs,
       .pName = "main"},
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
       .stage = VK_SHADER_STAGE_GEOMETRY_BIT,
       .module = geom,
       .pName = "main"}};
  VkPipelineVertexInputStateCreateInfo vi = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
  VkPipelineInputAssemblyStateCreateInfo ia = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST};
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
      .rasterizerDiscardEnable = VK_TRUE,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .lineWidth = 1};
  VkPipelineMultisampleStateCreateInfo ms = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};
  VkPipelineColorBlendStateCreateInfo cb = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
  VkPipelineRenderingCreateInfo rendering = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
  VkGraphicsPipelineCreateInfo info = {
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .pNext = &rendering,
      .stageCount = gs ? 2 : 1,
      .pStages = stages,
      .pVertexInputState = &vi,
      .pInputAssemblyState = &ia,
      .pViewportState = &vp,
      .pRasterizationState = &rs,
      .pMultisampleState = &ms,
      .pColorBlendState = &cb,
      .layout = layout};
  VkPipeline p = VK_NULL_HANDLE;
  VkResult result =
      vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &info, NULL, &p);
  vkDestroyShaderModule(dev, vs, NULL);
  if (geom)
    vkDestroyShaderModule(dev, geom, NULL);
  if (!gs) {
    if (result != VK_ERROR_FEATURE_NOT_PRESENT) {
      fprintf(stderr,
              "Vertex-only scope control returned %d instead of explicit "
              "rejection\n",
              result);
      exit(2);
    }
    puts("Vertex-only capture: explicitly unsupported (scope control PASS)");
  } else {
    VK(result);
  }
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
  VkRenderingInfo info = {.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                          .renderArea = {{0, 0}, {8, 8}},
                          .layerCount = 1};
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
  struct buffer indices = {0};
  if (flags & 1) {
    indices = make_buffer(24, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    uint32_t values[] = {99, 3, 1, 2, 0, 99};
    memcpy(indices.map, values, sizeof(values));
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
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, p);
  VkDeviceSize offset = 32, size = overflow ? 32 : 256, coff = 4;
  bind_xfb(cmd, 0, 1, &output.handle, &offset, &size);
  rendering_begin(cmd);
  if (!(flags & 4))
    begin_xfb(cmd, 0, resume == 2 ? 1 : 0, resume == 2 ? &counter.handle : NULL,
              resume == 2 ? &coff : NULL);
  uint32_t first = resume ? 0 : 2, instance = resume ? 0 : 3;
  if (flags & 1) {
    vkCmdBindIndexBuffer(cmd, indices.handle, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, 4, instances, 1, 2, instance);
  } else
    vkCmdDraw(cmd, resume == 1 ? 2 : 4, instances, first, instance);
  if (!(flags & 4))
    end_xfb(cmd, 0, 1, &counter.handle, &coff);
  vkCmdEndRendering(cmd);
  if (resume == 1) {
    barrier(cmd);
    rendering_begin(cmd);
    begin_xfb(cmd, 0, 1, &counter.handle, &coff);
    vkCmdDraw(cmd, 2, 1, 2, 0);
    end_xfb(cmd, 0, 1, &counter.handle, &coff);
    vkCmdEndRendering(cmd);
  }
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
  for (unsigned ins = 0; ins < instances && !(flags & 4); ins++)
    for (unsigned i = 0; i < 4; i++)
      for (unsigned j = 0; j < (dynamic ? (i % 2 + 1) : 1); j++) {
        if ((initial + count + 1) * 16 > size)
          continue;
        float v[4] = {(float)(first + ((flags & 1) ? index_values[i] : i)) +
                          10.0f + (float)j * 100.0f,
                      (float)(instance + ins) + 20.0f, 30, 1};
        memcpy(expected + 32 + (initial + count) * 16, v, 16);
        count++;
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
  int failed = mismatches || counter_guard || bytes != expected_bytes;
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
  if (indices.handle)
    free_buffer(indices);
  return failed;
}
int main(int argc, char **argv) {
  setbuf(stdout, NULL);
  if (argc < 2) {
    fprintf(stderr, "usage: vk-xfb <shader-directory> [--wrong-expected]\n");
    return 2;
  }
  int wrong = argc > 2 && !strcmp(argv[2], "--wrong-expected");
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
  if (!bind_xfb || !begin_xfb || !end_xfb)
    return 2;
  VkCommandPoolCreateInfo pi = {.sType =
                                    VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                .queueFamilyIndex = family};
  VK(vkCreateCommandPool(dev, &pi, NULL, &pool));
  VkPipelineLayoutCreateInfo li = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  VK(vkCreatePipelineLayout(dev, &li, NULL, &layout));
  pipeline(argv[1], NULL);
  VkPipeline p = pipeline(argv[1], "static.geom");
  int failed = 0;
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
  vkDestroyPipelineLayout(dev, layout, NULL);
  vkDestroyCommandPool(dev, pool, NULL);
  vkDestroyDevice(dev, NULL);
  vkDestroyInstance(inst, NULL);
  printf("XFB checks: %d/11 passed%s\n", 11 - failed,
         wrong ? " (deliberately wrong expectations)" : "");
  return failed ? 1 : 0;
}
