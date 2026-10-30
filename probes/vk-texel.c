/* Independent uniform-texel-buffer format/range/update/readback probe. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>
#define VK(call)                                                               \
  do {                                                                         \
    VkResult r = (call);                                                       \
    if (r != VK_SUCCESS) {                                                     \
      fprintf(stderr, "%s: %d at %d\n", #call, r, __LINE__);                   \
      exit(2);                                                                 \
    }                                                                          \
  } while (0)
static VkDevice dev;
static VkPhysicalDevice gpu;
static VkQueue queue;
static VkCommandPool pool;
static VkPipelineLayout layout;
static VkDescriptorSetLayout descriptor_layout;
static VkPipelineCache cache;
static int cache_expect;
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
static VkPipeline pipeline(const char *dir, unsigned kind) {
  char name[32];
  snprintf(name, sizeof(name), "read-%u.comp", kind);
  VkShaderModule module = shader(dir, name);
  VkComputePipelineCreateInfo info = {
      .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .flags = cache_expect
                   ? VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT
                   : 0,
      .stage = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                .module = module,
                .pName = "main"},
      .layout = layout};
  VkPipeline p;
  VkResult result = vkCreateComputePipelines(dev, cache, 1, &info, NULL, &p);
  if (cache_expect < 0) {
    if (result != VK_PIPELINE_COMPILE_REQUIRED) {
      fprintf(stderr, "Expected incompatible-option cache miss, got %d\n",
              result);
      exit(2);
    }
    puts("Compiler-option cache isolation: PASS");
    info.flags = 0;
    VK(vkCreateComputePipelines(dev, cache, 1, &info, NULL, &p));
  } else {
    VK(result);
    if (cache_expect > 0)
      puts("Same-option cache reload: PASS");
  }
  vkDestroyShaderModule(dev, module, NULL);
  return p;
}
static uint32_t value(unsigned kind, unsigned word, unsigned round) {
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
static int run(VkPipeline p, VkFormat format, unsigned kind,
               unsigned components, unsigned offset, int wrong) {
  const unsigned n = offset ? 7 : 4;
  struct buffer input =
      make_buffer(1024, VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT);
  struct buffer output = make_buffer(512, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
  VkBufferViewCreateInfo vi = {.sType =
                                   VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,
                               .buffer = input.handle,
                               .format = format,
                               .offset = offset,
                               .range = n * components * 4};
  VkBufferView view;
  VK(vkCreateBufferView(dev, &vi, NULL, &view));
  VkDescriptorPoolSize sizes[] = {{VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1},
                                  {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}};
  VkDescriptorPoolCreateInfo dpi = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .maxSets = 1,
      .poolSizeCount = 2,
      .pPoolSizes = sizes};
  VkDescriptorPool dp;
  VK(vkCreateDescriptorPool(dev, &dpi, NULL, &dp));
  VkDescriptorSetAllocateInfo ai = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = dp,
      .descriptorSetCount = 1,
      .pSetLayouts = &descriptor_layout};
  VkDescriptorSet set;
  VK(vkAllocateDescriptorSets(dev, &ai, &set));
  VkDescriptorBufferInfo bi = {output.handle, 32, 80};
  VkWriteDescriptorSet writes[] = {
      {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
       .dstSet = set,
       .dstBinding = 0,
       .descriptorCount = 1,
       .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
       .pTexelBufferView = &view},
      {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
       .dstSet = set,
       .dstBinding = 1,
       .descriptorCount = 1,
       .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
       .pBufferInfo = &bi}};
  vkUpdateDescriptorSets(dev, 2, writes, 0, NULL);
  VkCommandBufferAllocateInfo ca = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1};
  VkCommandBuffer cmd;
  VK(vkAllocateCommandBuffers(dev, &ca, &cmd));
  VkCommandBufferBeginInfo cb = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  VK(vkBeginCommandBuffer(cmd, &cb));
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, p);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 1,
                          &set, 0, NULL);
  vkCmdDispatch(cmd, 5, 1, 1);
  VkMemoryBarrier mb = {.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
                        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                        .dstAccessMask = VK_ACCESS_HOST_READ_BIT};
  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &mb, 0, NULL, 0, NULL);
  VK(vkEndCommandBuffer(cmd));
  VkFenceCreateInfo fi = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
  VkFence fence;
  VK(vkCreateFence(dev, &fi, NULL, &fence));
  int failed = 0;
  for (unsigned round = 0; round < 2; round++) {
    uint32_t source[256];
    for (unsigned i = 0; i < 256; i++)
      source[i] = value(kind, i, round);
    memcpy(input.map, source, sizeof(source));
    memset(output.map, 0xCD, 512);
    VkSubmitInfo si = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                       .commandBufferCount = 1,
                       .pCommandBuffers = &cmd};
    VK(vkQueueSubmit(queue, 1, &si, fence));
    VK(vkWaitForFences(dev, 1, &fence, VK_TRUE, 10000000000ull));
    unsigned char expected[512];
    memset(expected, 0xCD, sizeof(expected));
    unsigned indices[] = {n - 1, 0, n / 2, 1};
    for (unsigned i = 0; i < 4; i++) {
      uint32_t v[4] = {0, 0, 0, kind == 0 ? 0x3f800000u : 1u};
      for (unsigned c = 0; c < components; c++)
        v[c] = source[offset / 4 + indices[i] * components + c];
      memcpy(expected + 32 + i * 16, v, 16);
    }
    uint32_t size[4] = {n, 123, 456, 789};
    memcpy(expected + 96, size, 16);
    if (wrong)
      expected[32] ^= 1;
    unsigned errors = 0;
    for (unsigned i = 0; i < 512; i++)
      errors += expected[i] != output.map[i];
    int mutated = memcmp(source, input.map, sizeof(source)) != 0;
    printf("format=%u components=%u kind=%u offset=%u elements=%u update=%u: "
           "%s mismatch_bytes=%u input_changed=%d\n",
           format, components, kind, offset, n, round,
           errors || mutated ? "FAIL" : "PASS", errors, mutated);
    failed += !!(errors || mutated);
    VK(vkResetFences(dev, 1, &fence));
  }
  vkDestroyFence(dev, fence, NULL);
  vkFreeCommandBuffers(dev, pool, 1, &cmd);
  vkDestroyDescriptorPool(dev, dp, NULL);
  vkDestroyBufferView(dev, view, NULL);
  free_buffer(input);
  free_buffer(output);
  return failed;
}
int main(int argc, char **argv) {
  setbuf(stdout, NULL);
  if (argc < 2)
    return 2;
  int rgb = 0, wrong = 0;
  const char *cache_read = NULL, *cache_write = NULL;
  for (int i = 2; i < argc; i++) {
    if (!strcmp(argv[i], "--rgb32"))
      rgb = 1;
    else if (!strcmp(argv[i], "--wrong-expected"))
      wrong = 1;
    else if (!strcmp(argv[i], "--cache-read") && i + 1 < argc)
      cache_read = argv[++i];
    else if (!strcmp(argv[i], "--cache-write") && i + 1 < argc)
      cache_write = argv[++i];
    else if (!strcmp(argv[i], "--expect-cache-miss"))
      cache_expect = -1;
    else if (!strcmp(argv[i], "--expect-cache-hit"))
      cache_expect = 1;
    else
      return 2;
  }
  VkApplicationInfo app = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                           .apiVersion = VK_API_VERSION_1_3};
  VkInstanceCreateInfo ici = {.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                              .pApplicationInfo = &app};
  VkInstance instance;
  VK(vkCreateInstance(&ici, NULL, &instance));
  uint32_t n = 1;
  VK(vkEnumeratePhysicalDevices(instance, &n, &gpu));
  if (!n)
    return 2;
  VkPhysicalDeviceProperties props;
  vkGetPhysicalDeviceProperties(gpu, &props);
  printf("Device: %s\n", props.deviceName);
  uint32_t count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(gpu, &count, NULL);
  VkQueueFamilyProperties *families = calloc(count, sizeof(*families));
  vkGetPhysicalDeviceQueueFamilyProperties(gpu, &count, families);
  uint32_t family = 0;
  while (family < count &&
         !(families[family].queueFlags & VK_QUEUE_COMPUTE_BIT))
    family++;
  free(families);
  if (family == count)
    return 2;
  float priority = 1;
  VkDeviceQueueCreateInfo qi = {.sType =
                                    VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                .queueFamilyIndex = family,
                                .queueCount = 1,
                                .pQueuePriorities = &priority};
  VkPhysicalDeviceVulkan13Features f13 = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
      .pipelineCreationCacheControl = VK_TRUE};
  VkDeviceCreateInfo di = {.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                           .pNext = &f13,
                           .queueCreateInfoCount = 1,
                           .pQueueCreateInfos = &qi};
  VK(vkCreateDevice(gpu, &di, NULL, &dev));
  vkGetDeviceQueue(dev, family, 0, &queue);
  VkCommandPoolCreateInfo pi = {.sType =
                                    VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                .queueFamilyIndex = family};
  VK(vkCreateCommandPool(dev, &pi, NULL, &pool));
  VkDescriptorSetLayoutBinding bindings[] = {
      {0, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1,
       VK_SHADER_STAGE_COMPUTE_BIT, NULL},
      {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT,
       NULL}};
  VkDescriptorSetLayoutCreateInfo dli = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 2,
      .pBindings = bindings};
  VK(vkCreateDescriptorSetLayout(dev, &dli, NULL, &descriptor_layout));
  VkPipelineLayoutCreateInfo li = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 1,
      .pSetLayouts = &descriptor_layout};
  VK(vkCreatePipelineLayout(dev, &li, NULL, &layout));
  const VkFormat formats[3][4] = {
      {VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32_SFLOAT,
       VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT},
      {VK_FORMAT_R32_SINT, VK_FORMAT_R32G32_SINT, VK_FORMAT_R32G32B32_SINT,
       VK_FORMAT_R32G32B32A32_SINT},
      {VK_FORMAT_R32_UINT, VK_FORMAT_R32G32_UINT, VK_FORMAT_R32G32B32_UINT,
       VK_FORMAT_R32G32B32A32_UINT}};
  VkPipelineCacheCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
  void *data = NULL;
  if (cache_read) {
    FILE *f = fopen(cache_read, "rb");
    if (!f)
      return 2;
    fseek(f, 0, SEEK_END);
    ci.initialDataSize = ftell(f);
    rewind(f);
    data = malloc(ci.initialDataSize);
    if (fread(data, 1, ci.initialDataSize, f) != ci.initialDataSize)
      return 2;
    fclose(f);
    ci.pInitialData = data;
  }
  VK(vkCreatePipelineCache(dev, &ci, NULL, &cache));
  free(data);
  int failed = 0, total = 0;
  for (unsigned kind = 0; kind < 3; kind++) {
    VkPipeline p = pipeline(argv[1], kind);
    for (unsigned c = 1; c <= 4; c++) {
      VkFormatProperties fp;
      vkGetPhysicalDeviceFormatProperties(gpu, formats[kind][c - 1], &fp);
      int supported =
          !!(fp.bufferFeatures & VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT);
      if (c == 3)
        printf("RGB32 kind=%u uniform support=%d\n", kind, supported);
      if (c == 3 && !rgb)
        continue;
      if (!supported) {
        fprintf(stderr, "Required format %u unsupported\n",
                formats[kind][c - 1]);
        return 77;
      }
      for (unsigned off = 0; off <= 48; off += 48) {
        failed += run(p, formats[kind][c - 1], kind, c, off, wrong);
        total += 2;
      }
    }
    vkDestroyPipeline(dev, p, NULL);
  }
  if (cache_write) {
    size_t size = 0;
    VK(vkGetPipelineCacheData(dev, cache, &size, NULL));
    void *data = malloc(size);
    VK(vkGetPipelineCacheData(dev, cache, &size, data));
    FILE *f = fopen(cache_write, "wb");
    if (!f || fwrite(data, 1, size, f) != size)
      return 2;
    fclose(f);
    free(data);
    printf("Pipeline cache saved: %zu bytes\n", size);
  }
  vkDestroyPipelineCache(dev, cache, NULL);
  vkDestroyPipelineLayout(dev, layout, NULL);
  vkDestroyDescriptorSetLayout(dev, descriptor_layout, NULL);
  vkDestroyCommandPool(dev, pool, NULL);
  vkDestroyDevice(dev, NULL);
  vkDestroyInstance(instance, NULL);
  printf("Texel checks: %d/%d passed\n", total - failed, total);
  return failed ? 1 : 0;
}
