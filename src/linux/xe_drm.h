/* SPDX-License-Identifier: MIT */
/*
 * Copyright © 2023 Intel Corporation
 *
 * Minimal Xe DRM UAPI definitions for btop Intel GPU support.
 * Extracted from Linux kernel include/uapi/drm/xe_drm.h
 */

#ifndef _BTOP_XE_DRM_H_
#define _BTOP_XE_DRM_H_

#include <stdint.h>
#include <sys/ioctl.h>

#if defined(__cplusplus)
extern "C" {
#endif

#ifndef DRM_COMMAND_BASE
#define DRM_COMMAND_BASE 0x40
#endif

#ifndef DRM_IOWR
#define DRM_IOWR(nr, type) _IOWR('d', nr, type)
#endif

#define DRM_XE_DEVICE_QUERY 0x00

#define DRM_IOCTL_XE_DEVICE_QUERY DRM_IOWR(DRM_COMMAND_BASE + DRM_XE_DEVICE_QUERY, struct drm_xe_device_query)

struct drm_xe_engine_class_instance {
#define DRM_XE_ENGINE_CLASS_RENDER        0
#define DRM_XE_ENGINE_CLASS_COPY          1
#define DRM_XE_ENGINE_CLASS_VIDEO_DECODE  2
#define DRM_XE_ENGINE_CLASS_VIDEO_ENHANCE 3
#define DRM_XE_ENGINE_CLASS_COMPUTE       4
#define DRM_XE_ENGINE_CLASS_VM_BIND       5
	uint16_t engine_class;
	uint16_t engine_instance;
	uint16_t gt_id;
	uint16_t pad;
};

struct drm_xe_engine {
	struct drm_xe_engine_class_instance instance;
	uint64_t reserved[3];
};

struct drm_xe_query_engines {
	uint32_t num_engines;
	uint32_t pad;
	struct drm_xe_engine engines[];
};

#define DRM_XE_MEM_REGION_CLASS_SYSMEM 0
#define DRM_XE_MEM_REGION_CLASS_VRAM 1

struct drm_xe_mem_region {
	uint16_t mem_class;
	uint16_t instance;
	uint32_t min_page_size;
	uint64_t total_size;
	uint64_t used;
	uint64_t cpu_visible_size;
	uint64_t cpu_visible_used;
	uint64_t reserved[6];
};

struct drm_xe_query_mem_regions {
	uint32_t num_mem_regions;
	uint32_t pad;
	struct drm_xe_mem_region mem_regions[];
};

struct drm_xe_device_query {
	uint64_t extensions;
#define DRM_XE_DEVICE_QUERY_ENGINES       0
#define DRM_XE_DEVICE_QUERY_MEM_REGIONS   1
#define DRM_XE_DEVICE_QUERY_CONFIG        2
#define DRM_XE_DEVICE_QUERY_GT_LIST       3
#define DRM_XE_DEVICE_QUERY_HWCONFIG      4
#define DRM_XE_DEVICE_QUERY_GT_TOPOLOGY   5
#define DRM_XE_DEVICE_QUERY_ENGINE_CYCLES 6
#define DRM_XE_DEVICE_QUERY_UC_FW_VERSION 7
#define DRM_XE_DEVICE_QUERY_OA_UNITS      8
#define DRM_XE_DEVICE_QUERY_PXP_STATUS    9
	uint32_t query;
	uint32_t size;
	uint64_t data;
	uint64_t reserved[2];
};

#if defined(__cplusplus)
}
#endif

#endif
