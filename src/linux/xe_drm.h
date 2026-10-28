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
};

struct drm_xe_device_query {
	uint64_t extensions;
#define DRM_XE_DEVICE_QUERY_MEM_REGIONS   1
	uint32_t query;
	uint32_t size;
	uint64_t data;
	uint64_t reserved[2];
};

#if defined(__cplusplus)
}
#endif

#endif
