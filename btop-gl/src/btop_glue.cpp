#include "../include/btop_glue.hpp"
#include <iostream>

// Required by btop source files - provide minimal implementations
namespace Global
{
    std::string exit_error_msg;
    std::atomic<bool> resized{false};
    uid_t real_uid = getuid();
    uid_t set_uid = getuid();
    std::atomic<bool> init_conf{false};
    bool should_quit = false;
    bool debug = false;
    uint64_t start_time = 0;
    std::string overlay;
    std::string clock;

    // Define the Banner_src with the correct type: vector<array<string, 2>>
    const std::vector<std::array<std::string, 2>> Banner_src = {
        {"", "██████╗ ████████╗ ██████╗ ██████╗ "},
        {"", "██╔══██╗╚══██╔══╝██╔═══██╗██╔══██╗"},
        {"", "██████╔╝   ██║   ██║   ██║██████╔╝"},
        {"", "██╔══██╗   ██║   ██║   ██║██╔═══╝ "},
        {"", "██████╔╝   ██║   ╚██████╔╝██║     "},
        {"", "╚═════╝    ╚═╝    ╚═════╝ ╚═╝     "}};

    // Provide the Version string that's referenced
    const std::string Version = "1.4.3-gl";
}

namespace Runner
{
    std::atomic<bool> active{false};
    std::atomic<bool> stopping{false};
    bool pause_output = false; // Use bool as declared in btop_shared.hpp
    std::atomic<bool> redraw{false};

    // Function signature must match btop_shared.hpp: void run(const string& box="", bool no_update = false, bool force_redraw = false);
    void run(const std::string &box, bool no_update, bool force_redraw)
    {
        // For OpenGL version, we don't run the terminal-based runner
        // This is just a stub to satisfy the linker
    }
}

// Function implementations that are referenced
void clean_quit(int sig)
{
    // For OpenGL version, this is simplified
    std::cout << "Clean quit called with signal: " << sig << std::endl;
    if (sig != 0)
    {
        exit(sig);
    }
}

// Initialize btop components needed for data collection
void initBtopGL()
{
    // Initialize configuration first
    if (!Global::init_conf.load())
    {
        std::vector<std::string> load_warnings;
        Config::load(Config::conf_file, load_warnings);
        Global::init_conf.store(true);
    }

    // Set up basic globals
    Global::start_time = time(NULL);
}

bool BtopGLCollector::initialize()
{
    if (initialized)
    {
        return true;
    }

    try
    {
        // Initialize btop components
        initBtopGL();

        // Initialize shared components
        Shared::init();

        initialized = true;
        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Failed to initialize btop collector: " << e.what() << std::endl;
        return false;
    }
}

void BtopGLCollector::start()
{
    if (running.load())
    {
        return; // Already running
    }

    if (!initialized && !initialize())
    {
        throw std::runtime_error("Failed to initialize BtopGLCollector");
    }

    running.store(true);
    collector_thread = std::thread(&BtopGLCollector::collectLoop, this);
}

void BtopGLCollector::stop()
{
    if (!running.load())
    {
        return; // Not running
    }

    running.store(false);
    if (collector_thread.joinable())
    {
        collector_thread.join();
    }
}

void BtopGLCollector::collectLoop()
{
    while (running.load())
    {
        try
        {
            // Collect CPU data
            {
                std::lock_guard<std::mutex> lock(cpu_mutex);
                cached_cpu_info = Cpu::collect();
            }

            // Collect Memory data
            {
                std::lock_guard<std::mutex> lock(mem_mutex);
                cached_mem_info = Mem::collect();
            }

            // Collect Network data
            {
                std::lock_guard<std::mutex> lock(net_mutex);
                cached_net_info = Net::collect();
            }

            // Collect Process data
            {
                std::lock_guard<std::mutex> lock(proc_mutex);
                cached_proc_info = Proc::collect();
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error in data collection: " << e.what() << std::endl;
        }

        // Sleep for the specified interval
        std::this_thread::sleep_for(std::chrono::duration<float>(update_interval));
    }
}

Cpu::cpu_info BtopGLCollector::getCpuInfo()
{
    std::lock_guard<std::mutex> lock(cpu_mutex);
    return cached_cpu_info;
}

Mem::mem_info BtopGLCollector::getMemInfo()
{
    std::lock_guard<std::mutex> lock(mem_mutex);
    return cached_mem_info;
}

Net::net_info BtopGLCollector::getNetInfo()
{
    std::lock_guard<std::mutex> lock(net_mutex);
    return cached_net_info;
}

std::vector<Proc::proc_info> BtopGLCollector::getProcInfo()
{
    std::lock_guard<std::mutex> lock(proc_mutex);
    return cached_proc_info;
}