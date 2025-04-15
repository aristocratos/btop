#ifndef INTEL_GPU_TOP_H
#define INTEL_GPU_TOP_H

#include <stdbool.h>
#include <stdint.h>
#include <dirent.h>

struct pmu_pair {
	uint64_t cur;
	uint64_t prev;
};

struct pmu_counter {
	uint64_t type;
	uint64_t config;
	unsigned int idx;
	struct pmu_pair val;
	double scale;
	const char *units;
	bool present;
};

struct engine_class {
	unsigned int engine_class;
	const char *name;
	unsigned int num_engines;
};

struct engine {
	const char *name;
	char *display_name;
	char *short_name;

	unsigned int class;
	unsigned int instance;

	unsigned int num_counters;

	struct pmu_counter busy;
	struct pmu_counter wait;
	struct pmu_counter sema;
};

#define MAX_GTS 4
struct engines {
	unsigned int num_engines;
	unsigned int num_classes;
	struct engine_class *class;
	unsigned int num_counters;
	DIR *root;
	int fd;
	struct pmu_pair ts;

	int rapl_fd;
	struct pmu_counter r_gpu, r_pkg;
	unsigned int num_rapl;

	int imc_fd;
	struct pmu_counter imc_reads;
	struct pmu_counter imc_writes;
	unsigned int num_imc;

	struct pmu_counter freq_req;
	struct pmu_counter freq_req_gt[MAX_GTS];
	struct pmu_counter freq_act;
	struct pmu_counter freq_act_gt[MAX_GTS];
	struct pmu_counter irq;
	struct pmu_counter rc6;
	struct pmu_counter rc6_gt[MAX_GTS];

	bool discrete;
	char *device;

	int num_gts;

	/* Do not edit below this line.
	 * This structure is reallocated every time a new engine is
	 * found and size is increased by sizeof (engine).
	 */

	struct engine engine;

};

struct engines *discover_engines(const char *device);
void free_engines(struct engines *engines);
int pmu_init(struct engines *engines);
void pmu_sample(struct engines *engines);
double pmu_calc(struct pmu_pair *p, double d, double t, double s);

char* find_intel_gpu_dir();
char* get_intel_device_id(const char* vendor_path);
char *get_intel_device_name(const char *device_id);

#endif