#pragma once

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "nanovg.h"

enum NVGcreateFlags {
  // Flag indicating if geometry based anti-aliasing is used (may not be needed
  // when using MSAA).
  NVG_ANTIALIAS = 1 << 0,
  // Flag indicating if strokes should be drawn using stencil buffer. The
  // rendering will be a little
  // slower, but path overlaps (i.e. self-intersecting or sharp turns) will be
  // drawn just once.
  NVG_STENCIL_STROKES = 1 << 1,
  // Flag indicating that additional debug checks are done.
  NVG_DEBUG = 1 << 2,
};

typedef struct VkNvgExt {
  bool dynamicState; //Requires VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME
  bool colorBlendEquation; //Requires VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME
  bool colorWriteMask; //Requires VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME
} VkNvgExt;

typedef struct VKNVGCreateInfo {
  VkPhysicalDevice gpu;
  VkDevice device;
  VkRenderPass renderpass;
  VkCommandBuffer *cmdBuffer;
  uint32_t swapchainImageCount;
  uint32_t *currentFrame;
  const VkAllocationCallbacks *allocator; // Allocator for vulkan. can be null
  VkNvgExt ext;
} VKNVGCreateInfo;
#ifdef __cplusplus
extern "C" {
#endif
static void nvgDeleteVk(NVGcontext *ctx);

#ifdef __cplusplus
}
#endif

#ifdef NANOVG_VULKAN_IMPLEMENTATION

// optional define to switch to VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN
// by default used VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
// #define USE_TOPOLOGY_TRIANGLE_FAN

#include <assert.h>

#if !defined(__cplusplus) || defined(NANOVG_VK_NO_nullptrPTR)
#define nullptr NULL
#endif

#define NVGVK_CHECK_RESULT(f)                                                                                                                                                                                                                                                                                                  \
  {                                                                                                                                                                                                                                                                                                                            \
    VkResult res = (f);                                                                                                                                                                                                                                                                                                        \
    if (res != VK_SUCCESS) {                                                                                                                                                                                                                                                                                                   \
      assert(res == VK_SUCCESS);                                                                                                                                                                                                                                                                                               \
    }                                                                                                                                                                                                                                                                                                                          \
  }

enum VKNVGshaderType { NSVG_SHADER_FILLGRAD, NSVG_SHADER_FILLIMG, NSVG_SHADER_SIMPLE, NSVG_SHADER_IMG };

typedef struct VKNVGtexture {
  VkSampler sampler;

  VkImage image;
  VkImageLayout imageLayout;
  VkImageView view;

  VkDeviceMemory mem;
  void *mappedMem;
  VkDeviceSize rowPitch;
  bool mapped;
  int32_t width, height;
  int type; // enum NVGtexture
  int flags;
} VKNVGtexture;

enum VKNVGcallType {
  VKNVG_NONE = 0,
  VKNVG_FILL,
  VKNVG_CONVEXFILL,
  VKNVG_STROKE,
  VKNVG_TRIANGLES,
};

typedef struct VKNVGcall {
  int type;
  int image;
  int pathOffset;
  int pathCount;
  int triangleOffset;
  int triangleCount;
  int uniformOffset;
  NVGcompositeOperationState compositOperation;
} VKNVGcall;

typedef struct VKNVGpath {
  int fillOffset;
  int fillCount;
  int strokeOffset;
  int strokeCount;
} VKNVGpath;

typedef struct VKNVGfragUniforms {
  float scissorMat[12]; // matrices are actually 3 vec4s
  float paintMat[12];
  struct NVGcolor innerCol;
  struct NVGcolor outerCol;
  float scissorExt[2];
  float scissorScale[2];
  float extent[2];
  float radius;
  float feather;
  float strokeMult;
  float strokeThr;
  int texType;
  int type;
} VKNVGfragUniforms;

typedef struct VKNVGBuffer {
  VkBuffer buffer;
  VkDeviceMemory mem;
  VkDeviceSize size;
  void *mapped;
  bool initialised;
} VKNVGBuffer;

enum StencilSetting {
  VKNVG_STENCIL_STROKE_UNDEFINED = 0,
  VKNVG_STENCIL_STROKE_FILL = 1,
  VKNVG_STENCIL_STROKE_DRAW_AA,
  VKNVG_STENCIL_STROKE_CLEAR,
};
typedef struct VKNVGCreatePipelineKey {
  int stencilStroke;
  bool stencilFill;
  bool stencilTest;
  bool edgeAA;
  VkPrimitiveTopology topology;
  NVGcompositeOperationState compositOperation;
  VkColorComponentFlags colorWriteMask; // set and compare independently
} VKNVGCreatePipelineKey;

typedef struct VKNVGPipeline {
  VKNVGCreatePipelineKey create_key;
  VkPipeline pipeline;
} VKNVGPipeline;

typedef struct VkNvgVertexConstants {
  float viewSize[2];
  uint32_t uniformOffset;
} VkNvgVertexConstants;

typedef struct VKNVGcontext {
  VKNVGCreateInfo createInfo;

  VkPhysicalDeviceProperties gpuProperties;
  VkPhysicalDeviceMemoryProperties memoryProperties;

  int fragSize;
  int flags;

  // own resources
  VKNVGtexture *textures;
  int ntextures;
  int ctextures;

  VkDescriptorSetLayout *descLayout;
  VkPipelineLayout pipelineLayout;

  VKNVGPipeline *pipelines;
  int cpipelines;
  int npipelines;

  VkNvgVertexConstants vertexConstants;

  // Per frame buffers
  VKNVGcall *calls;
  int ccalls;
  uint32_t ncalls;
  VKNVGpath *paths;
  int cpaths;
  int npaths;
  struct NVGvertex *verts;
  int cverts;
  int nverts;

  VkDescriptorPool descPool;
  VkDescriptorSet *uniformDescriptorSet;
  VkDescriptorSet *uniformDescriptorSet2;
  VkDescriptorSet *ssboDescriptorSet;

  uint32_t cdescPool;

  unsigned char *uniforms;
  int cuniforms;
  int nuniforms;
  VKNVGBuffer *vertexBuffer;

  VKNVGBuffer *fragUniformBuffer;

  VKNVGPipeline *currentPipeline;

  VkShaderModule fillFragShader;
  VkShaderModule fillVertShader;
  VkQueue queue;

  VkNvgExt ext;
} VKNVGcontext;

static void vknvg_setDynamicState(VKNVGcontext *vk, VkCommandBuffer cmd, const VKNVGCreatePipelineKey *pipelineKey);

static int vknvg_maxi(int a, int b) { return a > b ? a : b; }

static void vknvg_xformToMat3x4(float *m3, float *t) {
  m3[0] = t[0];
  m3[1] = t[1];
  m3[2] = 0.0f;
  m3[3] = 0.0f;
  m3[4] = t[2];
  m3[5] = t[3];
  m3[6] = 0.0f;
  m3[7] = 0.0f;
  m3[8] = t[4];
  m3[9] = t[5];
  m3[10] = 1.0f;
  m3[11] = 0.0f;
}

static NVGcolor vknvg_premulColor(NVGcolor c) {
  c.r *= c.a;
  c.g *= c.a;
  c.b *= c.a;
  return c;
}

static VKNVGtexture *vknvg_findTexture(VKNVGcontext *vk, int id) {
  if (id > vk->ntextures || id <= 0) {
    return nullptr;
  }
  VKNVGtexture *tex = vk->textures + id - 1;
  return tex;
}
static VKNVGtexture *vknvg_allocTexture(VKNVGcontext *vk) {
  VKNVGtexture *tex = nullptr;
  int i;

  for (i = 0; i < vk->ntextures; i++) {
    if (vk->textures[i].image == VK_NULL_HANDLE) {
      tex = &vk->textures[i];
      break;
    }
  }
  if (tex == nullptr) {
    if (vk->ntextures + 1 > vk->ctextures) {
      VKNVGtexture *textures;
      int ctextures = vknvg_maxi(vk->ntextures + 1, 4) + vk->ctextures / 2; // 1.5x Overallocate
      textures = (VKNVGtexture *) realloc(vk->textures, sizeof(VKNVGtexture) * ctextures);
      if (textures == nullptr) {
        return nullptr;
      }
      vk->textures = textures;
      vk->ctextures = ctextures;
    }
    tex = &vk->textures[vk->ntextures++];
  }
  memset(tex, 0, sizeof(*tex));
  return tex;
}
static int vknvg_textureId(VKNVGcontext *vk, VKNVGtexture *tex) {
  ptrdiff_t id = tex - vk->textures;
  if (id < 0 || id > vk->ntextures) {
    return 0;
  }
  return (int) id + 1;
}
static int vknvg_deleteTexture(VKNVGcontext *vk, VKNVGtexture *tex) {
  VkDevice device = vk->createInfo.device;
  const VkAllocationCallbacks *allocator = vk->createInfo.allocator;
  if (tex) {
    if (tex->view != VK_NULL_HANDLE) {
      vkDestroyImageView(device, tex->view, allocator);
      tex->view = VK_NULL_HANDLE;
    }
    if (tex->sampler != VK_NULL_HANDLE) {
      vkDestroySampler(device, tex->sampler, allocator);
      tex->sampler = VK_NULL_HANDLE;
    }
    if (tex->image != VK_NULL_HANDLE) {
      vkDestroyImage(device, tex->image, allocator);
      tex->image = VK_NULL_HANDLE;
    }
    if (tex->mem != VK_NULL_HANDLE) {
      vkFreeMemory(device, tex->mem, allocator);
      tex->mem = VK_NULL_HANDLE;
    }
    return 1;
  }
  return 0;
}

#ifdef __cplusplus
extern "C" {
#endif

extern PFN_vkCmdSetColorBlendEquationEXT cmdSetColorBlendEquation;
extern PFN_vkCmdSetPrimitiveTopologyEXT cmdSetPrimitiveTopology;
extern PFN_vkCmdSetStencilTestEnableEXT cmdSetStencilTestEnable;
extern PFN_vkCmdSetStencilOpEXT cmdSetStencilOp;
extern PFN_vkCmdSetColorWriteMaskEXT cmdSetColorWriteMask;

#ifdef __cplusplus
}
#endif

#define vkCmdSetColorBlendEquationEXT cmdSetColorBlendEquation
#define vkCmdSetPrimitiveTopologyEXT cmdSetPrimitiveTopology
#define vkCmdSetStencilTestEnableEXT cmdSetStencilTestEnable
#define vkCmdSetStencilOpEXT cmdSetStencilOp
#define vkCmdSetColorWriteMaskEXT cmdSetColorWriteMask


#ifdef __cplusplus
inline PFN_vkCmdSetColorBlendEquationEXT cmdSetColorBlendEquation = nullptr;
inline PFN_vkCmdSetPrimitiveTopologyEXT cmdSetPrimitiveTopology = nullptr;
inline PFN_vkCmdSetStencilTestEnableEXT cmdSetStencilTestEnable = nullptr;
inline PFN_vkCmdSetStencilOpEXT cmdSetStencilOp = nullptr;
inline PFN_vkCmdSetColorWriteMaskEXT cmdSetColorWriteMask = nullptr;
#else
PFN_vkCmdSetColorBlendEquationEXT cmdSetColorBlendEquation = NULL;
PFN_vkCmdSetPrimitiveTopologyEXT cmdSetPrimitiveTopology = NULL;
PFN_vkCmdSetStencilTestEnableEXT cmdSetStencilTestEnable = NULL;
PFN_vkCmdSetStencilOpEXT cmdSetStencilOp = NULL;
PFN_vkCmdSetColorWriteMaskEXT cmdSetColorWriteMask = NULL;
#endif

static VKNVGPipeline *vknvg_allocPipeline(VKNVGcontext *vk) {
  VKNVGPipeline *ret = nullptr;
  if (vk->npipelines + 1 > vk->cpipelines) {
    VKNVGPipeline *pipelines;
    int cpipelines = vknvg_maxi(vk->npipelines + 1, 128) + vk->cpipelines / 2; // 1.5x Overallocate
    pipelines = (VKNVGPipeline *) realloc(vk->pipelines, sizeof(VKNVGPipeline) * cpipelines);
    if (pipelines == nullptr)
      return nullptr;
    vk->pipelines = pipelines;
    vk->cpipelines = cpipelines;
  }
  ret = &vk->pipelines[vk->npipelines++];
  memset(ret, 0, sizeof(VKNVGPipeline));
  return ret;
}
static int vknvg_compareCreatePipelineKey(VKNVGcontext *vk, const VKNVGCreatePipelineKey *a, const VKNVGCreatePipelineKey *b) {
  if (!vk->ext.dynamicState) {
    if (a->topology != b->topology) {
      return a->topology - b->topology;
    }
    if (a->stencilTest != b->stencilTest) {
      return a->stencilTest - b->stencilTest;
    }
    if (a->stencilFill != b->stencilFill) {
      return a->stencilFill - b->stencilFill;
    }
    if (a->stencilStroke != b->stencilStroke) {
      return a->stencilStroke - b->stencilStroke;
    }
  }

  if (!vk->ext.colorWriteMask) {
    if (a->colorWriteMask != b->colorWriteMask) {
      return a->colorWriteMask - b->colorWriteMask;
    }
  }

  if (!vk->ext.colorBlendEquation) {
    if (a->edgeAA != b->edgeAA) {
      return a->edgeAA - b->edgeAA;
    }
    if (a->compositOperation.srcRGB != b->compositOperation.srcRGB) {
      return a->compositOperation.srcRGB - b->compositOperation.srcRGB;
    }
    if (a->compositOperation.srcAlpha != b->compositOperation.srcAlpha) {
      return a->compositOperation.srcAlpha - b->compositOperation.srcAlpha;
    }
    if (a->compositOperation.dstRGB != b->compositOperation.dstRGB) {
      return a->compositOperation.dstRGB - b->compositOperation.dstRGB;
    }
    if (a->compositOperation.dstAlpha != b->compositOperation.dstAlpha) {
      return a->compositOperation.dstAlpha - b->compositOperation.dstAlpha;
    }
  }
  return 0;
}

static VKNVGPipeline *vknvg_findPipeline(VKNVGcontext *vk, VKNVGCreatePipelineKey *pipelinekey) {
  VKNVGPipeline *pipeline = nullptr;
  for (int i = 0; i < vk->npipelines; i++) {
    if (vknvg_compareCreatePipelineKey(vk, &vk->pipelines[i].create_key, pipelinekey) == 0) {
      pipeline = &vk->pipelines[i];
      break;
    }
  }
  return pipeline;
}

static VkResult vknvg_memory_type_from_properties(VkPhysicalDeviceMemoryProperties memoryProperties, uint32_t typeBits, VkFlags requirements_mask, uint32_t *typeIndex) {
  // Search memtypes to find first index with those properties
  for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++) {
    if ((typeBits & 1) == 1) {
      // Type is available, does it match user properties?
      if ((memoryProperties.memoryTypes[i].propertyFlags & requirements_mask) == requirements_mask) {
        *typeIndex = i;
        return VK_SUCCESS;
      }
    }
    typeBits >>= 1;
  }
  // No memory types matched, return failure
  return VK_ERROR_FORMAT_NOT_SUPPORTED;
}

static int vknvg_convertPaint(VKNVGcontext *vk, VKNVGfragUniforms *frag, NVGpaint *paint, NVGscissor *scissor, float width, float fringe, float strokeThr) {
  VKNVGtexture *tex = nullptr;
  float invxform[6];

  memset(frag, 0, sizeof(*frag));

  frag->innerCol = vknvg_premulColor(paint->innerColor);
  frag->outerCol = vknvg_premulColor(paint->outerColor);

  if (scissor->extent[0] < -0.5f || scissor->extent[1] < -0.5f) {
    memset(frag->scissorMat, 0, sizeof(frag->scissorMat));
    frag->scissorExt[0] = 1.0f;
    frag->scissorExt[1] = 1.0f;
    frag->scissorScale[0] = 1.0f;
    frag->scissorScale[1] = 1.0f;
  } else {
    nvgTransformInverse(invxform, scissor->xform);
    vknvg_xformToMat3x4(frag->scissorMat, invxform);
    frag->scissorExt[0] = scissor->extent[0];
    frag->scissorExt[1] = scissor->extent[1];
    frag->scissorScale[0] = sqrtf(scissor->xform[0] * scissor->xform[0] + scissor->xform[2] * scissor->xform[2]) / fringe;
    frag->scissorScale[1] = sqrtf(scissor->xform[1] * scissor->xform[1] + scissor->xform[3] * scissor->xform[3]) / fringe;
  }

  memcpy(frag->extent, paint->extent, sizeof(frag->extent));
  frag->strokeMult = (width * 0.5f + fringe * 0.5f) / fringe;
  frag->strokeThr = strokeThr;

  if (paint->image != 0) {
    tex = vknvg_findTexture(vk, paint->image);
    if (tex == nullptr)
      return 0;
    if ((tex->flags & NVG_IMAGE_FLIPY) != 0) {
      float m1[6], m2[6];
      nvgTransformTranslate(m1, 0.0f, frag->extent[1] * 0.5f);
      nvgTransformMultiply(m1, paint->xform);
      nvgTransformScale(m2, 1.0f, -1.0f);
      nvgTransformMultiply(m2, m1);
      nvgTransformTranslate(m1, 0.0f, -frag->extent[1] * 0.5f);
      nvgTransformMultiply(m1, m2);
      nvgTransformInverse(invxform, m1);
    } else {
      nvgTransformInverse(invxform, paint->xform);
    }
    frag->type = NSVG_SHADER_FILLIMG;

    if (tex->type == NVG_TEXTURE_RGBA)
      frag->texType = (tex->flags & NVG_IMAGE_PREMULTIPLIED) ? 0 : 1;
    else
      frag->texType = 2;
    //    printf("frag->texType = %d\n", frag->texType);
  } else {
    frag->type = NSVG_SHADER_FILLGRAD;
    frag->radius = paint->radius;
    frag->feather = paint->feather;
    nvgTransformInverse(invxform, paint->xform);
  }

  vknvg_xformToMat3x4(frag->paintMat, invxform);

  return 1;
}

static VKNVGBuffer vknvg_createBuffer(VkDevice device, VkPhysicalDeviceMemoryProperties memoryProperties, const VkAllocationCallbacks *allocator, VkBufferUsageFlags usage, VkFlags memory_type, void *data, uint32_t size) {

  const VkBufferCreateInfo buf_createInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr, 0, size, usage};

  VkBuffer buffer;
  NVGVK_CHECK_RESULT(vkCreateBuffer(device, &buf_createInfo, allocator, &buffer));
#ifdef __cplusplus
  VkMemoryRequirements mem_reqs = {};
#else
  VkMemoryRequirements mem_reqs = {0};
#endif
  vkGetBufferMemoryRequirements(device, buffer, &mem_reqs);

  uint32_t memoryTypeIndex;
  VkResult res = vknvg_memory_type_from_properties(memoryProperties, mem_reqs.memoryTypeBits, memory_type, &memoryTypeIndex);
  assert(res == VK_SUCCESS);
  VkMemoryAllocateInfo mem_alloc = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, mem_reqs.size, memoryTypeIndex};

  VkDeviceMemory mem;
  NVGVK_CHECK_RESULT(vkAllocateMemory(device, &mem_alloc, nullptr, &mem));

  void *mapped;
  NVGVK_CHECK_RESULT(vkMapMemory(device, mem, 0, mem_alloc.allocationSize, 0, &mapped));
  memcpy(mapped, data, size);
  NVGVK_CHECK_RESULT(vkBindBufferMemory(device, buffer, mem, 0));
  VKNVGBuffer buf = {buffer, mem, mem_alloc.allocationSize, mapped, true};
  return buf;
}

static void vknvg_destroyBuffer(VkDevice device, const VkAllocationCallbacks *allocator, VKNVGBuffer *buffer) {
  if (buffer->initialised) {
    vkUnmapMemory(device, buffer->mem);
  }
  vkDestroyBuffer(device, buffer->buffer, allocator);
  vkFreeMemory(device, buffer->mem, allocator);
}

static void vknvg_UpdateBuffer(VkDevice device, const VkAllocationCallbacks *allocator, VKNVGBuffer *buffer, VkPhysicalDeviceMemoryProperties memoryProperties, VkBufferUsageFlags usage, VkFlags memory_type, void *data, uint32_t size) {
  if (buffer->size < size) {
    vknvg_destroyBuffer(device, allocator, buffer);
    *buffer = vknvg_createBuffer(device, memoryProperties, allocator, usage, memory_type, data, size);
  } else {
    memcpy(buffer->mapped, data, size);
  }
}

static VkShaderModule vknvg_createShaderModule(VkDevice device, const void *code, size_t size, const VkAllocationCallbacks *allocator) {

  VkShaderModuleCreateInfo moduleCreateInfo = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, nullptr, 0, size, (const uint32_t *) code};
  VkShaderModule module;
  NVGVK_CHECK_RESULT(vkCreateShaderModule(device, &moduleCreateInfo, allocator, &module));
  return module;
}
static VkBlendFactor vknvg_NVGblendFactorToVkBlendFactor(enum NVGblendFactor factor) {
  switch (factor) {
    case NVG_ZERO:
      return VK_BLEND_FACTOR_ZERO;
    case NVG_ONE:
      return VK_BLEND_FACTOR_ONE;
    case NVG_SRC_COLOR:
      return VK_BLEND_FACTOR_SRC_COLOR;
    case NVG_ONE_MINUS_SRC_COLOR:
      return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    case NVG_DST_COLOR:
      return VK_BLEND_FACTOR_DST_COLOR;
    case NVG_ONE_MINUS_DST_COLOR:
      return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
    case NVG_SRC_ALPHA:
      return VK_BLEND_FACTOR_SRC_ALPHA;
    case NVG_ONE_MINUS_SRC_ALPHA:
      return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case NVG_DST_ALPHA:
      return VK_BLEND_FACTOR_DST_ALPHA;
    case NVG_ONE_MINUS_DST_ALPHA:
      return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    case NVG_SRC_ALPHA_SATURATE:
      return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
    default:
      return VK_BLEND_FACTOR_MAX_ENUM;
  }
}

static VkFlags vknvg_colorWriteMask(const VKNVGCreatePipelineKey *pipeline_key) {
  if (pipeline_key->stencilStroke == VKNVG_STENCIL_STROKE_CLEAR) {
    return 0;
  }

  if (pipeline_key->stencilFill) {
    return 0;
  }
  return 0xf;
}

static VkPipelineColorBlendAttachmentState vknvg_compositOperationToColorBlendAttachmentState(const VKNVGCreatePipelineKey *pipelineKey) {
#ifdef __cplusplus
  VkPipelineColorBlendAttachmentState state = {};
#else
  VkPipelineColorBlendAttachmentState state = {0};
#endif
  state.blendEnable = VK_TRUE;
  state.colorBlendOp = VK_BLEND_OP_ADD;
  state.alphaBlendOp = VK_BLEND_OP_ADD;
  state.colorWriteMask = vknvg_colorWriteMask(pipelineKey);

  state.srcColorBlendFactor = vknvg_NVGblendFactorToVkBlendFactor(pipelineKey->compositOperation.srcRGB);
  state.srcAlphaBlendFactor = vknvg_NVGblendFactorToVkBlendFactor(pipelineKey->compositOperation.srcAlpha);
  state.dstColorBlendFactor = vknvg_NVGblendFactorToVkBlendFactor(pipelineKey->compositOperation.dstRGB);
  state.dstAlphaBlendFactor = vknvg_NVGblendFactorToVkBlendFactor(pipelineKey->compositOperation.dstAlpha);

  if (state.srcColorBlendFactor == VK_BLEND_FACTOR_MAX_ENUM || state.srcAlphaBlendFactor == VK_BLEND_FACTOR_MAX_ENUM || state.dstColorBlendFactor == VK_BLEND_FACTOR_MAX_ENUM || state.dstAlphaBlendFactor == VK_BLEND_FACTOR_MAX_ENUM) {
    // default blend if failed convert
    state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  }
  return state;
}

static void vknvg_createDescriptorSetLayout(VkDevice device, const VkAllocationCallbacks *allocator, VkDescriptorSetLayout *layouts) {
  const VkDescriptorSetLayoutBinding binding_0 = {
          0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr,
  };
  const VkDescriptorSetLayoutCreateInfo create_info_0 = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0, 1, &binding_0};
  NVGVK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &create_info_0, allocator, &layouts[0]));

  const VkDescriptorSetLayoutBinding binding_1 = {
          1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr,
  };
  const VkDescriptorSetLayoutCreateInfo create_info_1 = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0, 1, &binding_1};
  NVGVK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &create_info_1, allocator, &layouts[1]));
}

static VkDescriptorPool vknvg_createDescriptorPool(VkDevice device, uint32_t count, const VkAllocationCallbacks *allocator) {

  const VkDescriptorPoolSize type_count[3] = {
          {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 4 * count},
          {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4 * count},
          {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 * count},
  };
  const VkDescriptorPoolCreateInfo descriptor_pool = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, nullptr, 0, count * 2, 3, type_count};
  VkDescriptorPool descPool;
  NVGVK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptor_pool, allocator, &descPool));
  return descPool;
}
static VkPipelineLayout vknvg_createPipelineLayout(VKNVGcontext *vk, const VkAllocationCallbacks *allocator) {
#ifdef __cplusplus
  VkPushConstantRange pushConstantRange = {};
#else
  VkPushConstantRange pushConstantRange = {0};
#endif
  pushConstantRange.offset = 0;
  pushConstantRange.size = sizeof(VkNvgVertexConstants);
  pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

  VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  pipelineLayoutCreateInfo.setLayoutCount = 2;
  pipelineLayoutCreateInfo.pSetLayouts = vk->descLayout;
  pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
  pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;

  VkPipelineLayout pipelineLayout;

  NVGVK_CHECK_RESULT(vkCreatePipelineLayout(vk->createInfo.device, &pipelineLayoutCreateInfo, allocator, &pipelineLayout));

  return pipelineLayout;
}

static VkPipelineDepthStencilStateCreateInfo initializeDepthStencilCreateInfo(const VKNVGCreatePipelineKey *pipelinekey) {

  VkPipelineDepthStencilStateCreateInfo ds = {VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
  ds.depthWriteEnable = VK_FALSE;
  ds.depthTestEnable = VK_TRUE;
  ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
  ds.depthBoundsTestEnable = VK_FALSE;

  if (pipelinekey->stencilStroke) { // enables
    ds.stencilTestEnable = VK_TRUE;
    ds.front.failOp = VK_STENCIL_OP_KEEP;
    ds.front.depthFailOp = VK_STENCIL_OP_KEEP;
    ds.front.passOp = VK_STENCIL_OP_KEEP;
    ds.front.compareOp = VK_COMPARE_OP_EQUAL;
    ds.front.reference = 0x00;
    ds.front.compareMask = 0xff;
    ds.front.writeMask = 0xff;
    ds.back = ds.front;
    ds.back.passOp = VK_STENCIL_OP_DECREMENT_AND_CLAMP;

    switch (pipelinekey->stencilStroke) {
      case VKNVG_STENCIL_STROKE_FILL:
        ds.front.passOp = VK_STENCIL_OP_INCREMENT_AND_CLAMP;
        ds.back.passOp = VK_STENCIL_OP_DECREMENT_AND_CLAMP;
        break;
      case VKNVG_STENCIL_STROKE_DRAW_AA:
        ds.front.passOp = VK_STENCIL_OP_KEEP;
        ds.back.passOp = VK_STENCIL_OP_KEEP;
        break;
      case VKNVG_STENCIL_STROKE_CLEAR:
        ds.front.failOp = VK_STENCIL_OP_ZERO;
        ds.front.depthFailOp = VK_STENCIL_OP_ZERO;
        ds.front.passOp = VK_STENCIL_OP_ZERO;
        ds.front.compareOp = VK_COMPARE_OP_ALWAYS;
        ds.back = ds.front;
        break;
    }
    return ds;
  }

  ds.stencilTestEnable = VK_FALSE;
  ds.back.failOp = VK_STENCIL_OP_KEEP;
  ds.back.passOp = VK_STENCIL_OP_KEEP;
  ds.back.compareOp = VK_COMPARE_OP_ALWAYS;

  if (pipelinekey->stencilFill) {
    ds.stencilTestEnable = VK_TRUE;
    ds.front.compareOp = VK_COMPARE_OP_ALWAYS;
    ds.front.failOp = VK_STENCIL_OP_KEEP;
    ds.front.depthFailOp = VK_STENCIL_OP_KEEP;
    ds.front.passOp = VK_STENCIL_OP_INCREMENT_AND_WRAP;
    ds.front.reference = 0x0;
    ds.front.compareMask = 0xff;
    ds.front.writeMask = 0xff;
    ds.back = ds.front;
    ds.back.passOp = VK_STENCIL_OP_DECREMENT_AND_WRAP;
  } else if (pipelinekey->stencilTest) {
    ds.stencilTestEnable = VK_TRUE;
    if (pipelinekey->edgeAA) {
      ds.front.compareOp = VK_COMPARE_OP_EQUAL;
      ds.front.reference = 0x0;
      ds.front.compareMask = 0xff;
      ds.front.writeMask = 0xff;
      ds.front.failOp = VK_STENCIL_OP_KEEP;
      ds.front.depthFailOp = VK_STENCIL_OP_KEEP;
      ds.front.passOp = VK_STENCIL_OP_KEEP;
      ds.back = ds.front;
    } else {
      ds.front.compareOp = VK_COMPARE_OP_NOT_EQUAL;
      ds.front.reference = 0x0;
      ds.front.compareMask = 0xff;
      ds.front.writeMask = 0xff;
      ds.front.failOp = VK_STENCIL_OP_ZERO;
      ds.front.depthFailOp = VK_STENCIL_OP_ZERO;
      ds.front.passOp = VK_STENCIL_OP_ZERO;
      ds.back = ds.front;
    }
  }

  return ds;
}

static VkFlags vknvg_cullMode(VKNVGCreatePipelineKey *pipelinekey) {
  if (pipelinekey->stencilFill) {
    return VK_CULL_MODE_NONE;
  }
  return VK_CULL_MODE_BACK_BIT;
}

static VKNVGPipeline *vknvg_createPipeline(VKNVGcontext *vk, VKNVGCreatePipelineKey *pipelinekey) {
  VkDevice device = vk->createInfo.device;
  VkPipelineLayout pipelineLayout = vk->pipelineLayout;
  VkRenderPass renderpass = vk->createInfo.renderpass;
  const VkAllocationCallbacks *allocator = vk->createInfo.allocator;

  VkShaderModule vert_shader = vk->fillVertShader;
  VkShaderModule frag_shader = vk->fillFragShader;

#ifdef __cplusplus
  VkVertexInputBindingDescription vi_bindings[1] = {{}};
#else
  VkVertexInputBindingDescription vi_bindings[1] = {{0}};
#endif
  vi_bindings[0].binding = 0;
  vi_bindings[0].stride = sizeof(NVGvertex);
  vi_bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  VkVertexInputAttributeDescription vi_attrs[2] = {
#ifdef __cplusplus
          {},
#else
          {0},
#endif
  };
  vi_attrs[0].binding = 0;
  vi_attrs[0].location = 0;
  vi_attrs[0].format = VK_FORMAT_R32G32_SFLOAT;
  vi_attrs[0].offset = 0;
  vi_attrs[1].binding = 0;
  vi_attrs[1].location = 1;
  vi_attrs[1].format = VK_FORMAT_R32G32_SFLOAT;
  vi_attrs[1].offset = (2 * sizeof(float));

  VkPipelineVertexInputStateCreateInfo vi = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
  vi.vertexBindingDescriptionCount = 1;
  vi.pVertexBindingDescriptions = vi_bindings;
  vi.vertexAttributeDescriptionCount = 2;
  vi.pVertexAttributeDescriptions = vi_attrs;

  VkPipelineInputAssemblyStateCreateInfo ia = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
  ia.topology = pipelinekey->topology;

  VkPipelineRasterizationStateCreateInfo rs = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
  rs.polygonMode = VK_POLYGON_MODE_FILL;
  rs.cullMode = vknvg_cullMode(pipelinekey);
  rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rs.depthClampEnable = VK_FALSE;
  rs.rasterizerDiscardEnable = VK_FALSE;
  rs.depthBiasEnable = VK_FALSE;
  rs.lineWidth = 1.0f;

  VkPipelineColorBlendAttachmentState colorblend = vknvg_compositOperationToColorBlendAttachmentState(pipelinekey);
  pipelinekey->colorWriteMask = vknvg_colorWriteMask(pipelinekey);

  VkPipelineColorBlendStateCreateInfo cb = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
  cb.attachmentCount = 1;
  cb.pAttachments = &colorblend;

  VkPipelineViewportStateCreateInfo vp = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
  vp.viewportCount = 1;
  vp.scissorCount = 1;

#ifdef __cplusplus
  auto dynamicStateEnables = static_cast<VkDynamicState *>(calloc(16, sizeof(VkDynamicState)));
#else
  VkDynamicState *dynamicStateEnables = calloc(16, sizeof(VkDynamicState));
#endif

  uint32_t NUM_DYNAMIC_STATES = 0;
  dynamicStateEnables[NUM_DYNAMIC_STATES++] = VK_DYNAMIC_STATE_VIEWPORT;
  dynamicStateEnables[NUM_DYNAMIC_STATES++] = VK_DYNAMIC_STATE_SCISSOR;
  if (vk->createInfo.ext.dynamicState) {
    vk->ext.dynamicState = true;
    dynamicStateEnables[NUM_DYNAMIC_STATES++] = VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY;
    dynamicStateEnables[NUM_DYNAMIC_STATES++] = VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE;
    dynamicStateEnables[NUM_DYNAMIC_STATES++] = VK_DYNAMIC_STATE_STENCIL_OP;
  }
  if (vk->createInfo.ext.colorBlendEquation) {
    vk->ext.colorBlendEquation = true;
    dynamicStateEnables[NUM_DYNAMIC_STATES++] = VK_DYNAMIC_STATE_COLOR_BLEND_EQUATION_EXT;
  }
  if (vk->createInfo.ext.colorWriteMask) {
    vk->ext.colorWriteMask = true;
    dynamicStateEnables[NUM_DYNAMIC_STATES++] = VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT;
  }

  VkPipelineDynamicStateCreateInfo dynamicState = {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
  dynamicState.dynamicStateCount = NUM_DYNAMIC_STATES;
  dynamicState.pDynamicStates = dynamicStateEnables;

  VkPipelineDepthStencilStateCreateInfo ds = initializeDepthStencilCreateInfo(pipelinekey);

  VkPipelineMultisampleStateCreateInfo ms = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
  ms.pSampleMask = nullptr;
  ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineShaderStageCreateInfo shaderStages[2] = {{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}, {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}};
  shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  shaderStages[0].module = vert_shader;
  shaderStages[0].pName = "main";

  uint32_t edgeAA = vk->flags & NVG_ANTIALIAS ? 1 : 0;

#ifdef __cplusplus
  VkSpecializationMapEntry entry = {};
#else
  VkSpecializationMapEntry entry = {0};
#endif
  entry.offset = 0;
  entry.constantID = 0;
  entry.size = sizeof(edgeAA);

#ifdef __cplusplus
  VkSpecializationInfo specializationInfo = {};
#else
  VkSpecializationInfo specializationInfo = {0};
#endif
  specializationInfo.mapEntryCount = 1;
  specializationInfo.pMapEntries = &entry;
  specializationInfo.dataSize = entry.size;
  specializationInfo.pData = &edgeAA;

  shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  shaderStages[1].module = frag_shader;
  shaderStages[1].pName = "main";
  shaderStages[1].pSpecializationInfo = &specializationInfo;

  VkGraphicsPipelineCreateInfo pipelineCreateInfo = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
  pipelineCreateInfo.layout = pipelineLayout;
  pipelineCreateInfo.stageCount = 2;
  pipelineCreateInfo.pStages = shaderStages;
  pipelineCreateInfo.pVertexInputState = &vi;
  pipelineCreateInfo.pInputAssemblyState = &ia;
  pipelineCreateInfo.pRasterizationState = &rs;
  pipelineCreateInfo.pColorBlendState = &cb;
  pipelineCreateInfo.pMultisampleState = &ms;
  pipelineCreateInfo.pViewportState = &vp;
  pipelineCreateInfo.pDepthStencilState = &ds;
  pipelineCreateInfo.renderPass = renderpass;
  pipelineCreateInfo.pDynamicState = &dynamicState;

  VkPipeline pipeline;
  NVGVK_CHECK_RESULT(vkCreateGraphicsPipelines(device, 0, 1, &pipelineCreateInfo, allocator, &pipeline));

  free(dynamicStateEnables);

  VKNVGPipeline *ret = vknvg_allocPipeline(vk);

  ret->create_key = *pipelinekey;
  ret->pipeline = pipeline;
  return ret;
}

static VkPipeline vknvg_bindPipeline(VKNVGcontext *vk, VkCommandBuffer cmdBuffer, VKNVGCreatePipelineKey *pipelinekey) {
  pipelinekey->colorWriteMask = vknvg_colorWriteMask(pipelinekey); // always set this before compare op
  VKNVGPipeline *pipeline = vknvg_findPipeline(vk, pipelinekey);
  if (!pipeline) {
    pipeline = vknvg_createPipeline(vk, pipelinekey);
  }
  if (pipeline != vk->currentPipeline) {
    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);
    vk->currentPipeline = pipeline;
  }
  return pipeline->pipeline;
}

static int vknvg_UpdateTexture(VkDevice device, VKNVGtexture *tex, int dx, int dy, int w, int h, const unsigned char *data) {
  if (!tex->mapped) {
    VkMemoryRequirements mem_reqs;
    vkGetImageMemoryRequirements(device, tex->image, &mem_reqs);
    NVGVK_CHECK_RESULT(vkMapMemory(device, tex->mem, 0, mem_reqs.size, 0, &tex->mappedMem));
    tex->mapped = true;

    VkImageSubresource subres = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0};
    VkSubresourceLayout layout;
    vkGetImageSubresourceLayout(device, tex->image, &subres, &layout);
    tex->rowPitch = layout.rowPitch;
  }
  int comp_size = (tex->type == NVG_TEXTURE_RGBA) ? 4 : 1;
  for (int y = 0; y < h; ++y) {
    char *src = (char *) data + ((dy + y) * (tex->width * comp_size)) + dx;
    char *dest = (char *) tex->mappedMem + ((dy + y) * tex->rowPitch) + dx;
    memcpy(dest, src, w * comp_size);
  }
  return 1;
}

// call it after vknvg_UpdateTexture
static void vknvg_InitTexture(VkCommandBuffer cmdbuffer, VkQueue queue, VKNVGtexture *tex) {
#ifdef __cplusplus
  VkCommandBufferBeginInfo beginInfo = {};
#else
  VkCommandBufferBeginInfo beginInfo = {0};
#endif
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(cmdbuffer, &beginInfo);

#ifdef __cplusplus
  VkImageMemoryBarrier layoutTransitionBarrier = {};
#else
  VkImageMemoryBarrier layoutTransitionBarrier = {0};
#endif
  layoutTransitionBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  layoutTransitionBarrier.srcAccessMask = 0;
  layoutTransitionBarrier.dstAccessMask = 0;
  layoutTransitionBarrier.oldLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
  layoutTransitionBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  layoutTransitionBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  layoutTransitionBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  layoutTransitionBarrier.image = tex->image;
  VkImageSubresourceRange resourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  layoutTransitionBarrier.subresourceRange = resourceRange;

  vkCmdPipelineBarrier(cmdbuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &layoutTransitionBarrier);

  vkEndCommandBuffer(cmdbuffer);

  VkPipelineStageFlags waitStageMash[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
#ifdef __cplusplus
  VkSubmitInfo submitInfo = {};
#else
  VkSubmitInfo submitInfo = {0};
#endif
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.waitSemaphoreCount = 0;
  submitInfo.pWaitSemaphores = NULL;
  submitInfo.pWaitDstStageMask = waitStageMash;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &cmdbuffer;
  submitInfo.signalSemaphoreCount = 0;
  submitInfo.pSignalSemaphores = NULL;
  vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(queue);
  vkResetCommandBuffer(cmdbuffer, 0);
  tex->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

static int vknvg_maxVertCount(const NVGpath *paths, int npaths) {
  int i, count = 0;
  for (i = 0; i < npaths; i++) {
    count += paths[i].nfill;
    count += paths[i].nstroke;
  }
  return count;
}

#ifndef USE_TOPOLOGY_TRIANGLE_FAN
static int vknvg_maxVertCountList(const NVGpath *paths, int npaths) {
  int i, count = 0;
  for (i = 0; i < npaths; i++) {
    count += (paths[i].nfill - 2) * 3;
    count += paths[i].nstroke;
  }
  return count;
}
#endif

static VKNVGcall *vknvg_allocCall(VKNVGcontext *vk) {
  VKNVGcall *ret = nullptr;
  if (vk->ncalls + 1 > vk->ccalls) {
    VKNVGcall *calls;
    int ccalls = vknvg_maxi(vk->ncalls + 1, 128) + vk->ccalls / 2; // 1.5x Overallocate
    calls = (VKNVGcall *) realloc(vk->calls, sizeof(VKNVGcall) * ccalls);
    if (calls == nullptr)
      return nullptr;
    vk->calls = calls;
    vk->ccalls = ccalls;
  }
  ret = &vk->calls[vk->ncalls++];
  memset(ret, 0, sizeof(VKNVGcall));
  return ret;
}

static int vknvg_allocPaths(VKNVGcontext *vk, int n) {
  int ret = 0;
  if (vk->npaths + n > vk->cpaths) {
    VKNVGpath *paths;
    int cpaths = vknvg_maxi(vk->npaths + n, 128) + vk->cpaths / 2; // 1.5x Overallocate
    paths = (VKNVGpath *) realloc(vk->paths, sizeof(VKNVGpath) * cpaths);
    if (paths == nullptr)
      return -1;
    vk->paths = paths;
    vk->cpaths = cpaths;
  }
  ret = vk->npaths;
  vk->npaths += n;
  return ret;
}

static int vknvg_allocVerts(VKNVGcontext *vk, int n) {
  int ret = 0;
  if (vk->nverts + n > vk->cverts) {
    NVGvertex *verts;
    int cverts = vknvg_maxi(vk->nverts + n, 4096) + vk->cverts / 2; // 1.5x Overallocate
    verts = (NVGvertex *) realloc(vk->verts, sizeof(NVGvertex) * cverts);
    if (verts == nullptr)
      return -1;
    vk->verts = verts;
    vk->cverts = cverts;
  }
  ret = vk->nverts;
  vk->nverts += n;
  return ret;
}

static int vknvg_allocFragUniforms(VKNVGcontext *vk, int n) {
  int ret = 0, structSize = vk->fragSize;
  if (vk->nuniforms + n > vk->cuniforms) {
    unsigned char *uniforms;
    int cuniforms = vknvg_maxi(vk->nuniforms + n, 128) + vk->cuniforms / 2; // 1.5x Overallocate
    uniforms = (unsigned char *) realloc(vk->uniforms, structSize * cuniforms);
    if (uniforms == nullptr)
      return -1;
    vk->uniforms = uniforms;
    vk->cuniforms = cuniforms;
  }
  ret = vk->nuniforms * structSize;
  vk->nuniforms += n;
  return ret;
}
static VKNVGfragUniforms *vknvg_fragUniformPtr(VKNVGcontext *vk, int i) { return (VKNVGfragUniforms *) &vk->uniforms[i]; }

static void vknvg_vset(NVGvertex *vtx, float x, float y, float u, float v) {
  vtx->x = x;
  vtx->y = y;
  vtx->u = u;
  vtx->v = v;
}

static void vknvg_setUniforms(VKNVGcontext *vk, VkDescriptorSet descSet, int uniformOffset, int image) {
  VkDevice device = vk->createInfo.device;
  uint32_t currentFrame = *vk->createInfo.currentFrame;

  vk->vertexConstants.uniformOffset = uniformOffset / vk->fragSize;
  vkCmdPushConstants(vk->createInfo.cmdBuffer[currentFrame], vk->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(VkNvgVertexConstants), &vk->vertexConstants);

  VKNVGtexture *tex = NULL;
  if (image != 0) {
    tex = vknvg_findTexture(vk, image);
  } else {
    tex = vknvg_findTexture(vk, 1);
  }

  VkDescriptorImageInfo image_info;
  image_info.imageLayout = tex->imageLayout;
  image_info.imageView = tex->view;
  image_info.sampler = tex->sampler;

  VkWriteDescriptorSet write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
  write.dstSet = descSet;
  write.dstBinding = 1;
  write.descriptorCount = 1;
  write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  write.pImageInfo = &image_info;
  vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
}

static void vknvg_fill(VKNVGcontext *vk, VKNVGcall *call, uint32_t descriptor_offset) {
  VKNVGpath *paths = &vk->paths[call->pathOffset];
  int npaths = call->pathCount;

  uint32_t currentFrame = *vk->createInfo.currentFrame;
  VkCommandBuffer cmdBuffer = vk->createInfo.cmdBuffer[currentFrame];

#ifdef __cplusplus
  VKNVGCreatePipelineKey pipelineKey = {};
#else
  VKNVGCreatePipelineKey pipelineKey = {0};
#endif
  pipelineKey.compositOperation = call->compositOperation;
#ifndef USE_TOPOLOGY_TRIANGLE_FAN
  pipelineKey.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
#else
  pipelineKey.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
#endif
  pipelineKey.stencilFill = true;

  vknvg_bindPipeline(vk, cmdBuffer, &pipelineKey);
  vknvg_setDynamicState(vk, cmdBuffer, &pipelineKey);
  vknvg_setUniforms(vk, vk->uniformDescriptorSet[descriptor_offset], call->uniformOffset, call->image);
  VkDescriptorSet sets[2] = {vk->ssboDescriptorSet[currentFrame], vk->uniformDescriptorSet[descriptor_offset]};
  vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk->pipelineLayout, 0, 2, sets, 0, nullptr);

  for (int i = 0; i < npaths; i++) {
    vkCmdDraw(cmdBuffer, paths[i].fillCount, 1, paths[i].fillOffset, 0);
  }

  vknvg_setUniforms(vk, vk->uniformDescriptorSet2[descriptor_offset], call->uniformOffset + vk->fragSize, call->image);
  sets[1] = vk->uniformDescriptorSet2[descriptor_offset];
  vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk->pipelineLayout, 0, 2, sets, 0, nullptr);

  if (vk->flags & NVG_ANTIALIAS) {

    pipelineKey.compositOperation = call->compositOperation;
    pipelineKey.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    pipelineKey.stencilFill = false;
    pipelineKey.stencilTest = true;
    pipelineKey.edgeAA = true;
    vknvg_bindPipeline(vk, cmdBuffer, &pipelineKey);
    vknvg_setDynamicState(vk, cmdBuffer, &pipelineKey);
    // Draw fringes
    for (int i = 0; i < npaths; ++i) {
      vkCmdDraw(cmdBuffer, paths[i].strokeCount, 1, paths[i].strokeOffset, 0);
    }
  }

  pipelineKey.compositOperation = call->compositOperation;
  pipelineKey.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
  pipelineKey.stencilFill = false;
  pipelineKey.stencilTest = true;
  pipelineKey.edgeAA = false;
  vknvg_bindPipeline(vk, cmdBuffer, &pipelineKey);
  vknvg_setDynamicState(vk, cmdBuffer, &pipelineKey);
  vkCmdDraw(cmdBuffer, call->triangleCount, 1, call->triangleOffset, 0);
}

static void vknvg_convexFill(VKNVGcontext *vk, VKNVGcall *call, uint32_t descriptor_offset) {
  VKNVGpath *paths = &vk->paths[call->pathOffset];
  int npaths = call->pathCount;

  const uint32_t currentFrame = *vk->createInfo.currentFrame;
  const VkCommandBuffer cmdBuffer = vk->createInfo.cmdBuffer[currentFrame];

#ifdef __cplusplus
  VKNVGCreatePipelineKey pipelineKey = {};
#else
  VKNVGCreatePipelineKey pipelineKey = {0};
#endif
  pipelineKey.compositOperation = call->compositOperation;
#ifndef USE_TOPOLOGY_TRIANGLE_FAN
  pipelineKey.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
#else
  pipelineKey.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
#endif

  vknvg_bindPipeline(vk, cmdBuffer, &pipelineKey);
  vknvg_setDynamicState(vk, cmdBuffer, &pipelineKey);
  vknvg_setUniforms(vk, vk->uniformDescriptorSet[descriptor_offset], call->uniformOffset, call->image);
  const VkDescriptorSet sets[2] = {vk->ssboDescriptorSet[currentFrame], vk->uniformDescriptorSet[descriptor_offset]};
  vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk->pipelineLayout, 0, 2, sets, 0, nullptr);

  for (int i = 0; i < npaths; ++i) {
    vkCmdDraw(cmdBuffer, paths[i].fillCount, 1, paths[i].fillOffset, 0);
  }
  if (vk->flags & NVG_ANTIALIAS) {
    pipelineKey.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    vknvg_bindPipeline(vk, cmdBuffer, &pipelineKey);
    vknvg_setDynamicState(vk, cmdBuffer, &pipelineKey);
    // Draw fringes
    for (int i = 0; i < npaths; ++i) {
      vkCmdDraw(cmdBuffer, paths[i].strokeCount, 1, paths[i].strokeOffset, 0);
    }
  }
}

static void vknvg_stroke(VKNVGcontext *vk, VKNVGcall *call, uint32_t descriptor_offset) {
  const uint32_t currentFrame = *vk->createInfo.currentFrame;
  const VkCommandBuffer cmdBuffer = vk->createInfo.cmdBuffer[currentFrame];

  VKNVGpath *paths = &vk->paths[call->pathOffset];
  int npaths = call->pathCount;

  if (vk->flags & NVG_STENCIL_STROKES) {

#ifdef __cplusplus
    VKNVGCreatePipelineKey pipelineKey = {};
#else
    VKNVGCreatePipelineKey pipelineKey = {0};
#endif
    pipelineKey.compositOperation = call->compositOperation;
    pipelineKey.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    // Fill stencil with 1 if stencil EQUAL passes
    pipelineKey.stencilStroke = VKNVG_STENCIL_STROKE_FILL;

    vknvg_bindPipeline(vk, cmdBuffer, &pipelineKey);
    vknvg_setDynamicState(vk, cmdBuffer, &pipelineKey);
    vknvg_setUniforms(vk, vk->uniformDescriptorSet2[descriptor_offset], call->uniformOffset + vk->fragSize, call->image);
    VkDescriptorSet sets[2] = {vk->ssboDescriptorSet[currentFrame], vk->uniformDescriptorSet2[descriptor_offset]};
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk->pipelineLayout, 0, 2, sets, 0, nullptr);

    for (int i = 0; i < npaths; ++i) {
      vkCmdDraw(cmdBuffer, paths[i].strokeCount, 1, paths[i].strokeOffset, 0);
    }

    vknvg_setUniforms(vk, vk->uniformDescriptorSet[descriptor_offset], call->uniformOffset, call->image);
    sets[1] = vk->uniformDescriptorSet[descriptor_offset];
    // //Draw AA shape if stencil EQUAL passes
    pipelineKey.stencilStroke = VKNVG_STENCIL_STROKE_DRAW_AA;
    vknvg_bindPipeline(vk, cmdBuffer, &pipelineKey);
    vknvg_setDynamicState(vk, cmdBuffer, &pipelineKey);
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk->pipelineLayout, 0, 2, sets, 0, nullptr);

    for (int i = 0; i < npaths; ++i) {
      vkCmdDraw(cmdBuffer, paths[i].strokeCount, 1, paths[i].strokeOffset, 0);
    }

    // Fill stencil with 0, always
    pipelineKey.stencilStroke = VKNVG_STENCIL_STROKE_CLEAR;
    vknvg_bindPipeline(vk, cmdBuffer, &pipelineKey);
    vknvg_setDynamicState(vk, cmdBuffer, &pipelineKey);
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk->pipelineLayout, 0, 2, sets, 0, nullptr);

    for (int i = 0; i < npaths; ++i) {
      vkCmdDraw(cmdBuffer, paths[i].strokeCount, 1, paths[i].strokeOffset, 0);
    }
  } else {

#ifdef __cplusplus
    VKNVGCreatePipelineKey pipelineKey = {};
#else
    VKNVGCreatePipelineKey pipelineKey = {0};
#endif

    pipelineKey.compositOperation = call->compositOperation;
    pipelineKey.stencilFill = false;
    pipelineKey.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    vknvg_bindPipeline(vk, cmdBuffer, &pipelineKey);
    vknvg_setDynamicState(vk, cmdBuffer, &pipelineKey);
    vknvg_setUniforms(vk, vk->uniformDescriptorSet[descriptor_offset], call->uniformOffset, call->image);
    VkDescriptorSet sets[2] = {vk->ssboDescriptorSet[currentFrame], vk->uniformDescriptorSet[descriptor_offset]};
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk->pipelineLayout, 0, 2, sets, 0, nullptr);
    // Draw Strokes

    VkDeviceSize offsets[] = {0};
    for (int i = 0; i < npaths; ++i) {
      vkCmdDraw(cmdBuffer, paths[i].strokeCount, 1, paths[i].strokeOffset, 0);
    }
  }
}

static void vknvg_triangles(VKNVGcontext *vk, VKNVGcall *call, uint32_t descriptor_offset) {
  if (call->triangleCount == 0) {
    return;
  }
  const uint32_t currentFrame = *vk->createInfo.currentFrame;
  const VkCommandBuffer cmdBuffer = vk->createInfo.cmdBuffer[currentFrame];

#ifdef __cplusplus
  VKNVGCreatePipelineKey pipelineKey = {};
#else
  VKNVGCreatePipelineKey pipelineKey = {0};
#endif
  pipelineKey.compositOperation = call->compositOperation;
  pipelineKey.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  pipelineKey.stencilFill = false;

  vknvg_bindPipeline(vk, cmdBuffer, &pipelineKey);
  vknvg_setDynamicState(vk, cmdBuffer, &pipelineKey);
  vknvg_setUniforms(vk, vk->uniformDescriptorSet[descriptor_offset], call->uniformOffset, call->image);
  const VkDescriptorSet sets[2] = {vk->ssboDescriptorSet[currentFrame], vk->uniformDescriptorSet[descriptor_offset]};
  vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk->pipelineLayout, 0, 2, sets, 0, nullptr);

  vkCmdDraw(cmdBuffer, call->triangleCount, 1, call->triangleOffset, 0);
}

///==================================================================================================================
static int vknvg_renderCreate(void *uptr) {
#ifdef __cplusplus
  auto *vk = static_cast<VKNVGcontext *>(uptr);
#else
  VKNVGcontext *vk = uptr;
#endif
  const VkDevice device = vk->createInfo.device;
  const VkAllocationCallbacks *allocator = vk->createInfo.allocator;

  vkGetPhysicalDeviceMemoryProperties(vk->createInfo.gpu, &vk->memoryProperties);
  vkGetPhysicalDeviceProperties(vk->createInfo.gpu, &vk->gpuProperties);

  const uint32_t fillVertShader[] = {
#include "shader/fill.vert.inc"
  };

  const uint32_t fillFragShader[] = {
#include "shader/fill.frag.inc"
  };

  vk->fillVertShader = vknvg_createShaderModule(device, fillVertShader, sizeof(fillVertShader), allocator);
  vk->fillFragShader = vknvg_createShaderModule(device, fillFragShader, sizeof(fillFragShader), allocator);
  VkDeviceSize align = vk->gpuProperties.limits.minUniformBufferOffsetAlignment;

  vk->fragSize = (int) sizeof(VKNVGfragUniforms); // std430 does not need padding

  vknvg_createDescriptorSetLayout(device, allocator, vk->descLayout);
  vk->pipelineLayout = vknvg_createPipelineLayout(vk, allocator);

  VkPhysicalDeviceFeatures supportedFeatures;
  vkGetPhysicalDeviceFeatures(vk->createInfo.gpu, &supportedFeatures);

  return 1;
}

static int vknvg_renderCreateTexture(void *uptr, int type, int w, int h, int imageFlags, const unsigned char *data) {
#ifdef __cplusplus
  auto *vk = static_cast<VKNVGcontext *>(uptr);
#else
  VKNVGcontext *vk = uptr;
#endif

  VKNVGtexture *tex = vknvg_allocTexture(vk);
  if (!tex) {
    return 0;
  }

  VkDevice device = vk->createInfo.device;
  const VkAllocationCallbacks *allocator = vk->createInfo.allocator;

  VkImageCreateInfo image_createInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  image_createInfo.pNext = nullptr;
  image_createInfo.imageType = VK_IMAGE_TYPE_2D;
  if (type == NVG_TEXTURE_RGBA) {
    image_createInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
  } else {
    image_createInfo.format = VK_FORMAT_R8_UNORM;
  }

  image_createInfo.extent.width = w;
  image_createInfo.extent.height = h;
  image_createInfo.extent.depth = 1;
  image_createInfo.mipLevels = 1;
  image_createInfo.arrayLayers = 1;
  image_createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  image_createInfo.tiling = VK_IMAGE_TILING_LINEAR;
  image_createInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
  image_createInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
  image_createInfo.queueFamilyIndexCount = 0;
  image_createInfo.pQueueFamilyIndices = nullptr;
  image_createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  image_createInfo.flags = 0;

  VkMemoryAllocateInfo mem_alloc = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
  mem_alloc.allocationSize = 0;

  VkImage mappableImage;
  VkDeviceMemory mappableMemory;

  NVGVK_CHECK_RESULT(vkCreateImage(device, &image_createInfo, allocator, &mappableImage));

  VkMemoryRequirements mem_reqs;
  vkGetImageMemoryRequirements(device, mappableImage, &mem_reqs);

  mem_alloc.allocationSize = mem_reqs.size;

  VkFlags flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
  VkResult res = vknvg_memory_type_from_properties(vk->memoryProperties, mem_reqs.memoryTypeBits, flags, &mem_alloc.memoryTypeIndex);
  assert(res == VK_SUCCESS);

  NVGVK_CHECK_RESULT(vkAllocateMemory(device, &mem_alloc, allocator, &mappableMemory));

  NVGVK_CHECK_RESULT(vkBindImageMemory(device, mappableImage, mappableMemory, 0));

  VkSamplerCreateInfo samplerCreateInfo = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
  if (imageFlags & NVG_IMAGE_NEAREST) {
    samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
    samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
  } else {
    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
  }
  samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  if (imageFlags & NVG_IMAGE_REPEATX) {
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
  } else {
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  }
  samplerCreateInfo.mipLodBias = 0.0;
  samplerCreateInfo.anisotropyEnable = VK_FALSE;
  samplerCreateInfo.maxAnisotropy = 1;
  samplerCreateInfo.compareEnable = VK_FALSE;
  samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
  samplerCreateInfo.minLod = 0.0;
  samplerCreateInfo.maxLod = 0.0;
  samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

  /* create sampler */
  NVGVK_CHECK_RESULT(vkCreateSampler(device, &samplerCreateInfo, allocator, &tex->sampler));

  VkImageViewCreateInfo view_info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
  view_info.pNext = nullptr;
  view_info.image = mappableImage;
  view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  view_info.format = image_createInfo.format;
  view_info.components.r = VK_COMPONENT_SWIZZLE_R;
  view_info.components.g = VK_COMPONENT_SWIZZLE_G;
  view_info.components.b = VK_COMPONENT_SWIZZLE_B;
  view_info.components.a = VK_COMPONENT_SWIZZLE_A;
  view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  view_info.subresourceRange.baseMipLevel = 0;
  view_info.subresourceRange.levelCount = 1;
  view_info.subresourceRange.baseArrayLayer = 0;
  view_info.subresourceRange.layerCount = 1;

  VkImageView image_view;
  NVGVK_CHECK_RESULT(vkCreateImageView(device, &view_info, allocator, &image_view));

  tex->height = h;
  tex->width = w;
  tex->image = mappableImage;
  tex->view = image_view;
  tex->mem = mappableMemory;
  tex->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  tex->type = type;
  tex->flags = imageFlags;
  if (data) {
    vknvg_UpdateTexture(device, tex, 0, 0, w, h, data);
  } else {
    int tx_format = 1;
    if (type == NVG_TEXTURE_RGBA)
      tx_format = 4;
    size_t texture_size = w * h * tx_format * sizeof(uint8_t);


#ifdef __cplusplus
    auto *generated_texture = static_cast<uint8_t *>(malloc(texture_size));
#else
    uint8_t *generated_texture = malloc(texture_size);
#endif

    for (uint32_t i = 0; i < (uint32_t) w; ++i) {
      for (uint32_t j = 0; j < (uint32_t) h; ++j) {
        size_t pixel = (i + j * w) * tx_format * sizeof(uint8_t);
        if (type == NVG_TEXTURE_RGBA) {
          generated_texture[pixel + 0] = 0x00;
          generated_texture[pixel + 1] = 0x00;
          generated_texture[pixel + 2] = 0x00;
          generated_texture[pixel + 3] = 0x00;
        } else
          generated_texture[pixel + 0] = 0x00;
      }
    }
    vknvg_UpdateTexture(device, tex, 0, 0, w, h, generated_texture);
    free(generated_texture);
  }

  uint32_t currentFrame = *vk->createInfo.currentFrame;
  vknvg_InitTexture(vk->createInfo.cmdBuffer[currentFrame], vk->queue, tex);

  return vknvg_textureId(vk, tex);
}
static int vknvg_renderDeleteTexture(void *uptr, int image) {

#ifdef __cplusplus
  auto *vk = static_cast<VKNVGcontext *>(uptr);
#else
  VKNVGcontext *vk = uptr;
#endif

  VKNVGtexture *tex = vknvg_findTexture(vk, image);

  if (tex->mapped) {
    vkUnmapMemory(vk->createInfo.device, tex->mem);
    tex->mapped = false;
  }
  return vknvg_deleteTexture(vk, tex);
}
static int vknvg_renderUpdateTexture(void *uptr, int image, int x, int y, int w, int h, const unsigned char *data) {
#ifdef __cplusplus
  auto *vk = static_cast<VKNVGcontext *>(uptr);
#else
  VKNVGcontext *vk = uptr;
#endif
  VKNVGtexture *tex = vknvg_findTexture(vk, image);
  vknvg_UpdateTexture(vk->createInfo.device, tex, x, y, w, h, data);
  return 1;
}
static int vknvg_renderGetTextureSize(void *uptr, int image, int *w, int *h) {
#ifdef __cplusplus
  auto *vk = static_cast<VKNVGcontext *>(uptr);
#else
  VKNVGcontext *vk = uptr;
#endif
  VKNVGtexture *tex = vknvg_findTexture(vk, image);
  if (tex) {
    *w = tex->width;
    *h = tex->height;
    return 1;
  }
  return 0;
}
static void vknvg_renderViewport(void *uptr, float width, float height, float devicePixelRatio) {
#ifdef __cplusplus
  auto *vk = static_cast<VKNVGcontext *>(uptr);
#else
  VKNVGcontext *vk = uptr;
#endif
  vk->vertexConstants.viewSize[0] = width;
  vk->vertexConstants.viewSize[1] = height;
}
static void vknvg_renderCancel(void *uptr) {
#ifdef __cplusplus
  auto *vk = static_cast<VKNVGcontext *>(uptr);
#else
  VKNVGcontext *vk = uptr;
#endif

  vk->nverts = 0;
  vk->npaths = 0;
  vk->ncalls = 0;
  vk->nuniforms = 0;
}

static void vknvg_renderFlush(void *uptr) {
#ifdef __cplusplus
  auto *vk = static_cast<VKNVGcontext *>(uptr);
#else
  VKNVGcontext *vk = uptr;
#endif
  const VkDevice device = vk->createInfo.device;
  const uint32_t currentFrame = *vk->createInfo.currentFrame;
  const VkPhysicalDeviceMemoryProperties memoryProperties = vk->memoryProperties;
  const VkAllocationCallbacks *allocator = vk->createInfo.allocator;

  if (vk->vertexBuffer == nullptr) {
    const uint32_t maxFramesInFlight = vk->createInfo.swapchainImageCount;
    vk->vertexBuffer = (VKNVGBuffer *) calloc(maxFramesInFlight, sizeof(VKNVGBuffer));
    vk->fragUniformBuffer = (VKNVGBuffer *) calloc(maxFramesInFlight, sizeof(VKNVGBuffer));
  }

  if (vk->ncalls > 0) {
    int i;
    const VkFlags flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    vknvg_UpdateBuffer(device, allocator, &vk->vertexBuffer[currentFrame], memoryProperties, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, flags, vk->verts, vk->nverts * sizeof(vk->verts[0]));
    vknvg_UpdateBuffer(device, allocator, &vk->fragUniformBuffer[currentFrame], memoryProperties, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, flags, vk->uniforms, vk->nuniforms * vk->fragSize);

    const VkDeviceSize offsets[1] = {0};
    vkCmdBindVertexBuffers(vk->createInfo.cmdBuffer[currentFrame], 0, 1, &vk->vertexBuffer[currentFrame].buffer, offsets);

    vk->currentPipeline = nullptr;

    if (vk->ncalls > vk->cdescPool) {
      vkDestroyDescriptorPool(device, vk->descPool, allocator);

      uint32_t pool_totals = 0;
      pool_totals += vk->ncalls * vk->createInfo.swapchainImageCount; // uniform texture descriptors
      pool_totals += vk->createInfo.swapchainImageCount; // ssbo descriptors
      vk->descPool = vknvg_createDescriptorPool(device, pool_totals, allocator);

      free(vk->uniformDescriptorSet);
      free(vk->uniformDescriptorSet2);
      free(vk->ssboDescriptorSet);

#ifdef __cplusplus
      vk->uniformDescriptorSet = static_cast<VkDescriptorSet *>(calloc(vk->ncalls * vk->createInfo.swapchainImageCount, sizeof(VkDescriptorSet)));
      vk->uniformDescriptorSet2 = static_cast<VkDescriptorSet *>(calloc(vk->ncalls * vk->createInfo.swapchainImageCount, sizeof(VkDescriptorSet)));
      vk->ssboDescriptorSet = static_cast<VkDescriptorSet *>(calloc(vk->createInfo.swapchainImageCount, sizeof(VkDescriptorSet)));
#else
      vk->uniformDescriptorSet = (VkDescriptorSet *) calloc(vk->ncalls * vk->createInfo.swapchainImageCount, sizeof(VkDescriptorSet));
      vk->uniformDescriptorSet2 = (VkDescriptorSet *) calloc(vk->ncalls * vk->createInfo.swapchainImageCount, sizeof(VkDescriptorSet));
      vk->ssboDescriptorSet = (VkDescriptorSet *) calloc(vk->createInfo.swapchainImageCount, sizeof(VkDescriptorSet));
#endif

      VkDescriptorSetAllocateInfo alloc_info_0 = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, vk->descPool, 1, &vk->descLayout[0]};
      for (i = 0; i < vk->createInfo.swapchainImageCount; i++) {
        NVGVK_CHECK_RESULT(vkAllocateDescriptorSets(device, &alloc_info_0, &vk->ssboDescriptorSet[i]))
      }

      VkDescriptorSetAllocateInfo alloc_info_1 = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, vk->descPool, 1, &vk->descLayout[1]};
      for (i = 0; i < vk->ncalls * vk->createInfo.swapchainImageCount; i++) {
        NVGVK_CHECK_RESULT(vkAllocateDescriptorSets(device, &alloc_info_1, &vk->uniformDescriptorSet[i]))
        NVGVK_CHECK_RESULT(vkAllocateDescriptorSets(device, &alloc_info_1, &vk->uniformDescriptorSet2[i]))
      }

      vk->cdescPool = vk->ncalls;
    }

#ifdef __cplusplus
    VkDescriptorBufferInfo buffer_info = {};
#else
    VkDescriptorBufferInfo buffer_info = {0};
#endif
    buffer_info.buffer = vk->fragUniformBuffer[currentFrame].buffer;
    buffer_info.offset = 0;
    buffer_info.range = vk->nuniforms * vk->fragSize;

    VkWriteDescriptorSet write_frag_data = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write_frag_data.dstSet = vk->ssboDescriptorSet[currentFrame];
    write_frag_data.descriptorCount = 1;
    write_frag_data.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write_frag_data.pBufferInfo = &buffer_info;
    write_frag_data.dstBinding = 0;
    vkUpdateDescriptorSets(device, 1, &write_frag_data, 0, nullptr);

    const uint32_t descriptor_offset = vk->cdescPool * currentFrame; // ensure descriptor sets dont clash
    for (i = 0; i < vk->ncalls; i++) {
      VKNVGcall *call = &vk->calls[i];
      if (call->type == VKNVG_FILL) {
        vknvg_fill(vk, call, descriptor_offset + i);
      } else if (call->type == VKNVG_CONVEXFILL) {
        vknvg_convexFill(vk, call, descriptor_offset + i);
      } else if (call->type == VKNVG_STROKE) {
        vknvg_stroke(vk, call, descriptor_offset + i);
      } else if (call->type == VKNVG_TRIANGLES) {
        vknvg_triangles(vk, call, descriptor_offset + i);
      }
    }
  }
  // Reset calls
  vk->nverts = 0;
  vk->npaths = 0;
  vk->ncalls = 0;
  vk->nuniforms = 0;
}

static void vknvg_renderFill(void *uptr, NVGpaint *paint, NVGcompositeOperationState compositeOperation, NVGscissor *scissor, float fringe, const float *bounds, const NVGpath *paths, int npaths) {
#ifdef __cplusplus
  auto *vk = static_cast<VKNVGcontext *>(uptr);
#else
  VKNVGcontext *vk = uptr;
#endif
  VKNVGcall *call = vknvg_allocCall(vk);
  NVGvertex *quad;
  VKNVGfragUniforms *frag;
  int i, maxverts, offset;

  if (call == NULL)
    return;

  call->type = VKNVG_FILL;
  call->triangleCount = 4;
  call->pathOffset = vknvg_allocPaths(vk, npaths);
  if (call->pathOffset == -1)
    goto error;
  call->pathCount = npaths;
  call->image = paint->image;
  call->compositOperation = compositeOperation;

  if (npaths == 1 && paths[0].convex) {
    call->type = VKNVG_CONVEXFILL;
    call->triangleCount = 0; // Bounding box fill quad not needed for convex fill
  }

  // Allocate vertices for all the paths.
#ifndef USE_TOPOLOGY_TRIANGLE_FAN
  maxverts = vknvg_maxVertCountList(paths, npaths) + call->triangleCount;
#else
  maxverts = vknvg_maxVertCount(paths, npaths) + call->triangleCount;
#endif
  offset = vknvg_allocVerts(vk, maxverts);
  if (offset == -1)
    goto error;

  for (i = 0; i < npaths; i++) {
    VKNVGpath *copy = &vk->paths[call->pathOffset + i];
    const NVGpath *path = &paths[i];
    memset(copy, 0, sizeof(VKNVGpath));
    if (path->nfill > 0) {
      copy->fillOffset = offset;
      copy->fillCount = (path->nfill - 2) * 3;
#ifndef USE_TOPOLOGY_TRIANGLE_FAN
      int j;
      for (j = 0; j < path->nfill - 2; j++) {
        vk->verts[offset] = path->fill[0];
        vk->verts[offset + 1] = path->fill[j + 1];
        vk->verts[offset + 2] = path->fill[j + 2];
        offset += 3;
      }
#else
      copy->fillCount = path->nfill;
      memcpy(&vk->verts[offset], path->fill, sizeof(NVGvertex) * path->nfill);
      offset += path->nfill;
#endif
    }
    if (path->nstroke > 0) {
      copy->strokeOffset = offset;
      copy->strokeCount = path->nstroke;
      memcpy(&vk->verts[offset], path->stroke, sizeof(NVGvertex) * path->nstroke);
      offset += path->nstroke;
    }
  }

  // Setup uniforms for draw calls
  if (call->type == VKNVG_FILL) {
    // Quad
    call->triangleOffset = offset;
    quad = &vk->verts[call->triangleOffset];
    vknvg_vset(&quad[0], bounds[2], bounds[3], 0.5f, 1.0f);
    vknvg_vset(&quad[1], bounds[2], bounds[1], 0.5f, 1.0f);
    vknvg_vset(&quad[2], bounds[0], bounds[3], 0.5f, 1.0f);
    vknvg_vset(&quad[3], bounds[0], bounds[1], 0.5f, 1.0f);

    call->uniformOffset = vknvg_allocFragUniforms(vk, 2);
    if (call->uniformOffset == -1)
      goto error;
    // Simple shader for stencil
    frag = vknvg_fragUniformPtr(vk, call->uniformOffset);
    memset(frag, 0, sizeof(*frag));
    frag->strokeThr = -1.0f;
    frag->type = NSVG_SHADER_SIMPLE;
    // Fill shader
    vknvg_convertPaint(vk, vknvg_fragUniformPtr(vk, call->uniformOffset + vk->fragSize), paint, scissor, fringe, fringe, -1.0f);
  } else {
    call->uniformOffset = vknvg_allocFragUniforms(vk, 1);
    if (call->uniformOffset == -1)
      goto error;
    // Fill shader
    vknvg_convertPaint(vk, vknvg_fragUniformPtr(vk, call->uniformOffset), paint, scissor, fringe, fringe, -1.0f);
  }

  return;

error:
  // We get here if call alloc was ok, but something else is not.
  // Roll back the last call to prevent drawing it.
  if (vk->ncalls > 0)
    vk->ncalls--;
}

static void vknvg_renderStroke(void *uptr, NVGpaint *paint, NVGcompositeOperationState compositeOperation, NVGscissor *scissor, float fringe, float strokeWidth, const NVGpath *paths, int npaths) {
#ifdef __cplusplus
  auto *vk = static_cast<VKNVGcontext *>(uptr);
#else
  VKNVGcontext *vk = uptr;
#endif
  VKNVGcall *call = vknvg_allocCall(vk);

  int offset, maxverts, i;
  if (call == NULL)
    return;

  call->type = VKNVG_STROKE;
  call->pathOffset = vknvg_allocPaths(vk, npaths);
  if (call->pathOffset == -1)
    goto error;
  call->pathCount = npaths;
  call->image = paint->image;
  call->compositOperation = compositeOperation;

  // Allocate vertices for all the paths.
  maxverts = vknvg_maxVertCount(paths, npaths);
  offset = vknvg_allocVerts(vk, maxverts);
  if (offset == -1)
    goto error;

  for (i = 0; i < npaths; i++) {
    VKNVGpath *copy = &vk->paths[call->pathOffset + i];
    const NVGpath *path = &paths[i];
    memset(copy, 0, sizeof(VKNVGpath));
    if (path->nstroke) {
      copy->strokeOffset = offset;
      copy->strokeCount = path->nstroke;
      memcpy(&vk->verts[offset], path->stroke, sizeof(NVGvertex) * path->nstroke);
      offset += path->nstroke;
    }
  }

  if (vk->flags & NVG_STENCIL_STROKES) {
    // Fill shader
    call->uniformOffset = vknvg_allocFragUniforms(vk, 2);
    if (call->uniformOffset == -1)
      goto error;

    vknvg_convertPaint(vk, vknvg_fragUniformPtr(vk, call->uniformOffset), paint, scissor, strokeWidth, fringe, -1.0f);
    vknvg_convertPaint(vk, vknvg_fragUniformPtr(vk, call->uniformOffset + vk->fragSize), paint, scissor, strokeWidth, fringe, 1.0f - 0.5f / 255.0f);

  } else {
    // Fill shader
    call->uniformOffset = vknvg_allocFragUniforms(vk, 1);
    if (call->uniformOffset == -1)
      goto error;
    vknvg_convertPaint(vk, vknvg_fragUniformPtr(vk, call->uniformOffset), paint, scissor, strokeWidth, fringe, -1.0f);
  }

  return;

error:
  // We get here if call alloc was ok, but something else is not.
  // Roll back the last call to prevent drawing it.
  if (vk->ncalls > 0)
    vk->ncalls--;
}

static void vknvg_renderTriangles(void *uptr, NVGpaint *paint, NVGcompositeOperationState compositeOperation, NVGscissor *scissor, const NVGvertex *verts, int nverts, float fringe) {
#ifdef __cplusplus
  auto *vk = static_cast<VKNVGcontext *>(uptr);
#else
  VKNVGcontext *vk = uptr;
#endif

  VKNVGcall *call = vknvg_allocCall(vk);
  VKNVGfragUniforms *frag;

  if (call == nullptr)
    return;

  call->type = VKNVG_TRIANGLES;
  call->image = paint->image;
  call->compositOperation = compositeOperation;

  // Allocate vertices for all the paths.
  call->triangleOffset = vknvg_allocVerts(vk, nverts);
  if (call->triangleOffset == -1)
    goto error;
  call->triangleCount = nverts;

  memcpy(&vk->verts[call->triangleOffset], verts, sizeof(NVGvertex) * nverts);

  // Fill shader
  call->uniformOffset = vknvg_allocFragUniforms(vk, 1);
  if (call->uniformOffset == -1)
    goto error;
  frag = vknvg_fragUniformPtr(vk, call->uniformOffset);
  vknvg_convertPaint(vk, frag, paint, scissor, 1.0f, fringe, -1.0f);
  frag->type = NSVG_SHADER_IMG;

  return;

error:
  // We get here if call alloc was ok, but something else is not.
  // Roll back the last call to prevent drawing it.
  if (vk->ncalls > 0)
    vk->ncalls--;
}

static void vknvg_renderDelete(void *uptr) {

#ifdef __cplusplus
  auto *vk = static_cast<VKNVGcontext *>(uptr);
#else
  VKNVGcontext *vk = uptr;
#endif

  VkDevice device = vk->createInfo.device;
  const VkAllocationCallbacks *allocator = vk->createInfo.allocator;

  for (int i = 0; i < vk->ntextures; i++) {
    if (vk->textures[i].image != VK_NULL_HANDLE) {
      vknvg_deleteTexture(vk, &vk->textures[i]);
    }
  }

  for (int i = 0; i < vk->createInfo.swapchainImageCount; i++) {
    vknvg_destroyBuffer(device, allocator, &vk->vertexBuffer[i]);
    vknvg_destroyBuffer(device, allocator, &vk->fragUniformBuffer[i]);
  }

  vkDestroyShaderModule(device, vk->fillVertShader, allocator);
  vkDestroyShaderModule(device, vk->fillFragShader, allocator);

  vkDestroyDescriptorPool(device, vk->descPool, allocator);
  vkDestroyDescriptorSetLayout(device, vk->descLayout[0], allocator);
  vkDestroyDescriptorSetLayout(device, vk->descLayout[1], allocator);
  vkDestroyPipelineLayout(device, vk->pipelineLayout, allocator);

  for (int i = 0; i < vk->npipelines; i++) {
    vkDestroyPipeline(device, vk->pipelines[i].pipeline, allocator);
  }

  free(vk->vertexBuffer);
  free(vk->fragUniformBuffer);

  free(vk->uniformDescriptorSet);
  free(vk->uniformDescriptorSet2);
  free(vk->ssboDescriptorSet);

  free(vk->textures);
  free(vk->pipelines);
  free(vk->calls);
  free(vk->paths);
  free(vk->verts);
  free(vk->uniforms);
  free(vk->descLayout);
  free(vk);
}

static NVGcontext *nvgCreateVk(VKNVGCreateInfo createInfo, int flags, VkQueue queue) {

  NVGparams params;
  NVGcontext *ctx = nullptr;

#ifdef __cplusplus
  auto *vk = static_cast<VKNVGcontext *>(malloc(sizeof(VKNVGcontext)));
#else
  VKNVGcontext *vk = malloc(sizeof(VKNVGcontext));
#endif

  if (vk == nullptr)
    goto error;
  memset(vk, 0, sizeof(VKNVGcontext));

  memset(&params, 0, sizeof(params));
  params.renderCreate = vknvg_renderCreate;
  params.renderCreateTexture = vknvg_renderCreateTexture;
  params.renderDeleteTexture = vknvg_renderDeleteTexture;
  params.renderUpdateTexture = vknvg_renderUpdateTexture;
  params.renderGetTextureSize = vknvg_renderGetTextureSize;
  params.renderViewport = vknvg_renderViewport;
  params.renderCancel = vknvg_renderCancel;
  params.renderFlush = vknvg_renderFlush;
  params.renderFill = vknvg_renderFill;
  params.renderStroke = vknvg_renderStroke;
  params.renderTriangles = vknvg_renderTriangles;
  params.renderDelete = vknvg_renderDelete;
  params.userPtr = vk;
  params.edgeAntiAlias = flags & NVG_ANTIALIAS ? 1 : 0;

  vk->flags = flags;
  vk->createInfo = createInfo;
  vk->queue = queue;


#ifdef __cplusplus
  vk->descLayout = static_cast<VkDescriptorSetLayout *>(calloc(2, sizeof(VkDescriptorSetLayout)));
  cmdSetPrimitiveTopology = reinterpret_cast<PFN_vkCmdSetPrimitiveTopologyEXT>(vkGetDeviceProcAddr(vk->createInfo.device, "vkCmdSetPrimitiveTopologyEXT"));
  cmdSetStencilTestEnable = reinterpret_cast<PFN_vkCmdSetStencilTestEnableEXT>(vkGetDeviceProcAddr(vk->createInfo.device, "vkCmdSetStencilTestEnableEXT"));
  cmdSetStencilOp = reinterpret_cast<PFN_vkCmdSetStencilOpEXT>(vkGetDeviceProcAddr(vk->createInfo.device, "vkCmdSetStencilOpEXT"));
  cmdSetColorBlendEquation = reinterpret_cast<PFN_vkCmdSetColorBlendEquationEXT>(vkGetDeviceProcAddr(vk->createInfo.device, "vkCmdSetColorBlendEquationEXT"));
  cmdSetColorWriteMask = reinterpret_cast<PFN_vkCmdSetColorWriteMaskEXT>(vkGetDeviceProcAddr(vk->createInfo.device, "vkCmdSetColorWriteMaskEXT"));
#else
  vk->descLayout = (VkDescriptorSetLayout *) calloc(2, sizeof(VkDescriptorSetLayout));
  cmdSetPrimitiveTopology = (PFN_vkCmdSetPrimitiveTopologyEXT) vkGetDeviceProcAddr(vk->createInfo.device, "vkCmdSetPrimitiveTopologyEXT");
  cmdSetStencilTestEnable = (PFN_vkCmdSetStencilTestEnableEXT) vkGetDeviceProcAddr(vk->createInfo.device, "vkCmdSetStencilTestEnableEXT");
  cmdSetStencilOp = (PFN_vkCmdSetStencilOpEXT) vkGetDeviceProcAddr(vk->createInfo.device, "vkCmdSetStencilOpEXT");
  cmdSetColorBlendEquation = (PFN_vkCmdSetColorBlendEquationEXT) vkGetDeviceProcAddr(vk->createInfo.device, "vkCmdSetColorBlendEquationEXT");
  cmdSetColorWriteMask = (PFN_vkCmdSetColorWriteMaskEXT) vkGetDeviceProcAddr(vk->createInfo.device, "vkCmdSetColorWriteMaskEXT");
#endif

  ctx = nvgCreateInternal(&params);
  if (ctx == nullptr)
    goto error;

  return ctx;

error:
  // 'gl' is freed by nvgDeleteInternal.
  if (ctx != nullptr)
    nvgDeleteInternal(ctx);
  return nullptr;
}

static void nvgDeleteVk(NVGcontext *ctx) { nvgDeleteInternal(ctx); }

static void vknvg_setDynamicState(VKNVGcontext *vk, VkCommandBuffer cmd, const VKNVGCreatePipelineKey *pipelineKey) {
  if (vk->ext.dynamicState) {
    vkCmdSetPrimitiveTopologyEXT(cmd, pipelineKey->topology);
  }
  if (vk->ext.colorWriteMask) {
    vkCmdSetColorWriteMaskEXT(cmd, 0, 1, &pipelineKey->colorWriteMask);
  }
  if (vk->ext.colorBlendEquation) {
    VkPipelineColorBlendAttachmentState colorBlendAttachment = vknvg_compositOperationToColorBlendAttachmentState(pipelineKey);
#ifdef __cplusplus
    VkColorBlendEquationEXT colorBlendEquation = {};
#else
    VkColorBlendEquationEXT colorBlendEquation = {0};
#endif
    colorBlendEquation.srcColorBlendFactor = colorBlendAttachment.srcColorBlendFactor;
    colorBlendEquation.dstColorBlendFactor = colorBlendAttachment.dstColorBlendFactor;
    colorBlendEquation.colorBlendOp = colorBlendAttachment.colorBlendOp;
    colorBlendEquation.srcAlphaBlendFactor = colorBlendAttachment.srcAlphaBlendFactor;
    colorBlendEquation.dstAlphaBlendFactor = colorBlendAttachment.dstAlphaBlendFactor;
    colorBlendEquation.alphaBlendOp = colorBlendAttachment.alphaBlendOp;
    vkCmdSetColorBlendEquationEXT(cmd, 0, 1, &colorBlendEquation);
  }
  if (vk->ext.dynamicState) {
    VkPipelineDepthStencilStateCreateInfo ds = initializeDepthStencilCreateInfo(pipelineKey);
    vkCmdSetStencilTestEnableEXT(cmd, ds.stencilTestEnable);
    if (ds.stencilTestEnable) {
      vkCmdSetStencilOpEXT(cmd, VK_STENCIL_FACE_FRONT_BIT, ds.front.failOp, ds.front.passOp, ds.front.depthFailOp, ds.front.compareOp);
      vkCmdSetStencilOpEXT(cmd, VK_STENCIL_FACE_BACK_BIT, ds.back.failOp, ds.back.passOp, ds.back.depthFailOp, ds.back.compareOp);
    }
  }
}

#if !defined(__cplusplus) || defined(NANOVG_VK_NO_nullptrPTR)
#undef nullptr
#endif
#endif
