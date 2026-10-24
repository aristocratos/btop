#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <ctype.h>

#include "intel_gpu_top.h"
#include "intel_chipset.h"

#define VENDOR_ID "0x8086"
#define SYSFS_PATH "/sys/class/drm"
#define VENDOR_FILE "vendor"
#define DEVICE_FILE "device"

static bool is_card_node(const char *name) {
    if (strncmp(name, "card", 4) != 0)
        return false;

    const char *p = name + 4;
    if (*p == '\0')
        return false;

    for (; *p; p++) {
        if (*p < '0' || *p > '9')
            return false;
    }

    return true;
}

char* find_intel_gpu_dir() {
    static char path[256];
    char dirs[1][256];

    if (find_intel_gpu_dirs(dirs, 1) < 1)
        return NULL;

    snprintf(path, sizeof(path), "%s", dirs[0]);

    return path;
}

int find_intel_gpu_dirs(char dirs[][256], int max_count) {
    DIR *dir;
    struct dirent *entry;
    char vendor_path[256];
    char vendor_id[16];
    int count = 0;

    if ((dir = opendir(SYSFS_PATH)) == NULL) {
        perror("opendir");
        return 0;
    }

    while (count < max_count && (entry = readdir(dir)) != NULL) {
        if (!is_card_node(entry->d_name))
            continue;

        // Construct the path to the vendor file
        snprintf(vendor_path, sizeof(vendor_path), "%s/%s/device/%s", SYSFS_PATH, entry->d_name, VENDOR_FILE);

        // Check if the vendor file exists
        if (access(vendor_path, F_OK) != -1) {
            FILE *file = fopen(vendor_path, "r");
            if (file) {
                if (fgets(vendor_id, sizeof(vendor_id), file)) {
                    // Trim the newline character
                    vendor_id[strcspn(vendor_id, "\n")] = 0;

                    if (strcmp(vendor_id, VENDOR_ID) == 0) {
                        // Record the parent directory (i.e., /sys/class/drm/card*)
                        snprintf(dirs[count], 256, "%s/%s", SYSFS_PATH, entry->d_name);
                        count++;
                    }
                }
                fclose(file);
            }
        }
    }

    closedir(dir);

    return count;  // Number of Intel GPUs found (0 if none)
}

char *get_intel_pmu_device_name(const char *gpu_dir) {
    static char pmu_name[160];
    char sanitized_bus[128];
    char device_path[300];
    char driver_link[256];
    char driver_path[300];
    char link_target[512];
    ssize_t len;

    // Resolve <gpu_dir>/device -> .../<pci bus id>, e.g. .../0000:03:00.0
    snprintf(device_path, sizeof(device_path), "%s/device", gpu_dir);

    len = readlink(device_path, link_target, sizeof(link_target) - 1);
    if (len < 0)
        return NULL;

    link_target[len] = '\0';
    char *bus_id = strrchr(link_target, '/');
    bus_id = bus_id ? bus_id + 1 : link_target;

    // Resolve <gpu_dir>/device/driver -> .../i915 or .../xe
    const char *driver_name = "i915";
    snprintf(driver_path, sizeof(driver_path), "%s/device/driver", gpu_dir);

    ssize_t dlen = readlink(driver_path, driver_link, sizeof(driver_link) - 1);
    if (dlen > 0) {
        driver_link[dlen] = '\0';
        char *dname = strrchr(driver_link, '/');
        driver_name = dname ? dname + 1 : driver_link;
    }

    // Return if no i915/xe managed GPU or PMU is available
    if (strcmp(driver_name, "i915") != 0 && strcmp(driver_name, "xe") != 0)
        return NULL;

    // Sanitize bus id for perf: colons become underscores, e.g. 0000:03:00.0 -> 0000_03_00_0
    snprintf(sanitized_bus, sizeof(sanitized_bus), "%s", bus_id);
    for (char *s = sanitized_bus; *s; s++) {
        if (*s == ':')
            *s = '_';
    }

    if (strcmp(driver_name, "xe") == 0)
        snprintf(pmu_name, sizeof(pmu_name), "xe_%s", sanitized_bus);
    else if (strcmp(bus_id, "0000:00:02.0") == 0)
        // Legacy PMU name for the primary integrated GPU
        snprintf(pmu_name, sizeof(pmu_name), "i915");
    else
        snprintf(pmu_name, sizeof(pmu_name), "i915_%s", sanitized_bus);

    return strdup(pmu_name);
}

char* get_intel_device_id(const char* gpu_dir) {
    static char device_path[256];
    char device_id[16];

    // Construct the path to the device file
    snprintf(device_path, sizeof(device_path), "%s/device/%s", gpu_dir, DEVICE_FILE);

    FILE *file = fopen(device_path, "r");
    if (file) {
        if (fgets(device_id, sizeof(device_id), file)) {
            fclose(file);
            // Trim the newline character
            device_id[strcspn(device_id, "\n")] = 0;
            // Return a copy of the device ID
            return strdup(device_id);
        }
        fclose(file);
    } else {
        perror("fopen");
    }

    return NULL;
}

char *get_intel_device_name(const char *device_id) {
    uint16_t devid = strtol(device_id, NULL, 16);
    char dev_name[256];
    char full_name[256];
    const struct intel_device_info *info = intel_get_device_info(devid);
    if (info) {
        if (info->codename == NULL) {
            strcpy(dev_name, "(unknown)");
        } else {
            strcpy(dev_name, info->codename);
            dev_name[0] = toupper(dev_name[0]);
        }
        snprintf(full_name, sizeof(full_name), "Intel %s (Gen%u)", dev_name, info->graphics_ver);
        return strdup(full_name);
    }
    return NULL;
}
