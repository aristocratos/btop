/* Copyright 2026 Ruaneri Portela (ruaneriportela@outlook.com)

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

           http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

   indent = tab
   tab-size = 4
*/
#pragma once
#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include <sys/_types/_pid_t.h>

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOTypes.h>

class GPUActivities {
  public:
    struct Usage {
        int64_t accumulated_gpu_time;
        int64_t last_submitted_time;
        std::string api;
    };

    std::vector<Usage> usage;
    pid_t proc;
    std::string name;
    int64_t command_queue_count;
    int64_t command_queue_count_max;
    GPUActivities(io_object_t io_accelerator_children);

  private:
    static void map_key_to_usage_string(GPUActivities::Usage& usage, const std::string& key, const std::string& value);
    static void map_key_to_usage_number(GPUActivities::Usage& usage, const std::string& key, int64_t value);
};

class GPU {
  public:
    struct PerformanceStatistics {
        int64_t device_utilization;

        int64_t alloc_system_memory;
        int64_t allocated_pb_size;
        int64_t in_use_system_memory;
        int64_t in_use_system_memory_driver;
        int64_t last_recovery_time;
        int64_t recovery_count;
        int64_t renderer_utilization;
        int64_t split_scene_count;
        int64_t tiled_scene_bytes;
        int64_t tiler_utilization;

        int64_t gpu_frequency;
        int64_t gpu_voltage;

        int64_t milliwatts;
        double temp_c;
    };

  private:
    std::string name;
    std::string driver;
    std::string io_path;

    PerformanceStatistics statistics;

    std::chrono::nanoseconds prev_gpu_elapsed {0};
    uint64_t actual_gpu_internal_time = 0;
    uint64_t last_gpu_internal_time = 0;

    std::unordered_map<pid_t, std::tuple<GPUActivities, uint64_t, double>> last_activities;
    std::unordered_map<pid_t, std::tuple<GPUActivities, uint64_t, double>> actual_activities;

    int64_t core_count;

    std::vector<std::tuple<uint32_t, uint32_t>> gpu_table;
    uint32_t max_freq = 0;
    uint32_t max_voltage = 0;

    void lookup_process_percentage();
    void lookup(io_object_t ioAccelerator);
    void map_key_to_performance_statistics(const std::string& key, int64_t value);
    void parser_channels(CFDictionaryRef delta, double elapsed_seconds);

    static bool children_iterator_callback(io_object_t object, void* data);

    static bool apple_arm_io_device_interator_callback(io_object_t object, void* data);

    std::chrono::nanoseconds prev_sample_time {0};
    CFTypeRef subscription = nullptr;
    CFMutableDictionaryRef channels = nullptr;
    CFDictionaryRef prev_sample = nullptr;

  public:
    GPU(io_object_t io_accelerator);

    const std::unordered_map<pid_t, std::tuple<GPUActivities, uint64_t, double>>& get_activities() const;
    const PerformanceStatistics& get_statistics() const;
    const std::string& get_name() const;
    const int64_t& get_core_count() const;

    bool refresh();

    ~GPU();
};

class IOGPU {
  private:
    std::vector<GPU> gpus;
    static bool iterator_gpu_callback(io_object_t object, void* data);
    static void ioReportTryLoad();

  public:
    IOGPU();
    std::vector<GPU>& get_gpus();
};