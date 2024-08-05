/*
 * Copyright © 2017 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */

#ifndef I915_PERF_H
#define I915_PERF_H

#include <stdint.h>

#ifdef __linux__
#include <linux/perf_event.h>
#endif

//#include "igt_gt.h"

static inline int
perf_event_open(struct perf_event_attr *attr,
		pid_t pid,
		int cpu,
		int group_fd,
		unsigned long flags)
{
#ifndef __NR_perf_event_open
#if defined(__i386__)
#define __NR_perf_event_open 336
#elif defined(__x86_64__)
#define __NR_perf_event_open 298
#else
#define __NR_perf_event_open 0
#endif
#endif
    attr->size = sizeof(*attr);
    return syscall(__NR_perf_event_open, attr, pid, cpu, group_fd, flags);
}

uint64_t igt_perf_type_id(const char *device);
int igt_perf_events_dir(int i915);
int igt_perf_open(uint64_t type, uint64_t config);
int igt_perf_open_group(uint64_t type, uint64_t config, int group);

const char *i915_perf_device(int i915, char *buf, int buflen);
uint64_t i915_perf_type_id(int i915);

const char *xe_perf_device(int xe, char *buf, int buflen);
uint64_t xe_perf_type_id(int);

int perf_igfx_open(uint64_t config);
int perf_igfx_open_group(uint64_t config, int group);

int perf_i915_open(int i915, uint64_t config);
int perf_i915_open_group(int i915, uint64_t config, int group);

int perf_xe_open(int xe, uint64_t config);

#endif /* I915_PERF_H */
