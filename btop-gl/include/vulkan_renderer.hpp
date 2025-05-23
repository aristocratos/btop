#pragma once

#define VK_ENABLE_BETA_EXTENSIONS
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <memory>
#include <chrono>
#include <string>
#include <vector>
#include <array>
#include <optional>

// #include "btop_shared.hpp" // Let btop_glue.hpp handle this include.

#include "../nanovg.h"
#include "../nanovg_vk.h"
#include "btop_glue.hpp" // This should bring in Proc::proc_info via its own includes

struct QueueFamilyIndices
{
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete()
    {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

struct SwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

class VulkanRenderer
{
public:
    VulkanRenderer(GLFWwindow *window, int width, int height);
    ~VulkanRenderer();

    // Initialize Vulkan and NanoVG
    bool initialize();

    // Main render loop
    void render();

    // Update data from collector
    void update();

    // Handle window resize
    void resize(int width, int height);

    // Get frame time for FPS display
    float getFrameTime() const { return frame_time; }

    // Mode system for cycling through different visualizations
    enum class VisualizationMode
    {
        COMBO_DASHBOARD = 0,
        CLASSIC_GRAPHS,
        CPU_CORES,
        MEMORY_LANDSCAPE,
        NETWORK_FLOW,
        PROCESS_RAIN,
        DISK_ACTIVITY,
        OVERVIEW_DASHBOARD,
        MODE_COUNT
    };

    void cycleMode()
    {
        current_mode = static_cast<VisualizationMode>((static_cast<int>(current_mode) + 1) % static_cast<int>(VisualizationMode::MODE_COUNT));
        mode_transition_time = 0.0f;
    }

    VisualizationMode getCurrentMode() const { return current_mode; }

private:
    // Window and dimensions
    GLFWwindow *window;
    int window_width, window_height;

    // Vulkan core objects
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkSurfaceKHR surface;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device;
    VkQueue graphicsQueue;
    VkQueue presentQueue;

    // Swap chain
    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    std::vector<VkImageView> swapChainImageViews;
    std::vector<VkFramebuffer> swapChainFramebuffers;

    // Render pass and command buffers
    VkRenderPass renderPass;
    std::vector<VkCommandBuffer> commandBuffers;
    VkCommandPool commandPool;

    // Synchronization objects
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    size_t currentFrame = 0;

    // NanoVG context
    NVGcontext *vg;

    // Animation and timing
    std::chrono::high_resolution_clock::time_point last_frame_time;
    float frame_time;
    float time_accumulator;

    // Data collector reference
    BtopGLCollector &collector;

    // Mode system
    VisualizationMode current_mode;
    float mode_transition_time;

    // Data storage
    std::vector<float> cpu_data;
    std::vector<float> memory_data;
    std::vector<float> network_recv_data;
    std::vector<float> network_send_data;
    std::vector<Proc::proc_info> process_list;
    size_t history_size;

    // Colors - beautiful neon palette
    struct Colors
    {
        NVGcolor cpu = nvgRGBA(0, 255, 102, 255);           // Neon green
        NVGcolor memory = nvgRGBA(255, 51, 153, 255);       // Hot pink
        NVGcolor network_recv = nvgRGBA(51, 204, 255, 255); // Cyan
        NVGcolor network_send = nvgRGBA(255, 153, 0, 255);  // Orange
        NVGcolor background = nvgRGBA(20, 20, 30, 255);     // Dark blue
        NVGcolor accent = nvgRGBA(204, 102, 255, 255);      // Purple
        NVGcolor text = nvgRGBA(255, 255, 102, 255);        // Bright yellow
    } colors;

    // Vulkan validation layers
    const std::vector<const char *> validationLayers = {
        "VK_LAYER_KHRONOS_validation"};

    // Required device extensions
    const std::vector<const char *> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    static const int MAX_FRAMES_IN_FLIGHT = 2;

// Debug mode
#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = false; // Disabled on macOS by default
#endif

    // Vulkan initialization methods
    void createInstance();
    void setupDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSwapChain();
    void createImageViews();
    void createRenderPass();
    void createFramebuffers();
    void createCommandPool();
    void createCommandBuffers();
    void createSyncObjects();

    // Helper methods
    bool checkValidationLayerSupport();
    std::vector<const char *> getRequiredExtensions();
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo);

    bool isDeviceSuitable(VkPhysicalDevice device);
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities);

    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void drawFrame();

    void cleanup();
    void cleanupSwapChain();
    void recreateSwapChain();

    // NanoVG initialization
    void initializeNanoVG();

    // Rendering methods for different modes
    void renderClassicGraphs();
    void renderCpuCores();
    void renderMemoryLandscape();
    void renderNetworkFlow();
    void renderProcessRain();
    void renderDiskActivity();
    void renderOverviewDashboard();
    void renderComboDashboard();

    // Helper methods
    void drawGraph(const std::vector<float> &data, float x, float y, float w, float h, NVGcolor color, const std::string &label);
    void drawText(const std::string &text, float x, float y, float size, NVGcolor color = {});
    void drawGlowingCircle(float x, float y, float radius, NVGcolor color, float intensity = 1.0f);
    void drawFlowingParticles(float x1, float y1, float x2, float y2, NVGcolor color, int count = 10);

    // Debug callback
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                        VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                        const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                                                        void *pUserData);
};