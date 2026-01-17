/* Copyright 2026 Ruaneri Portela (ruaneriportela@outlook.com)
   Copyright 2026 Emanuele Zattin (Emanuelez@gmail.com)

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
#include <mach/mach_time.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <tuple>
#include <unordered_map>
#include <utility>

#include "gpu.hpp"
#include "iokit.hpp"

//? Others
static uint64_t get_time_ns() {
    static mach_timebase_info_data_t timebase = {0, 0};
    if (timebase.denom == 0) {
        mach_timebase_info(&timebase);
    }
    return mach_absolute_time() * timebase.numer / timebase.denom;
}

//? This does not work for the same reason as in sensors.cpp
static double get_gpu_temperature() {
    CFDictionaryRef matching = create_hid_matching(0xff00, 5);
    IOHIDEventSystemClientRef system = IOHIDEventSystemClientCreate(kCFAllocatorDefault);
    IOHIDEventSystemClientSetMatching(system, matching);
    CFArrayRef services = IOHIDEventSystemClientCopyServices(system);

    double gpu_temp = 0.0;

    if (services != nullptr) {
        CFIndex count = CFArrayGetCount(services);
        for (CFIndex i = 0; i < count; i++) {
            IOHIDServiceClientRef service = (IOHIDServiceClientRef)CFArrayGetValueAtIndex(services, i);
            if (service != nullptr) {
                CFStringRef name = IOHIDServiceClientCopyProperty(service, CFSTR("Product"));
                if (name != nullptr) {
                    std::string sensor_name =
                        safe_cfstring_to_std_string(name).value_or("");
                    CFRelease(name);

                    //? In M4 CPU show all cores temperatures, but need to
                    // parser
                    if (sensor_name.find("GPU") != std::string::npos) {
                        IOHIDEventRef event = IOHIDServiceClientCopyEvent(service, kIOHIDEventTypeTemperature, 0, 0);
                        if (event != nullptr) {
                            double temp = IOHIDEventGetFloatValue(event, IOHIDEventFieldBase(kIOHIDEventTypeTemperature));
                            if (temp > 0 and temp < 150) {  //? Sanity check
                                gpu_temp += temp;
                            }
                            CFRelease(event);
                        }
                    }
                }
            }
        }
        CFRelease(services);
    }
    CFRelease(matching);
    CFRelease(system);
    return gpu_temp;
}

void GPUActivities::map_key_to_usage_number(GPUActivities::Usage &usage, const std::string &key, int64_t value) {
    static const std::unordered_map<std::string, int64_t GPUActivities::Usage::*> map = {
            {"accumulatedGPUTime", &GPUActivities::Usage::accumulated_gpu_time},
            {"lastSubmittedTime", &GPUActivities::Usage::last_submitted_time},
    };

    if (auto it = map.find(key); it != map.end()) {
        usage.*(it->second) = value;
    }
}

void GPUActivities::map_key_to_usage_string(GPUActivities::Usage &usage, const std::string &key, const std::string &value) {
    static const std::unordered_map<std::string, std::string GPUActivities::Usage::*> map = {
            {"API", &GPUActivities::Usage::api},
    };

    if (auto it = map.find(key); it != map.end()) {
        usage.*(it->second) = value;
    }
}

//? Creates the process entry, the parameter entry, and the IOKit object that
//? references it. Each process can have zero or more graphical usage contexts.
GPUActivities::GPUActivities(io_object_t entry) {
    CFTypeRef creator_ref = IORegistryEntryCreateCFProperty(entry, CFSTR("IOUserClientCreator"), kCFAllocatorDefault, 0);

    if (not creator_ref) return;

    auto creator = safe_cfstring_to_std_string(static_cast<CFStringRef>(creator_ref));

    CFRelease(creator_ref);

    if (not creator) return;

    const std::string &s = *creator;

    const auto pidPos = s.find("pid ");
    const auto commaPos = s.find(',');

    if (pidPos == std::string::npos or commaPos == std::string::npos) return;

    try {
        proc = std::stoul(s.substr(pidPos + 4, commaPos - (pidPos + 4)));
        name = s.substr(commaPos + 2);
    } catch (...) {
        return;
    }

    CFTypeRef app_usage_ref = IORegistryEntryCreateCFProperty(entry, CFSTR("AppUsage"), kCFAllocatorDefault, 0);

    if (not app_usage_ref) return;

    if (CFGetTypeID(app_usage_ref) != CFArrayGetTypeID()) {
        CFRelease(app_usage_ref);
        return;
    }

    CFArrayRef app_usage_array = static_cast<CFArrayRef>(app_usage_ref);
    CFIndex count = CFArrayGetCount(app_usage_array);

    for (CFIndex i = 0; i < count; ++i) {
        CFTypeRef item = CFArrayGetValueAtIndex(app_usage_array, i);

        if (not item or CFGetTypeID(item) != CFDictionaryGetTypeID()) continue;

        CFDictionaryRef usage_stats = static_cast<CFDictionaryRef>(item);
        if (usage_stats) {
            CFIndex count = CFDictionaryGetCount(usage_stats);

            std::vector<const void *> keys(count);
            std::vector<const void *> values(count);

            CFDictionaryGetKeysAndValues(usage_stats, keys.data(),
                                         values.data());
            GPUActivities::Usage new_usage{};
            for (CFIndex i = 0; i < count; ++i) {
                CFStringRef keyRef = static_cast<CFStringRef>(keys[i]);

                auto key = safe_cfstring_to_std_string(keyRef);
                if (!key) continue;

                auto number = safe_cfdictionary_to_int64(usage_stats, keyRef);
                if (number.has_value()) {
                    map_key_to_usage_number(new_usage, *key, *number);
                    continue;
                }

                auto string =
                    safe_cfdictionary_to_std_string(usage_stats, keyRef);
                if (string.has_value()) {
                    map_key_to_usage_string(new_usage, *key, *string);
                    continue;
                }
            }
            usage.emplace_back(new_usage);
        }
    }
    CFRelease(app_usage_ref);
}

//? Converts each entry to its value in the struct
void GPU::map_key_to_performance_statistics(const std::string &key, int64_t value) {
    static const std::unordered_map<std::string, int64_t PerformanceStatistics::*>map = {
            {"Alloc system memory", &PerformanceStatistics::alloc_system_memory},
            {"Allocated PB Size", &PerformanceStatistics::allocated_pb_size},
            {"Device Utilization %", &PerformanceStatistics::device_utilization},
            {"In use system memory", &PerformanceStatistics::in_use_system_memory},
            {"In use system memory (driver)", &PerformanceStatistics::in_use_system_memory_driver},
            {"lastRecoveryTime", &PerformanceStatistics::last_recovery_time},
            {"recoveryCount", &PerformanceStatistics::recovery_count},
            {"Renderer Utilization %", &PerformanceStatistics::renderer_utilization},
            {"SplitSceneCount", &PerformanceStatistics::split_scene_count},
            {"TiledSceneBytes", &PerformanceStatistics::tiled_scene_bytes},
            {"Tiler Utilization %", &PerformanceStatistics::tiler_utilization},
    };

    if (auto it = map.find(key); it != map.end()) {
        statistics.*(it->second) = value;
    }
}

//? This callback will run for every child of the IOGPU type input. These
//? children are assumed to be processes that are using that GPU.
bool GPU::children_iterator_callback(io_object_t object, void *data) {
    auto *self = static_cast<GPU *>(data);

    GPUActivities new_activity(object);

    if (new_activity.usage.size() == 0) {
        return true;
    }

    uint64_t total_usage = 0;

    for (const auto &u : new_activity.usage) {
        total_usage += u.accumulated_gpu_time;
    }

    self->actual_gpu_internal_time += total_usage;

    self->actual_activities.insert_or_assign(new_activity.proc,std::make_tuple(std::move(new_activity), total_usage, 0));

    return true;
}

//? Obtains the table with frequencies and corresponding voltages, necessary for
//? IOReport to function. Once you find the pmgr, you should stop iterating.
bool GPU::apple_arm_io_device_interator_callback(io_object_t device, void *data) {
    auto *self = static_cast<GPU *>(data);

    io_name_t name;
    IORegistryEntryGetName(device, name);

    if (std::strcmp(name, "pmgr") != 0) {
        return true;
    }

    CFMutableDictionaryRef properties = nullptr;
    if (IORegistryEntryCreateCFProperties( device, &properties, kCFAllocatorDefault, 0) != kIOReturnSuccess) {
          return false;
    }
      

    auto buffer = safe_cfdictionary_to_raw_vector(properties, CFSTR("voltage-states9"));

    if (buffer) {
        const auto &bytes = *buffer;
        const CFIndex length = bytes.size();

        if (length >= 8 and (length % 8) == 0) {
            self->gpu_table.clear();
            uint32_t max_freq = 0;
            uint32_t max_voltage = 0;

            // For 8 bytes: [freq_hz(4), voltage_uv(4)]
            for (CFIndex i = 0; i + 8 <= length; i += 8) {
                uint32_t freq_hz = 0;
                uint32_t voltage_uv = 0;

                std::memcpy(&freq_hz, bytes.data() + i, sizeof(freq_hz));
                std::memcpy(&voltage_uv, bytes.data() + i + 4,sizeof(voltage_uv));

                if (freq_hz > 0) {
                    self->gpu_table.emplace_back(freq_hz, voltage_uv);

                    if (freq_hz > max_freq) {
                        max_freq = freq_hz;
                    }
                    if (voltage_uv > max_voltage) {
                        max_voltage = voltage_uv;
                    }
                }
            }

            self->max_freq = max_freq;
            self->max_voltage = max_voltage;

            std::sort(self->gpu_table.begin(), self->gpu_table.end(),[](const auto &a, const auto &b) {
                          return std::get<0>(a) < std::get<0>(b);
                      });
        }
    }

    CFRelease(properties);
    return false;
}

//? With some estimate of GPU usage already in class, we calculate and normalize
//? GPU time by its usage, comparing the current input with the previous one.
void GPU::lookup_process_percentage() {
    int64_t delta_gpu_internal_time = actual_gpu_internal_time - last_gpu_internal_time;

    uint64_t actual_gpu_seconds_elapsed = get_time_ns();
    prev_gpu_elapsed_seconds = actual_gpu_seconds_elapsed;

    if (!last_activities.empty() and delta_gpu_internal_time > 0) {
        double denom = static_cast<double>(delta_gpu_internal_time);
        double maxUtil = static_cast<double>(statistics.device_utilization);

        for (auto &[pid, currTuple] : actual_activities) {
            auto &totalTime = std::get<1>(currTuple);
            auto &target_percentage = std::get<2>(currTuple);

            auto it = last_activities.find(pid);
            if (it == last_activities.end()) {
                continue;
            }

            auto &prev_total_time = std::get<1>(it->second);

            uint64_t deltaGpuTime = (totalTime >= prev_total_time) ? (totalTime - prev_total_time) : 0;

            if (deltaGpuTime == 0) {
                continue;
            }

            double relative = static_cast<double>(deltaGpuTime) / denom;

            target_percentage = std::clamp(relative * maxUtil, 0.0, maxUtil);
        }
    }
}

//? Here we look at the data from the IOAccelerator class input, which provides
//? data without the need for IOReport, such as memory and simple GPU usage and
//? process list
void GPU::lookup(io_object_t io_accelerator) {
    CFDictionaryRef perfStats =
        static_cast<CFDictionaryRef>(IORegistryEntryCreateCFProperty(io_accelerator, CFSTR("PerformanceStatistics"), kCFAllocatorDefault,0));

    if (perfStats) {
        CFIndex count = CFDictionaryGetCount(perfStats);

        std::vector<const void *> keys(count);
        std::vector<const void *> values(count);

        CFDictionaryGetKeysAndValues(perfStats, keys.data(), values.data());

        for (CFIndex i = 0; i < count; ++i) {
            auto key = safe_cfstring_to_std_string(static_cast<CFStringRef>(keys[i]));
            if (!key) {
                continue;
            }

            auto value = safe_cfnumber_to_int64(static_cast<CFNumberRef>(values[i]));
            if (key) {
                map_key_to_performance_statistics(*key, *value);
            }
        }

        CFRelease(perfStats);
    }

    last_activities.clear();

    last_gpu_internal_time = actual_gpu_internal_time;
    actual_gpu_internal_time = 0;
    if (actual_activities.size() > 0) {
        last_activities = std::move(actual_activities);
    }

    actual_activities.clear();

    io_service_children_interator(io_accelerator, kIOServicePlane,children_iterator_callback, this);
}

GPU::GPU(io_object_t ioAccelerator) {
    // Save full path to fast refresh on some stats, eg. memory
    io_name_t path;
    if (IORegistryEntryGetPath(ioAccelerator, kIOServicePlane, path) != KERN_SUCCESS){
        return;
    }
        
    io_path = std::string(path);

    CFStringRef nameRef = static_cast<CFStringRef>(IORegistryEntryCreateCFProperty(ioAccelerator, CFSTR("model"), kCFAllocatorDefault, 0));

    if (nameRef) {
        name = safe_cfstring_to_std_string(nameRef).value_or("Undefined");
        CFRelease(nameRef);
    }

    CFStringRef driverRef = static_cast<CFStringRef>(IORegistryEntryCreateCFProperty(ioAccelerator, CFSTR("IOClass"), kCFAllocatorDefault, 0));

    if (driverRef) {
        driver = safe_cfstring_to_std_string(driverRef).value_or("Undefined");
        CFRelease(driverRef);
    }

    CFNumberRef coreCountRef = static_cast<CFNumberRef>(IORegistryEntryCreateCFProperty(ioAccelerator, CFSTR("gpu-core-count"), kCFAllocatorDefault, 0));

    if (coreCountRef) {
        core_count = safe_cfnumber_to_int64(coreCountRef).value_or(0);
        CFRelease(coreCountRef);
    }

    io_service_class_interator("AppleARMIODevice",apple_arm_io_device_interator_callback, this);

    lookup(ioAccelerator);

    if (IOReport::lib_handle) {
        //? Open the channels
        CFStringRef gpu_stats_group = CFStringCreateWithCString(kCFAllocatorDefault, "GPU Stats", kCFStringEncodingUTF8);
        CFDictionaryRef gpu_stats_channels = IOReport::CopyChannelsInGroup(gpu_stats_group, nullptr, 0, 0, 0);
        CFRelease(gpu_stats_group);

        if (gpu_stats_channels == nullptr) {
            return;
        }

        CFStringRef energy_group = CFStringCreateWithCString(kCFAllocatorDefault, "Energy Model", kCFStringEncodingUTF8);
        CFDictionaryRef energy_channels = IOReport::CopyChannelsInGroup(energy_group, nullptr, 0, 0, 0);
        CFRelease(energy_group);

        if (energy_channels == nullptr) {
            CFRelease(gpu_stats_channels);
            return;
        }

        //? Merge channels
        channels = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, gpu_stats_channels);

        IOReport::MergeChannels(channels, energy_channels, nullptr);
        CFRelease(gpu_stats_channels);
        CFRelease(energy_channels);

        //? Create subscription
        CFMutableDictionaryRef sub_channels = nullptr;
        subscription = IOReport::CreateSubscription(nullptr, channels, &sub_channels, 0, nullptr);

        if (subscription == nullptr) {
            return;
        }

        //? Take Initial Sample
        prevSample = IOReport::CreateSamples(subscription, channels, nullptr);
        CFRelease(sub_channels);
        prev_sample_time = get_time_ns();
    }
    lookup_process_percentage();
    return;
}

//? Processes the content received from IOReport
void GPU::parser_channels(CFDictionaryRef delta, double elapsedSeconds) {
    CFArrayRef channel_array = (CFArrayRef)CFDictionaryGetValue(delta, CFSTR("IOReportChannels"));
    if (channel_array == nullptr or
        CFGetTypeID(channel_array) != CFArrayGetTypeID()) {
        return;
    }

    uint64_t n_joule = 0;
    //? Temperature accumulation
    double temp_sum = 0.0;
    int64_t temp_count = 0;

    //? Map unit to nanojoules conversion factor
    static const std::unordered_map<std::string, long long> unit_to_nj {
        {"mJ", 1'000'000},  // 1 mJ = 1e6 nJ
        {"uJ", 1'000},      // 1 µJ = 1e3 nJ
        {"nJ", 1}           // 1 nJ = 1 nJ
    };

    CFIndex count = CFArrayGetCount(channel_array);

    for (CFIndex i = 0; i < count; i++) {
        CFDictionaryRef channel = (CFDictionaryRef)CFArrayGetValueAtIndex(channel_array, i);

        if (!channel or CFGetTypeID(channel) != CFDictionaryGetTypeID()) {
            continue;
        }

        //? It's possible to check the driver name to see if it matches the
        // IOClass of the GPU Class in IOKit; there's an ID at the end, but it's
        // negligible.
        auto driver_name = safe_cfstring_to_std_string(IOReport::ChannelGetDriverName(channel)).value_or("");

        //? Filter driver name on Channel values
        if (driver_name.find(driver) == std::string::npos) { 
            continue;
        }

        auto group = safe_cfstring_to_std_string(IOReport::ChannelGetGroup(channel)).value_or("");
        auto subgroup = safe_cfstring_to_std_string(IOReport::ChannelGetSubGroup(channel)).value_or("");
        auto channel_name = safe_cfstring_to_std_string(IOReport::ChannelGetChannelName(channel)).value_or("");

        //? GPU Performance States
        if (group == "GPU Stats" and subgroup == "GPU Performance States" and channel_name == "GPUPH") {
            int32_t state_count = IOReport::StateGetCount(channel);
            int64_t total_time = 0, active_time = 0;
            int64_t weighted_freq = 0, weighted_volt = 0;

            //? For each item listed here, we have the time the chip was in that
            // mode and the position in the table where the chip was in its
            // energy state.
            for (int32_t s = 0; s < state_count; s++) {
                auto state_name = safe_cfstring_to_std_string(IOReport::StateGetNameForIndex(channel, s)).value_or("");

                int64_t residency_ns = IOReport::StateGetResidency(channel, s);
                total_time += residency_ns;

                if (state_name.empty() or state_name == "OFF" or state_name == "IDLE") {
                    continue;
                }

                int64_t freq = 0, volt = 0;
                //? Based on that saved table, we can identify which clock speed
                // and voltage were currently being used.
                if (state_name[0] == 'P' and state_name.length() > 1) {
                    try {
                        int pstate_idx = std::stoi(state_name.substr(1)) - 1;
                        if (pstate_idx >= 0 and static_cast<size_t>(pstate_idx) < gpu_table.size()) {
                            freq = std::get<0>(gpu_table[pstate_idx]);
                            volt = std::get<1>(gpu_table[pstate_idx]);
                        }
                    } catch (...) {
                    }
                } else {
                    try {
                        freq = std::stoll(state_name);
                    } catch (...) {
                    }
                }

                if (freq > 0 and residency_ns > 0) {
                    weighted_freq += freq * residency_ns;
                    weighted_volt += volt * residency_ns;
                    active_time += residency_ns;
                }
            }

            //? Normalizes over time.
            if (active_time > 0) {
                statistics.gpu_frequency = weighted_freq / active_time;
                statistics.gpu_voltage = weighted_volt / active_time;
            }

            //? Normalizes over time. It generates more precise data, replacing
            // the data from the simple search in the class.
            if (total_time > 0) {
                double usage_percent = (static_cast<double>(active_time) / total_time) * 100.0;
                statistics.device_utilization = static_cast<int64_t>(std::clamp(usage_percent, 0.0, 100.0));
            }
        }

        //? Temperature
        if (group == "GPU Stats" and subgroup == "Temperature") {
            int64_t value = IOReport::SimpleGetIntegerValue(channel, 0);
            if (channel_name == "Average Sum") {
                temp_sum = static_cast<double>(value);
            }
            else if (channel_name == "Average Sum Count") {
                temp_count = value;
            }
        }

        //? GPU Energy
        if (group == "Energy Model" and channel_name.find("GPU Energy") != std::string::npos) {
            auto unit = safe_cfstring_to_std_string(IOReport::ChannelGetUnitLabel(channel)).value_or("");

            int64_t energy_value = IOReport::SimpleGetIntegerValue(channel, 0);

            long long factor = 1'000'000'000;  // default to 1 nJ = 1e-9 J
            if (unit_to_nj.count(unit)){
                factor = unit_to_nj.at(unit);
            }

            n_joule += energy_value * factor;
        }
    }

    //? Compute power in mW
    if (elapsedSeconds > 0 and n_joule > 0) {
        statistics.milliwatts = static_cast<int64_t>(static_cast<double>(n_joule) * 1e-6 / elapsedSeconds);
    }

    //? This does not work on my M4 Pro
    //? Calculate average temperature
    //? IOReport temperature values are in centiCelsius (hundredths of a degree)
    if (temp_count > 0 and temp_sum > 0) {
        statistics.temp_c = (temp_sum / static_cast<double>(temp_count)) / 100.0;
    }
}

//? Reloads the GPU class data
bool GPU::refresh() {
    //? With the path stored in memory, we can remove the entire GPU without
    // having to iterate through the children again.
    io_object_t ioAccelerator = IORegistryEntryFromPath(kIOMainPortDefault, io_path.c_str());

    if (ioAccelerator == IO_OBJECT_NULL) {
        return false;
    }

    lookup(ioAccelerator);
    IOObjectRelease(ioAccelerator);

    //? IO Report provide Clock, Wattage and Temperature (but the last not work)
    if (IOReport::lib_handle) {
        CFDictionaryRef currentSample = IOReport::CreateSamples(subscription, channels, nullptr);

        uint64_t currentTime = get_time_ns();
        double elapsedSeconds = (currentTime - prev_sample_time) / 1e9;

        CFDictionaryRef delta = IOReport::CreateSamplesDelta(prevSample, currentSample, nullptr);

        if (delta) {
            parser_channels(delta, elapsedSeconds);
            CFRelease(delta);
        }

        if (currentSample) {
            CFRelease(prevSample);
            prevSample = currentSample;
        }
        prev_sample_time = currentTime;

        //? If IOReport didn't provide temperature, fall back to IOHIDSensors
        if (statistics.temp_c <= 0.0) {
            statistics.temp_c = get_gpu_temperature();
        }
    }

    lookup_process_percentage();
    return true;
}

const std::unordered_map<pid_t, std::tuple<GPUActivities, uint64_t, double>> &GPU::get_activities() const {
    return actual_activities;
}

const GPU::PerformanceStatistics &GPU::get_statistics() const {
    return statistics;
}
const std::string &GPU::get_name() const { return name; }

const int64_t &GPU::get_core_count() const { return core_count; }

GPU::~GPU() {
    if (prevSample) { 
        CFRelease(prevSample);
    }

    if (channels){ 
        CFRelease(channels);
    }
}
// IOGPU

//? This callback should receive the self of the IOGPU class that called it, it
//? will create the entry for the GPU found
bool IOGPU::iterator_gpu_callback(io_object_t object, void *data) {
    auto *self = static_cast<IOGPU *>(data);
    self->gpus.emplace_back(object);
    return true;
}

//? In theory, if you do this on an Intel Mac with a dedicated GPU, it may show
//? the other accelerator, in which case there may be bugs as I haven't tested
//? it.
IOGPU::IOGPU() {
    IOReport::try_load();
    io_service_class_interator("IOAccelerator", iterator_gpu_callback, this);
}

std::vector<GPU> &IOGPU::get_gpus() { 
    return gpus; 
}
