/*
 * Copyright © 2007-2023 Intel Corporation
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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <termios.h>
#include <time.h>
#include <sys/sysmacros.h>

#include <stdarg.h>

#include "intel_gpu_top.h"
#include "i915_drm.h"
#include "igt_perf.h"

__attribute__((format(scanf,3,4)))
static int igt_sysfs_scanf(int dir, const char *attr, const char *fmt, ...)
{
	FILE *file;
	int fd;
	int ret = -1;

	fd = openat(dir, attr, O_RDONLY);
	if (fd < 0)
		return -1;

	file = fdopen(fd, "r");
	if (file) {
		va_list ap;

		va_start(ap, fmt);
		ret = vfscanf(file, fmt, ap);
		va_end(ap);

		fclose(file);
	} else {
		close(fd);
	}

	return ret;
}

static int pmu_parse(struct pmu_counter *pmu, const char *path, const char *str)
{
	locale_t locale, oldlocale;
	bool result = true;
	char buf[128] = {};
	int dir;

	dir = open(path, O_RDONLY);
	if (dir < 0)
		return -errno;

	/* Replace user environment with plain C to match kernel format */
	locale = newlocale(LC_ALL, "C", 0);
	oldlocale = uselocale(locale);

	result &= igt_sysfs_scanf(dir, "type", "%"PRIu64, &pmu->type) == 1;

	snprintf(buf, sizeof(buf) - 1, "events/%s", str);
	result &= igt_sysfs_scanf(dir, buf, "event=%"PRIx64, &pmu->config) == 1;

	snprintf(buf, sizeof(buf) - 1, "events/%s.scale", str);
	result &= igt_sysfs_scanf(dir, buf, "%lf", &pmu->scale) == 1;

	snprintf(buf, sizeof(buf) - 1, "events/%s.unit", str);
	result &= igt_sysfs_scanf(dir, buf, "%127s", buf) == 1;
	pmu->units = strdup(buf);

	uselocale(oldlocale);
	freelocale(locale);

	close(dir);

	if (!result)
		return -EINVAL;

	if (isnan(pmu->scale) || !pmu->scale)
		return -ERANGE;

	return 0;
}

static int rapl_parse(struct pmu_counter *pmu, const char *str)
{
	const char *expected_units = "Joules";
	int err;

	err = pmu_parse(pmu, "/sys/devices/power", str);
	if (err < 0)
		return err;

	if (!pmu->units || strcmp(pmu->units, expected_units)) {
		fprintf(stderr,
			"Unexpected units for RAPL %s: found '%s', expected '%s'\n",
			str, pmu->units, expected_units);
	}

	return 0;
}

static void
rapl_open(struct pmu_counter *pmu,
	  const char *domain,
	  struct engines *engines)
{
	int fd;

	if (rapl_parse(pmu, domain) < 0)
		return;

	fd = igt_perf_open_group(pmu->type, pmu->config, engines->rapl_fd);
	if (fd < 0)
		return;

	if (engines->rapl_fd == -1)
		engines->rapl_fd = fd;

	pmu->idx = engines->num_rapl++;
	pmu->present = true;
}

static void gpu_power_open(struct pmu_counter *pmu,
			   struct engines *engines)
{
	rapl_open(pmu, "energy-gpu", engines);
}

static void pkg_power_open(struct pmu_counter *pmu,
			   struct engines *engines)
{
	rapl_open(pmu, "energy-pkg", engines);
}

//* Read a decimal u64 attribute from a hwmon sysfs file. Returns 0 if the
//* file is missing or unreadable (best-effort; sensors may appear later).
static uint64_t hwmon_read_u64(const char *path)
{
	FILE *f = fopen(path, "r");
	if (!f)
		return 0;

	unsigned long long value = 0;
	if (fscanf(f, "%llu", &value) != 1)
		value = 0;

	fclose(f);
	return (uint64_t)value;
}

//* Locate the i915 hwmon device. The driver creates /sys/class/hwmon/hwmonN
//* whose `name` file equals `i915`; its `device` symlink points back at
//* the PCI GPU (e.g. 0000:2f:00.0). We match by name and, when we can derive
//* the PCI address from the resolved i915 device name, confirm the symlink so
//* we never read the wrong sensor on a multi-GPU host.
static bool hwmon_find(struct engines *engines, char *path, size_t size)
{
	DIR *d;
	struct dirent *de;
	char wanted[64] = {0};
	bool found = false;

	path[0] = 0;

	//* engines->device is the resolved perf name, e.g. "i915_0000_2f_00.0";
	//* turn it into "0000:2f:00.0" for the symlink match.
	if (engines->device) {
		const char *p = strstr(engines->device, "i915_");
		if (p) {
			p += 5;
			size_t i = 0;
			for (; p[i] && i < sizeof(wanted) - 1; i++)
				wanted[i] = (p[i] == '_') ? ':' : p[i];
		}
	}

	if ((d = opendir("/sys/class/hwmon")) == NULL)
		return false;

	while ((de = readdir(d)) != NULL) {
		char base[64], namepath[PATH_MAX], name[16];
		char target[PATH_MAX];
		ssize_t tn;

		//* Note: do not gate on d_type == DT_DIR. sysfs frequently reports
		//* d_type == DT_UNKNOWN, which would silently skip every entry.
		if (strncmp(de->d_name, "hwmon", 5) != 0)
			continue;

		snprintf(base, sizeof(base), "/sys/class/hwmon/%s", de->d_name);
		snprintf(namepath, sizeof(namepath), "%s/name", base);

		FILE *f = fopen(namepath, "r");
		if (f) {
			if (fgets(name, sizeof(name), f) && strncmp(name, "i915", 4) == 0) {
				//* Confirm the PCI address, if we have one to check against.
				bool dev_ok = true;
				if (wanted[0]) {
					snprintf(namepath, sizeof(namepath), "%s/device", base);
					tn = readlink(namepath, target, sizeof(target) - 1);
					if (tn >= 0) {
						target[tn] = 0;
						if (strstr(target, wanted) == NULL)
							dev_ok = false;
					}
				}

				if (dev_ok) {
					snprintf(path, size, "%s", base);
					found = true;
				}
			}
			fclose(f);
		}

		if (found)
			break;
	}

	closedir(d);
	return found;
}

static uint64_t
get_pmu_config(int dirfd, const char *name, const char *counter)
{
	char buf[128], *p;
	int fd, ret;

	ret = snprintf(buf, sizeof(buf), "%s-%s", name, counter);
	if (ret < 0 || ret == sizeof(buf))
		return -1;

	fd = openat(dirfd, buf, O_RDONLY);
	if (fd < 0)
		return -1;

	ret = read(fd, buf, sizeof(buf));
	close(fd);
	if (ret <= 0)
		return -1;

	p = index(buf, '0');
	if (!p)
		return -1;

	return strtoul(p, NULL, 0);
}

#define engine_ptr(engines, n) (&engines->engine + (n))

static const char *class_display_name(unsigned int class)
{
	switch (class) {
	case I915_ENGINE_CLASS_RENDER:
		return "Render/3D";
	case I915_ENGINE_CLASS_COPY:
		return "Blitter";
	case I915_ENGINE_CLASS_VIDEO:
		return "Video";
	case I915_ENGINE_CLASS_VIDEO_ENHANCE:
		return "VideoEnhance";
	case I915_ENGINE_CLASS_COMPUTE:
		return "Compute";
	default:
		return "[unknown]";
	}
}

static const char *class_short_name(unsigned int class)
{
	switch (class) {
	case I915_ENGINE_CLASS_RENDER:
		return "RCS";
	case I915_ENGINE_CLASS_COPY:
		return "BCS";
	case I915_ENGINE_CLASS_VIDEO:
		return "VCS";
	case I915_ENGINE_CLASS_VIDEO_ENHANCE:
		return "VECS";
	case I915_ENGINE_CLASS_COMPUTE:
		return "CCS";
	default:
		return "UNKN";
	}
}

static int engine_cmp(const void *__a, const void *__b)
{
	const struct engine *a = (struct engine *)__a;
	const struct engine *b = (struct engine *)__b;

	if (a->class != b->class)
		return a->class - b->class;
	else
		return a->instance - b->instance;
}

#define is_igpu(x) (strcmp(x, "i915") == 0)

struct engines *discover_engines(const char *device)
{
	char sysfs_root[PATH_MAX];
	char device_name[64];
	struct engines *engines;
	struct dirent *dent;
	int ret = 0;
	DIR *d, *scan;

	//* The i915 perf PMU used to be exposed as
	//* /sys/devices/<driver>/events. Kernels that identify the device by
	//* its PCI address expose it as /sys/devices/i915_<b:d.f>/events
	//* instead, so the bare <driver> path is absent. Resolve the concrete
	//* i915_* device by scanning /sys/devices for an entry exposing an
	//* events subdir, and remember its name (pmu_init needs it to find the
	//* perf event source type).
	snprintf(sysfs_root, sizeof(sysfs_root),
		 "/sys/devices/%s/events", device);
	snprintf(device_name, sizeof(device_name), "%s", device);
	if ((scan = opendir("/sys/devices")) != NULL) {
		struct dirent *ed;
		while ((ed = readdir(scan)) != NULL) {
			if (strncmp(ed->d_name, "i915", 4) != 0)
				continue;
			char cand[PATH_MAX];
			snprintf(cand, sizeof(cand),
				"/sys/devices/%s/events", ed->d_name);
			if (opendir(cand) != NULL) {
				snprintf(sysfs_root, sizeof(sysfs_root),
					"%s", cand);
				snprintf(device_name, sizeof(device_name),
					"%s", ed->d_name);
				break;
			}
		}
		closedir(scan);
	}

	engines = malloc(sizeof(struct engines));
	if (!engines)
		return NULL;

	memset(engines, 0, sizeof(*engines));

	engines->num_engines = 0;
	//* engines->device drives the perf-event-source lookup in pmu_init
	//* (/sys/bus/event_source/devices/<device>/type), so it must carry the
	//* resolved instance name rather than just "i915".
	engines->device = strdup(device_name);
	engines->discrete = !is_igpu(device);

	d = opendir(sysfs_root);
	if (!d)
		goto err;

	while ((dent = readdir(d)) != NULL) {
		const char *endswith = "-busy";
		const unsigned int endlen = strlen(endswith);
		struct engine *engine =
				engine_ptr(engines, engines->num_engines);
		char buf[256];

		if (dent->d_type != DT_REG)
			continue;

		if (strlen(dent->d_name) >= sizeof(buf)) {
			ret = ENAMETOOLONG;
			break;
		}

		strcpy(buf, dent->d_name);

		/* xxxN-busy */
		if (strlen(buf) < (endlen + 4))
			continue;
		if (strcmp(&buf[strlen(buf) - endlen], endswith))
			continue;

		memset(engine, 0, sizeof(*engine));

		buf[strlen(buf) - endlen] = 0;
		engine->name = strdup(buf);
		if (!engine->name) {
			ret = errno;
			break;
		}

		engine->busy.config = get_pmu_config(dirfd(d), engine->name,
						     "busy");
		if (engine->busy.config == -1) {
			ret = ENOENT;
			break;
		}

		/* Double check config is an engine config. */
		if (engine->busy.config >= __I915_PMU_OTHER(0)) {
			free((void *)engine->name);
			continue;
		}

		engine->class = (engine->busy.config &
				 (__I915_PMU_OTHER(0) - 1)) >>
				I915_PMU_CLASS_SHIFT;

		engine->instance = (engine->busy.config >>
				    I915_PMU_SAMPLE_BITS) &
				    ((1 << I915_PMU_SAMPLE_INSTANCE_BITS) - 1);

		ret = asprintf(&engine->display_name, "%s/%u",
			       class_display_name(engine->class),
			       engine->instance);
		if (ret <= 0) {
			ret = errno;
			break;
		}

		ret = asprintf(&engine->short_name, "%s/%u",
			       class_short_name(engine->class),
			       engine->instance);
		if (ret <= 0) {
			ret = errno;
			break;
		}

		engines->num_engines++;
		engines = realloc(engines, sizeof(struct engines) +
				  engines->num_engines * sizeof(struct engine));
		if (!engines) {
			ret = errno;
			break;
		}

		ret = 0;
	}

	if (ret) {
		errno = ret;
		goto err;
	}

	qsort(engine_ptr(engines, 0), engines->num_engines,
	      sizeof(struct engine), engine_cmp);

	engines->root = d;

	return engines;

err:
	if (engines->device)
		free(engines->device);
	free(engines);

	return NULL;
}

void free_engines(struct engines *engines)
{
	struct pmu_counter **pmu, *free_list[] = {
		&engines->r_gpu,
		&engines->r_pkg,
		&engines->imc_reads,
		&engines->imc_writes,
		NULL
	};
	unsigned int i;

	if (!engines)
		return;

	for (pmu = &free_list[0]; *pmu; pmu++) {
		if ((*pmu)->present)
			free((char *)(*pmu)->units);
	}

	for (i = 0; i < engines->num_engines; i++) {
		struct engine *engine = engine_ptr(engines, i);

		free((char *)engine->name);
		free((char *)engine->short_name);
		free((char *)engine->display_name);
	}

	closedir(engines->root);

	if (engines->device)
		free(engines->device);
	free(engines->class);
	free(engines);
}

#define _open_pmu(type, cnt, pmu, fd) \
({ \
	int fd__; \
\
	fd__ = igt_perf_open_group((type), (pmu)->config, (fd)); \
	if (fd__ >= 0) { \
		if ((fd) == -1) \
			(fd) = fd__; \
		(pmu)->present = true; \
		(pmu)->idx = (cnt)++; \
	} \
\
	fd__; \
})

static int imc_parse(struct pmu_counter *pmu, const char *str)
{
	return pmu_parse(pmu, "/sys/devices/uncore_imc", str);
}

static void
imc_open(struct pmu_counter *pmu,
	 const char *domain,
	 struct engines *engines)
{
	int fd;

	if (imc_parse(pmu, domain) < 0)
		return;

	fd = igt_perf_open_group(pmu->type, pmu->config, engines->imc_fd);
	if (fd < 0)
		return;

	if (engines->imc_fd == -1)
		engines->imc_fd = fd;

	pmu->idx = engines->num_imc++;
	pmu->present = true;
}

static void imc_writes_open(struct pmu_counter *pmu, struct engines *engines)
{
	imc_open(pmu, "data_writes", engines);
}

static void imc_reads_open(struct pmu_counter *pmu, struct engines *engines)
{
	imc_open(pmu, "data_reads", engines);
}

static int get_num_gts(uint64_t type)
{
	int fd, cnt;

	errno = 0;
	for (cnt = 0; cnt < MAX_GTS; cnt++) {
		fd = igt_perf_open(type, __I915_PMU_INTERRUPTS(cnt));
		if (fd < 0)
			break;

		close(fd);
	}

	if (!cnt || (errno && errno != ENOENT))
		cnt = -errno;

	return cnt;
}

static void init_aggregate_counters(struct engines *engines)
{
	struct pmu_counter *pmu;

	pmu = &engines->freq_req;
	pmu->type = igt_perf_type_id(engines->device);
	pmu->config = I915_PMU_REQUESTED_FREQUENCY;
	pmu->present = true;

	pmu = &engines->freq_act;
	pmu->type = igt_perf_type_id(engines->device);
	pmu->config = I915_PMU_ACTUAL_FREQUENCY;
	pmu->present = true;

	pmu = &engines->rc6;
	pmu->type = igt_perf_type_id(engines->device);
	pmu->config = I915_PMU_RC6_RESIDENCY;
	pmu->present = true;
}

int pmu_init(struct engines *engines)
{
	unsigned int i;
	int fd;
	uint64_t type = igt_perf_type_id(engines->device);

	engines->fd = -1;
	engines->num_counters = 0;
	engines->num_gts = get_num_gts(type);
	if (engines->num_gts <= 0)
		return -1;

	engines->irq.config = I915_PMU_INTERRUPTS;
	fd = _open_pmu(type, engines->num_counters, &engines->irq, engines->fd);
	if (fd < 0)
		return -1;

	init_aggregate_counters(engines);

	for (i = 0; i < engines->num_gts; i++) {
		engines->freq_req_gt[i].config = __I915_PMU_REQUESTED_FREQUENCY(i);
		_open_pmu(type, engines->num_counters, &engines->freq_req_gt[i], engines->fd);

		engines->freq_act_gt[i].config = __I915_PMU_ACTUAL_FREQUENCY(i);
		_open_pmu(type, engines->num_counters, &engines->freq_act_gt[i], engines->fd);

		engines->rc6_gt[i].config = __I915_PMU_RC6_RESIDENCY(i);
		_open_pmu(type, engines->num_counters, &engines->rc6_gt[i], engines->fd);
	}

	for (i = 0; i < engines->num_engines; i++) {
		struct engine *engine = engine_ptr(engines, i);
		struct {
			struct pmu_counter *pmu;
			const char *counter;
		} *cnt, counters[] = {
			{ .pmu = &engine->busy, .counter = "busy" },
			{ .pmu = &engine->wait, .counter = "wait" },
			{ .pmu = &engine->sema, .counter = "sema" },
			{ .pmu = NULL, .counter = NULL },
		};

		for (cnt = counters; cnt->pmu; cnt++) {
			if (!cnt->pmu->config)
				cnt->pmu->config =
					get_pmu_config(dirfd(engines->root),
						       engine->name,
						       cnt->counter);
			fd = _open_pmu(type, engines->num_counters, cnt->pmu,
				       engines->fd);
			if (fd >= 0)
				engine->num_counters++;
		}
	}

	engines->rapl_fd = -1;
	if (!engines->discrete) {
		gpu_power_open(&engines->r_gpu, engines);
		pkg_power_open(&engines->r_pkg, engines);
	}

	engines->imc_fd = -1;
	imc_reads_open(&engines->imc_reads, engines);
	imc_writes_open(&engines->imc_writes, engines);

	//* DG1 / IGPUs have no RAPL `energy-gpu` PMU; fall back to the i915
	//* hwmon sensor for power and temperature.
	engines->hwmon_energy.present = false;
	engines->temp_milli = -1;
	if (hwmon_find(engines, engines->hwmon_path, sizeof(engines->hwmon_path))) {
		engines->hwmon_present = true;
		engines->hwmon_energy.present = true;
		engines->hwmon_energy.idx = 0;
	}

	return 0;
}

static uint64_t pmu_read_multi(int fd, unsigned int num, uint64_t *val)
{
	uint64_t buf[2 + num];
	unsigned int i;
	ssize_t len;

	memset(buf, 0, sizeof(buf));

	len = read(fd, buf, sizeof(buf));
	assert(len == sizeof(buf));

	for (i = 0; i < num; i++)
		val[i] = buf[2 + i];

	return buf[1];
}

double pmu_calc(struct pmu_pair *p, double d, double t, double s)
{
	double v;

	v = p->cur - p->prev;
	v /= d;
	v /= t;
	v *= s;

	if (s == 100.0 && v > 100.0)
		v = 100.0;

	return v;
}

static void __update_sample(struct pmu_counter *counter, uint64_t val)
{
	counter->val.prev = counter->val.cur;
	counter->val.cur = val;
}

static void update_sample(struct pmu_counter *counter, uint64_t *val)
{
	if (counter->present)
		__update_sample(counter, val[counter->idx]);
}

void pmu_sample(struct engines *engines)
{
	const int num_val = engines->num_counters;
	uint64_t val[2 + num_val];
	unsigned int i;

	engines->ts.prev = engines->ts.cur;
	engines->ts.cur = pmu_read_multi(engines->fd, num_val, val);

	engines->freq_req.val.cur = engines->freq_req.val.prev = 0;
	engines->freq_act.val.cur = engines->freq_act.val.prev = 0;
	engines->rc6.val.cur = engines->rc6.val.prev = 0;

	for (i = 0; i < engines->num_gts; i++) {
		update_sample(&engines->freq_req_gt[i], val);
		engines->freq_req.val.cur += engines->freq_req_gt[i].val.cur;
		engines->freq_req.val.prev += engines->freq_req_gt[i].val.prev;

		update_sample(&engines->freq_act_gt[i], val);
		engines->freq_act.val.cur += engines->freq_act_gt[i].val.cur;
		engines->freq_act.val.prev += engines->freq_act_gt[i].val.prev;

		update_sample(&engines->rc6_gt[i], val);
		engines->rc6.val.cur += engines->rc6_gt[i].val.cur;
		engines->rc6.val.prev += engines->rc6_gt[i].val.prev;
	}

	engines->freq_req.val.cur /= engines->num_gts;
	engines->freq_req.val.prev /= engines->num_gts;

	engines->freq_act.val.cur /= engines->num_gts;
	engines->freq_act.val.prev /= engines->num_gts;

	engines->rc6.val.cur /= engines->num_gts;
	engines->rc6.val.prev /= engines->num_gts;

	update_sample(&engines->irq, val);

	for (i = 0; i < engines->num_engines; i++) {
		struct engine *engine = engine_ptr(engines, i);

		update_sample(&engine->busy, val);
		update_sample(&engine->sema, val);
		update_sample(&engine->wait, val);
	}

	if (engines->num_rapl) {
		pmu_read_multi(engines->rapl_fd, engines->num_rapl, val);
		update_sample(&engines->r_gpu, val);
		update_sample(&engines->r_pkg, val);
	}

	if (engines->num_imc) {
		pmu_read_multi(engines->imc_fd, engines->num_imc, val);
		update_sample(&engines->imc_reads, val);
		update_sample(&engines->imc_writes, val);
	}
	//* Read the i915 hwmon sensors: the energy counter is a cumulative
	//* microjoule value (power = dE/dt), temp1_input is milli-Celsius.
	if (engines->hwmon_present) {
		char buf[PATH_MAX];

		snprintf(buf, sizeof(buf), "%s/energy1_input", engines->hwmon_path);
		engines->hwmon_energy.val.prev = engines->hwmon_energy.val.cur;
		engines->hwmon_energy.val.cur = hwmon_read_u64(buf);

		snprintf(buf, sizeof(buf), "%s/temp1_input", engines->hwmon_path);
		engines->temp_milli = (int)hwmon_read_u64(buf);
	}
}
