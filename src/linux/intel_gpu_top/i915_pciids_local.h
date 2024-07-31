/* SPDX-License-Identifier: MIT */
/*
 * Copyright © 2022 Intel Corporation
 */
#ifndef _I915_PCIIDS_LOCAL_H_
#define _I915_PCIIDS_LOCAL_H_

#include "i915_pciids.h"

/* MTL perf */
#ifndef INTEL_MTL_M_IDS
#define INTEL_MTL_M_IDS(MACRO__, ...) \
	MACRO__(0x7D60, ## __VA_ARGS__), \
	MACRO__(0x7D67, ## __VA_ARGS__)
#endif

#ifndef INTEL_MTL_P_GT2_IDS
#define INTEL_MTL_P_GT2_IDS(MACRO__, ...) \
	MACRO__(0x7D45, ## __VA_ARGS__)
#endif

#ifndef INTEL_MTL_P_GT3_IDS
#define INTEL_MTL_P_GT3_IDS(MACRO__, ...) \
	MACRO__(0x7D55, ## __VA_ARGS__), \
	MACRO__(0x7DD5, ## __VA_ARGS__)
#endif

#ifndef INTEL_MTL_P_IDS
#define INTEL_MTL_P_IDS(MACRO__, ...) \
	INTEL_MTL_P_GT2_IDS(MACRO__, ## __VA_ARGS__), \
	INTEL_MTL_P_GT3_IDS(MACRO__, ## __VA_ARGS__)
#endif

#ifndef INTEL_ARL_GT1_IDS
#define INTEL_ARL_GT1_IDS(MACRO__, ...) \
	MACRO__(0x7D41, ## __VA_ARGS__), \
	MACRO__(0x7D67, ## __VA_ARGS__)
#endif

#ifndef INTEL_ARL_GT2_IDS
#define INTEL_ARL_GT2_IDS(MACRO__, ...) \
	MACRO__(0x7D51, ## __VA_ARGS__), \
	MACRO__(0x7DD1, ## __VA_ARGS__)
#endif

#ifndef INTEL_ARL_IDS
#define INTEL_ARL_IDS(MACRO__, ...) \
	INTEL_ARL_GT1_IDS(MACRO__, ## __VA_ARGS__), \
	INTEL_ARL_GT2_IDS(MACRO__, ## __VA_ARGS__)
#endif

/* PVC */
#ifndef INTEL_PVC_IDS
#define INTEL_PVC_IDS(MACRO__, ...) \
	MACRO__(0x0BD0, ## __VA_ARGS__), \
	MACRO__(0x0BD1, ## __VA_ARGS__), \
	MACRO__(0x0BD2, ## __VA_ARGS__), \
	MACRO__(0x0BD5, ## __VA_ARGS__), \
	MACRO__(0x0BD6, ## __VA_ARGS__), \
	MACRO__(0x0BD7, ## __VA_ARGS__), \
	MACRO__(0x0BD8, ## __VA_ARGS__), \
	MACRO__(0x0BD9, ## __VA_ARGS__), \
	MACRO__(0x0BDA, ## __VA_ARGS__), \
	MACRO__(0x0BDB, ## __VA_ARGS__)
#endif

#endif /* _I915_PCIIDS_LOCAL_H */
