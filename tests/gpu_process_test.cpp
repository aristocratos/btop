// GPU Per-Process Usage Test for Apple Silicon
// This demonstrates how to get per-process GPU usage using IOKit
// Similar to what Activity Monitor uses internally

#include <iostream>
#include <iomanip>
#include <map>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <algorithm>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>

struct GpuClientInfo {
    pid_t pid;
    std::string process_name;
    uint64_t accumulated_gpu_time;  // nanoseconds
    uint64_t last_submitted_time;
};

// Helper to extract string from CFString
std::string cfstring_to_string(CFStringRef cf_str) {
    if (!cf_str) return "";

    CFIndex length = CFStringGetLength(cf_str);
    CFIndex max_size = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
    std::string result(max_size, '\0');

    if (CFStringGetCString(cf_str, &result[0], max_size, kCFStringEncodingUTF8)) {
        result.resize(strlen(result.c_str()));
        return result;
    }
    return "";
}

// Parse "pid XXX, processname" format
bool parse_creator_string(const std::string& creator, pid_t& pid, std::string& name) {
    // Format: "pid 1234, ProcessName"
    size_t pid_pos = creator.find("pid ");
    if (pid_pos == std::string::npos) return false;

    size_t comma_pos = creator.find(',', pid_pos);
    if (comma_pos == std::string::npos) return false;

    try {
        pid = std::stoi(creator.substr(pid_pos + 4, comma_pos - pid_pos - 4));
    } catch (...) {
        return false;
    }

    // Get process name (skip ", ")
    if (comma_pos + 2 < creator.length()) {
        name = creator.substr(comma_pos + 2);
    }

    return true;
}

// Extract accumulated GPU time from AppUsage array
uint64_t extract_gpu_time(CFArrayRef app_usage) {
    if (!app_usage) return 0;

    uint64_t total_time = 0;
    CFIndex count = CFArrayGetCount(app_usage);

    for (CFIndex i = 0; i < count; i++) {
        CFDictionaryRef usage_dict = (CFDictionaryRef)CFArrayGetValueAtIndex(app_usage, i);
        if (!usage_dict || CFGetTypeID(usage_dict) != CFDictionaryGetTypeID()) continue;

        CFNumberRef gpu_time_ref = (CFNumberRef)CFDictionaryGetValue(usage_dict, CFSTR("accumulatedGPUTime"));
        if (gpu_time_ref && CFGetTypeID(gpu_time_ref) == CFNumberGetTypeID()) {
            int64_t gpu_time = 0;
            CFNumberGetValue(gpu_time_ref, kCFNumberSInt64Type, &gpu_time);
            total_time += gpu_time;
        }
    }

    return total_time;
}

// Collect GPU client information from IORegistry by iterating accelerator children
std::map<pid_t, GpuClientInfo> collect_gpu_clients() {
    std::map<pid_t, GpuClientInfo> clients;

    // Find the AGXAccelerator (GPU device)
    CFMutableDictionaryRef matching = IOServiceMatching("IOAccelerator");
    if (!matching) {
        std::cerr << "Failed to create IOAccelerator matching dictionary" << std::endl;
        return clients;
    }

    io_iterator_t accel_iterator;
    kern_return_t kr = IOServiceGetMatchingServices(kIOMainPortDefault, matching, &accel_iterator);
    if (kr != KERN_SUCCESS) {
        std::cerr << "Failed to get IOAccelerator services: " << kr << std::endl;
        return clients;
    }

    io_service_t accelerator;
    while ((accelerator = IOIteratorNext(accel_iterator)) != 0) {
        // Get accelerator name for debugging
        io_name_t name;
        IORegistryEntryGetName(accelerator, name);
        std::cout << "Found accelerator: " << name << std::endl;

        // Get children (AGXDeviceUserClient instances)
        io_iterator_t child_iterator;
        kr = IORegistryEntryGetChildIterator(accelerator, kIOServicePlane, &child_iterator);
        if (kr != KERN_SUCCESS) {
            std::cerr << "Failed to get children: " << kr << std::endl;
            IOObjectRelease(accelerator);
            continue;
        }

        io_service_t child;
        int child_count = 0;
        while ((child = IOIteratorNext(child_iterator)) != 0) {
            child_count++;

            // Get the class name
            io_name_t class_name;
            IOObjectGetClass(child, class_name);

            // Only process AGXDeviceUserClient
            if (strcmp(class_name, "AGXDeviceUserClient") != 0) {
                IOObjectRelease(child);
                continue;
            }

            // Get properties of the user client
            CFMutableDictionaryRef properties = nullptr;
            kr = IORegistryEntryCreateCFProperties(child, &properties, kCFAllocatorDefault, 0);

            if (kr == KERN_SUCCESS && properties) {
                // Debug: print all keys
                if (child_count <= 3) {  // Only print first 3 for debugging
                    CFIndex key_count = CFDictionaryGetCount(properties);
                    std::cout << "  Child " << child_count << " (" << class_name << ") has " << key_count << " properties" << std::endl;

                    // Print property keys
                    CFArrayRef keys = nullptr;
                    keys = CFArrayCreateMutable(kCFAllocatorDefault, key_count, &kCFTypeArrayCallBacks);
                    CFDictionaryGetKeysAndValues(properties, nullptr, nullptr);

                    // Iterate properties
                    CFDictionaryApplyFunction(properties, [](const void* key, const void* value, void* context) {
                        CFStringRef key_str = (CFStringRef)key;
                        char key_buf[256];
                        if (CFStringGetCString(key_str, key_buf, sizeof(key_buf), kCFStringEncodingUTF8)) {
                            std::cout << "    Key: " << key_buf << std::endl;
                        }
                    }, nullptr);
                }

                // Get IOUserClientCreator
                CFStringRef creator_ref = (CFStringRef)CFDictionaryGetValue(properties, CFSTR("IOUserClientCreator"));
                if (creator_ref && CFGetTypeID(creator_ref) == CFStringGetTypeID()) {
                    std::string creator = cfstring_to_string(creator_ref);

                    pid_t pid;
                    std::string proc_name;
                    if (parse_creator_string(creator, pid, proc_name)) {
                        // Get AppUsage array
                        CFArrayRef app_usage = (CFArrayRef)CFDictionaryGetValue(properties, CFSTR("AppUsage"));
                        uint64_t gpu_time = 0;
                        if (app_usage && CFGetTypeID(app_usage) == CFArrayGetTypeID()) {
                            gpu_time = extract_gpu_time(app_usage);
                        }

                        // Aggregate by PID (a process may have multiple clients)
                        if (clients.find(pid) != clients.end()) {
                            clients[pid].accumulated_gpu_time += gpu_time;
                        } else {
                            clients[pid] = {pid, proc_name, gpu_time, 0};
                        }
                    }
                }

                CFRelease(properties);
            } else {
                if (child_count <= 3) {
                    std::cout << "  Child " << child_count << " (" << class_name << ") - no properties (kr=" << kr << ")" << std::endl;
                }
            }

            IOObjectRelease(child);
        }

        std::cout << "Total children: " << child_count << std::endl;
        IOObjectRelease(child_iterator);
        IOObjectRelease(accelerator);
    }

    IOObjectRelease(accel_iterator);
    return clients;
}

// Calculate GPU usage percentage
std::map<pid_t, double> calculate_gpu_usage(int sample_interval_ms = 1000) {
    std::map<pid_t, double> usage;

    // First sample
    auto sample1 = collect_gpu_clients();
    auto time1 = std::chrono::steady_clock::now();

    // Wait for sample interval
    std::this_thread::sleep_for(std::chrono::milliseconds(sample_interval_ms));

    // Second sample
    auto sample2 = collect_gpu_clients();
    auto time2 = std::chrono::steady_clock::now();

    // Calculate elapsed time in nanoseconds
    auto elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(time2 - time1).count();

    // Calculate usage for each process
    for (const auto& [pid, info2] : sample2) {
        auto it = sample1.find(pid);
        if (it != sample1.end()) {
            uint64_t delta = info2.accumulated_gpu_time - it->second.accumulated_gpu_time;
            // GPU time / elapsed time * 100 = percentage
            double percent = (double)delta / (double)elapsed_ns * 100.0;
            if (percent > 0.01) {  // Filter out tiny values
                usage[pid] = percent;
            }
        } else if (info2.accumulated_gpu_time > 0) {
            // New process, estimate based on accumulated time
            double percent = (double)info2.accumulated_gpu_time / (double)elapsed_ns * 100.0;
            if (percent > 0.01) {
                usage[pid] = percent;
            }
        }
    }

    return usage;
}

int main() {
    std::cout << "=== Apple Silicon Per-Process GPU Usage Test ===\n" << std::endl;

    // Get current GPU clients
    auto clients = collect_gpu_clients();

    std::cout << "\nGPU Clients Found: " << clients.size() << std::endl;
    std::cout << "----------------------------------------" << std::endl;

    if (clients.empty()) {
        std::cout << "No GPU clients found." << std::endl;
        std::cout << "\nThis could mean:" << std::endl;
        std::cout << "  - No processes are using the GPU" << std::endl;
        std::cout << "  - The IOKit properties might require elevated privileges" << std::endl;
        std::cout << "  - The API has changed on this macOS version" << std::endl;
        return 1;
    }

    // Sort by accumulated GPU time
    std::vector<std::pair<pid_t, GpuClientInfo>> sorted_clients(clients.begin(), clients.end());
    std::sort(sorted_clients.begin(), sorted_clients.end(),
              [](const auto& a, const auto& b) {
                  return a.second.accumulated_gpu_time > b.second.accumulated_gpu_time;
              });

    std::cout << "\nTop GPU Clients by Accumulated Time:" << std::endl;
    for (size_t i = 0; i < std::min(sorted_clients.size(), size_t(15)); i++) {
        const auto& info = sorted_clients[i].second;
        double time_sec = info.accumulated_gpu_time / 1e9;
        std::cout << "  PID " << info.pid << " (" << info.process_name << "): "
                  << time_sec << "s GPU time" << std::endl;
    }

    std::cout << "\n=== Test Complete ===" << std::endl;
    return 0;
}
