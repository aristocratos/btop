#pragma once

// Include the original btop headers
#include "../../src/btop_shared.hpp"
#include "../../src/btop_tools.hpp"
#include "../../src/btop_config.hpp"

#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>

// We need to avoid conflicts with the main btop program
// So we'll define our own initialization that doesn't conflict

// Wrapper class to manage btop data collection for OpenGL visualization
class BtopGLCollector
{
public:
    // Singleton instance
    static BtopGLCollector &getInstance()
    {
        static BtopGLCollector instance;
        return instance;
    }

    // Initialize the collector (without conflicting with main btop)
    bool initialize();

    // Start data collection
    void start();

    // Stop data collection
    void stop();

    // Get CPU data (thread-safe copy)
    Cpu::cpu_info getCpuInfo();

    // Get Memory data (thread-safe copy)
    Mem::mem_info getMemInfo();

    // Get Network data (thread-safe copy)
    Net::net_info getNetInfo();

    // Get Process data (thread-safe copy)
    std::vector<Proc::proc_info> getProcInfo();

    // Check if data collection is running
    bool isRunning() const { return running.load(); }

    // Set update interval (seconds)
    void setUpdateInterval(float seconds) { update_interval = seconds; }

    // Get update interval
    float getUpdateInterval() const { return update_interval; }

private:
    BtopGLCollector() : running(false), update_interval(1.0f) {}
    ~BtopGLCollector() { stop(); }

    // Deleted copy/move constructors
    BtopGLCollector(const BtopGLCollector &) = delete;
    BtopGLCollector &operator=(const BtopGLCollector &) = delete;
    BtopGLCollector(BtopGLCollector &&) = delete;
    BtopGLCollector &operator=(BtopGLCollector &&) = delete;

    // Data collection thread function
    void collectLoop();

    // Thread management
    std::thread collector_thread;
    std::atomic<bool> running;
    float update_interval;

    // Mutexes for thread-safe access
    std::mutex cpu_mutex;
    std::mutex mem_mutex;
    std::mutex net_mutex;
    std::mutex proc_mutex;

    // Cached data
    Cpu::cpu_info cached_cpu_info;
    Mem::mem_info cached_mem_info;
    Net::net_info cached_net_info;
    std::vector<Proc::proc_info> cached_proc_info;

    bool initialized = false;
};

// Minimal initialization for OpenGL version
// This avoids the full btop initialization that includes signal handlers, etc.
void initBtopGL();