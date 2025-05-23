#include "../include/vulkan_renderer.hpp"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <set>
#include <cstring>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

VulkanRenderer::VulkanRenderer(GLFWwindow *window, int width, int height)
    : window(window), window_width(width), window_height(height),
      instance(VK_NULL_HANDLE), debugMessenger(VK_NULL_HANDLE), surface(VK_NULL_HANDLE),
      physicalDevice(VK_NULL_HANDLE), device(VK_NULL_HANDLE), graphicsQueue(VK_NULL_HANDLE), presentQueue(VK_NULL_HANDLE),
      swapChain(VK_NULL_HANDLE), renderPass(VK_NULL_HANDLE), commandPool(VK_NULL_HANDLE),
      vg(nullptr), frame_time(0.0f), time_accumulator(0.0f),
      collector(BtopGLCollector::getInstance()),
      current_mode(VisualizationMode::OVERVIEW_DASHBOARD), mode_transition_time(0.0f),
      history_size(100)
{
    last_frame_time = std::chrono::high_resolution_clock::now();

    // Initialize data vectors
    cpu_data.reserve(history_size);
    memory_data.reserve(history_size);
    network_recv_data.reserve(history_size);
    network_send_data.reserve(history_size);
}

VulkanRenderer::~VulkanRenderer()
{
    cleanup();
}

bool VulkanRenderer::initialize()
{
    try
    {
        createInstance();
        setupDebugMessenger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();
        createRenderPass();
        createFramebuffers();
        createCommandPool();
        createCommandBuffers();
        createSyncObjects();
        initializeNanoVG();

        std::cout << "Vulkan renderer initialized successfully!" << std::endl;
        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Failed to initialize Vulkan renderer: " << e.what() << std::endl;
        return false;
    }
}

void VulkanRenderer::createInstance()
{
    if (enableValidationLayers && !checkValidationLayerSupport())
    {
        throw std::runtime_error("validation layers requested, but not available!");
    }

    // Debug: Print available extensions
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());

    std::cout << "Available " << extensionCount << " extensions:" << std::endl;
    for (const auto &extension : availableExtensions)
    {
        std::cout << "  " << extension.extensionName << std::endl;
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "btop++ Vulkan";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "btop-gl";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    createInfo.pApplicationInfo = &appInfo;

    auto extensions = getRequiredExtensions();

    // Debug: Print requested extensions
    std::cout << "\nRequesting " << extensions.size() << " extensions:" << std::endl;
    for (const auto &ext : extensions)
    {
        std::cout << "  " << ext << std::endl;
    }

    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (enableValidationLayers)
    {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();

        populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT *)&debugCreateInfo;
    }
    else
    {
        createInfo.enabledLayerCount = 0;
        createInfo.pNext = nullptr;
    }

    VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
    if (result != VK_SUCCESS)
    {
        std::cerr << "vkCreateInstance failed with error code: " << result << std::endl;
        throw std::runtime_error("failed to create instance!");
    }
}

bool VulkanRenderer::checkValidationLayerSupport()
{
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char *layerName : validationLayers)
    {
        bool layerFound = false;

        for (const auto &layerProperties : availableLayers)
        {
            if (strcmp(layerName, layerProperties.layerName) == 0)
            {
                layerFound = true;
                break;
            }
        }

        if (!layerFound)
        {
            return false;
        }
    }

    return true;
}

std::vector<const char *> VulkanRenderer::getRequiredExtensions()
{
    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char *> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    // Add portability enumeration for macOS/MoltenVK
    extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);

    if (enableValidationLayers)
    {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanRenderer::debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                             VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                             const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                                                             void *pUserData)
{
    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
    return VK_FALSE;
}

void VulkanRenderer::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo)
{
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
}

void VulkanRenderer::setupDebugMessenger()
{
    if (!enableValidationLayers)
        return;

    VkDebugUtilsMessengerCreateInfoEXT createInfo;
    populateDebugMessengerCreateInfo(createInfo);

    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr)
    {
        func(instance, &createInfo, nullptr, &debugMessenger);
    }
}

void VulkanRenderer::createSurface()
{
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create window surface!");
    }
}

void VulkanRenderer::pickPhysicalDevice()
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    if (deviceCount == 0)
    {
        throw std::runtime_error("failed to find GPUs with Vulkan support!");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    for (const auto &device : devices)
    {
        if (isDeviceSuitable(device))
        {
            physicalDevice = device;
            break;
        }
    }

    if (physicalDevice == VK_NULL_HANDLE)
    {
        throw std::runtime_error("failed to find a suitable GPU!");
    }
}

bool VulkanRenderer::isDeviceSuitable(VkPhysicalDevice device)
{
    QueueFamilyIndices indices = findQueueFamilies(device);

    bool extensionsSupported = checkDeviceExtensionSupport(device);

    bool swapChainAdequate = false;
    if (extensionsSupported)
    {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    return indices.isComplete() && extensionsSupported && swapChainAdequate;
}

bool VulkanRenderer::checkDeviceExtensionSupport(VkPhysicalDevice device)
{
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

    for (const auto &extension : availableExtensions)
    {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

QueueFamilyIndices VulkanRenderer::findQueueFamilies(VkPhysicalDevice device)
{
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const auto &queueFamily : queueFamilies)
    {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

        if (presentSupport)
        {
            indices.presentFamily = i;
        }

        if (indices.isComplete())
        {
            break;
        }

        i++;
    }

    return indices;
}

void VulkanRenderer::createLogicalDevice()
{
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies)
    {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (enableValidationLayers)
    {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    }
    else
    {
        createInfo.enabledLayerCount = 0;
    }

    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create logical device!");
    }

    vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
    vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
}

SwapChainSupportDetails VulkanRenderer::querySwapChainSupport(VkPhysicalDevice device)
{
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

    if (formatCount != 0)
    {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

    if (presentModeCount != 0)
    {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

VkSurfaceFormatKHR VulkanRenderer::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats)
{
    for (const auto &availableFormat : availableFormats)
    {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return availableFormat;
        }
    }

    return availableFormats[0];
}

VkPresentModeKHR VulkanRenderer::chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes)
{
    for (const auto &availablePresentMode : availablePresentModes)
    {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            return availablePresentMode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanRenderer::chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities)
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        return capabilities.currentExtent;
    }
    else
    {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        VkExtent2D actualExtent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)};

        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

void VulkanRenderer::createSwapChain()
{
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
    {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
    uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    if (indices.graphicsFamily != indices.presentFamily)
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create swap chain!");
    }

    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
    swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = extent;
}

void VulkanRenderer::createImageViews()
{
    swapChainImageViews.resize(swapChainImages.size());

    for (size_t i = 0; i < swapChainImages.size(); i++)
    {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = swapChainImages[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = swapChainImageFormat;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create image views!");
        }
    }
}

void VulkanRenderer::createRenderPass()
{
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapChainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create render pass!");
    }
}

void VulkanRenderer::createFramebuffers()
{
    swapChainFramebuffers.resize(swapChainImageViews.size());

    for (size_t i = 0; i < swapChainImageViews.size(); i++)
    {
        VkImageView attachments[] = {
            swapChainImageViews[i]};

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = swapChainExtent.width;
        framebufferInfo.height = swapChainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create framebuffer!");
        }
    }
}

void VulkanRenderer::createCommandPool()
{
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create command pool!");
    }
}

void VulkanRenderer::createCommandBuffers()
{
    commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();

    if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate command buffers!");
    }
}

void VulkanRenderer::createSyncObjects()
{
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
    }
}

void VulkanRenderer::initializeNanoVG()
{
    // Create NanoVG Vulkan context
    VKNVGCreateInfo createInfo{};
    createInfo.gpu = physicalDevice;
    createInfo.device = device;
    createInfo.renderpass = renderPass;
    createInfo.cmdBuffer = &commandBuffers[currentFrame];
    createInfo.swapchainImageCount = static_cast<uint32_t>(swapChainImages.size());
    createInfo.currentFrame = reinterpret_cast<uint32_t *>(&currentFrame);
    createInfo.allocator = nullptr;
    createInfo.ext = {}; // Use default extensions

    vg = nvgCreateVk(createInfo, NVG_ANTIALIAS | NVG_STENCIL_STROKES, graphicsQueue);
    if (!vg)
    {
        throw std::runtime_error("Failed to create NanoVG Vulkan context");
    }

    // Load a default font (replace with your actual font path if needed)
    // For macOS, common system font paths are in /System/Library/Fonts/ or /Library/Fonts/
    // We'll try a common sans-serif font. This assumes it's available.
    // If this path is not correct, you might need to copy a .ttf file to your project and use that path.
    int font_id = nvgCreateFont(vg, "sans", "/System/Library/Fonts/Helvetica.ttc");
    if (font_id == -1)
    {
        // Fallback to another common font if Helvetica is not found (common on some systems)
        font_id = nvgCreateFont(vg, "sans", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
        if (font_id == -1)
        {
            std::cerr << "Failed to load default font for NanoVG. Text will not be visible." << std::endl;
        }
        else
        {
            std::cout << "NanoVG: Loaded DejaVuSans.ttf" << std::endl;
        }
    }
    else
    {
        std::cout << "NanoVG: Loaded Helvetica.ttc" << std::endl;
    }

    // Set a default font for NanoVG to use if not specified in drawText
    // This is crucial if drawText calls don't specify a font family.
    nvgFontFace(vg, "sans");

    std::cout << "NanoVG Vulkan context created successfully!" << std::endl;
}

// Placeholder implementations for visualization methods (same as OpenGL version but adapted for Vulkan context)
void VulkanRenderer::update()
{
    // Update timing
    auto current_time = std::chrono::high_resolution_clock::now();
    frame_time = std::chrono::duration<float>(current_time - last_frame_time).count();
    last_frame_time = current_time;
    time_accumulator += frame_time;

    // Get latest data from collector (same as before)
    auto cpu_info = collector.getCpuInfo();
    auto mem_info = collector.getMemInfo();
    auto net_info = collector.getNetInfo();
    std::vector<Proc::proc_info> proc_info_list_from_collector = collector.getProcInfo(); // Fetch process info

    // Update CPU data
    if (!cpu_info.cpu_percent.empty() && !cpu_info.cpu_percent.at("total").empty())
    {
        float cpu_total = static_cast<float>(cpu_info.cpu_percent.at("total").back());
        cpu_data.push_back(cpu_total);
    }
    else
    {
        float test_cpu = 30.0f + 20.0f * sin(time_accumulator * 0.5f);
        cpu_data.push_back(test_cpu);
    }

    // Update Memory data
    if (mem_info.stats.count("used") && mem_info.stats.count("available"))
    {
        uint64_t used = mem_info.stats.at("used");
        uint64_t total = used + mem_info.stats.at("available");
        float mem_percent = total > 0 ? (static_cast<float>(used) / total * 100.0f) : 0.0f;
        memory_data.push_back(mem_percent);
    }
    else
    {
        float test_mem = 45.0f + 15.0f * cos(time_accumulator * 0.3f);
        memory_data.push_back(test_mem);
    }

    // Update Network data
    if (!net_info.bandwidth.empty())
    {
        if (net_info.bandwidth.count("download") && !net_info.bandwidth.at("download").empty())
        {
            float recv_speed = static_cast<float>(net_info.bandwidth.at("download").back());
            network_recv_data.push_back(recv_speed / 1024.0f);
        }
        else
        {
            float test_recv = 1024.0f + 512.0f * sin(time_accumulator * 0.8f);
            network_recv_data.push_back(test_recv);
        }

        if (net_info.bandwidth.count("upload") && !net_info.bandwidth.at("upload").empty())
        {
            float send_speed = static_cast<float>(net_info.bandwidth.at("upload").back());
            network_send_data.push_back(send_speed / 1024.0f);
        }
        else
        {
            float test_send = 256.0f + 128.0f * cos(time_accumulator * 1.2f);
            network_send_data.push_back(test_send);
        }
    }
    else
    {
        float test_recv = 1024.0f + 512.0f * sin(time_accumulator * 0.8f);
        float test_send = 256.0f + 128.0f * cos(time_accumulator * 1.2f);
        network_recv_data.push_back(test_recv);
        network_send_data.push_back(test_send);
    }

    // Update Process list (take top 10 by CPU usage for now)
    process_list.clear();
    if (!proc_info_list_from_collector.empty())
    {
        std::sort(proc_info_list_from_collector.begin(), proc_info_list_from_collector.end(),
                  [](const Proc::proc_info &a, const Proc::proc_info &b) -> bool
                  {
                      return a.cpu_p > b.cpu_p;
                  });
        for (size_t i = 0; i < std::min(proc_info_list_from_collector.size(), (size_t)10); ++i)
        {
            process_list.push_back(proc_info_list_from_collector[i]);
        }
    }

    // Maintain history size
    if (cpu_data.size() > history_size)
        cpu_data.erase(cpu_data.begin());
    if (memory_data.size() > history_size)
        memory_data.erase(memory_data.begin());
    if (network_recv_data.size() > history_size)
        network_recv_data.erase(network_recv_data.begin());
    if (network_send_data.size() > history_size)
        network_send_data.erase(network_send_data.begin());
}

void VulkanRenderer::render()
{
    drawFrame();
}

void VulkanRenderer::drawFrame()
{
    vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        recreateSwapChain();
        return;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    vkResetFences(device, 1, &inFlightFences[currentFrame]);

    vkResetCommandBuffer(commandBuffers[currentFrame], 0);
    recordCommandBuffer(commandBuffers[currentFrame], imageIndex);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

    VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to submit draw command buffer!");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {swapChain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(presentQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        recreateSwapChain();
    }
    else if (result != VK_SUCCESS)
    {
        throw std::runtime_error("failed to present swap chain image!");
    }

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanRenderer::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapChainExtent;

    VkClearValue clearColor = {{{0.08f, 0.08f, 0.12f, 1.0f}}};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    if (vg)
    {
        // Auto-cycle disabled: mode changes only on user request (press '?')

        // Begin NanoVG frame
        nvgBeginFrame(vg, window_width, window_height, 1.0f);

        // Clear background with dark color
        nvgBeginPath(vg);
        nvgRect(vg, 0, 0, window_width, window_height);
        nvgFillColor(vg, colors.background);
        nvgFill(vg);

        // Title and mode indicator
        std::string mode_name;
        switch (current_mode)
        {
        case VisualizationMode::COMBO_DASHBOARD:
            mode_name = "COMBO DASHBOARD";
            break;
        case VisualizationMode::CLASSIC_GRAPHS:
            mode_name = "CLASSIC GRAPHS";
            break;
        case VisualizationMode::CPU_CORES:
            mode_name = "CPU CORES";
            break;
        case VisualizationMode::MEMORY_LANDSCAPE:
            mode_name = "MEMORY LANDSCAPE";
            break;
        case VisualizationMode::NETWORK_FLOW:
            mode_name = "NETWORK FLOW";
            break;
        case VisualizationMode::PROCESS_RAIN:
            mode_name = "PROCESS RAIN";
            break;
        case VisualizationMode::DISK_ACTIVITY:
            mode_name = "DISK ACTIVITY";
            break;
        case VisualizationMode::OVERVIEW_DASHBOARD:
            mode_name = "OVERVIEW";
            break;
        default:
            mode_name = "UNKNOWN";
            break;
        }

        drawText("BTOP++ VULKAN - " + mode_name, 20, 40, 32, colors.text);

        // Render based on current mode
        switch (current_mode)
        {
        case VisualizationMode::COMBO_DASHBOARD:
            renderComboDashboard();
            break;
        case VisualizationMode::CLASSIC_GRAPHS:
            renderClassicGraphs();
            break;
        case VisualizationMode::CPU_CORES:
            renderCpuCores();
            break;
        case VisualizationMode::MEMORY_LANDSCAPE:
            renderMemoryLandscape();
            break;
        case VisualizationMode::NETWORK_FLOW:
            renderNetworkFlow();
            break;
        case VisualizationMode::PROCESS_RAIN:
            renderProcessRain();
            break;
        case VisualizationMode::DISK_ACTIVITY:
            renderDiskActivity();
            break;
        case VisualizationMode::OVERVIEW_DASHBOARD:
            renderOverviewDashboard();
            break;
        }

        nvgEndFrame(vg);
    }

    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to record command buffer!");
    }
}

// Visualization methods (copy from NanoVG version)
void VulkanRenderer::renderClassicGraphs()
{
    float margin = 40;
    float graph_width = (window_width - margin * 4) / 3;
    float graph_height = 200;
    float y_pos = 100;

    drawGraph(cpu_data, margin, y_pos, graph_width, graph_height, colors.cpu, "CPU");
    drawGraph(memory_data, margin * 2 + graph_width, y_pos, graph_width, graph_height, colors.memory, "MEMORY");
    drawGraph(network_recv_data, margin * 3 + graph_width * 2, y_pos, graph_width, graph_height, colors.network_recv, "NETWORK IN");
    drawGraph(network_send_data, margin * 3 + graph_width * 2, y_pos + graph_height / 2, graph_width, graph_height / 2, colors.network_send, "NETWORK OUT");
}

void VulkanRenderer::renderCpuCores()
{
    float center_x = window_width / 2.0f;
    float center_y = window_height / 2.0f;
    float radius = 150.0f;
    int core_count = 8;

    drawText("CPU CORES", center_x - 80, center_y - 200, 32, colors.text);

    for (int i = 0; i < core_count; ++i)
    {
        float angle = (2.0f * M_PI * i) / core_count;
        float core_x = center_x + cos(angle) * radius;
        float core_y = center_y + sin(angle) * radius;

        float core_usage = 30.0f + 40.0f * sin(time_accumulator * 0.3f + i * 0.5f);
        float intensity = core_usage / 100.0f;

        drawGlowingCircle(core_x, core_y, 20 + intensity * 15, colors.cpu, intensity);
        drawText("C" + std::to_string(i), core_x - 8, core_y + 50, 16, colors.text);
        drawText(std::to_string(static_cast<int>(core_usage)) + "%", core_x - 15, core_y + 70, 14, colors.cpu);
    }
}

void VulkanRenderer::renderMemoryLandscape()
{
    float margin = 50;
    float landscape_width = window_width - margin * 2;
    float landscape_height = 300;
    float base_y = window_height - 200;

    float mem_value = memory_data.empty() ? 45.0f : memory_data.back();

    drawText("MEMORY LANDSCAPE", margin, 120, 32, colors.text);

    float used_height = (mem_value / 100.0f) * landscape_height * 0.6f;
    float cache_height = landscape_height * 0.3f;
    float free_height = landscape_height - used_height - cache_height;

    // Used memory
    nvgBeginPath(vg);
    nvgRect(vg, margin, base_y - used_height, landscape_width, used_height);
    NVGpaint bg = nvgLinearGradient(vg, margin, base_y - used_height, margin, base_y,
                                    nvgRGBA(255, 51, 153, 200), nvgRGBA(255, 51, 153, 100));
    nvgFillPaint(vg, bg);
    nvgFill(vg);

    // Cache layer
    nvgBeginPath(vg);
    nvgRect(vg, margin, base_y - used_height - cache_height, landscape_width, cache_height);
    nvgFillColor(vg, nvgRGBA(51, 204, 255, 150));
    nvgFill(vg);

    // Free memory
    nvgBeginPath(vg);
    nvgRect(vg, margin, base_y - landscape_height, landscape_width, free_height);
    nvgFillColor(vg, nvgRGBA(204, 102, 255, 100));
    nvgFill(vg);

    drawText("USED: " + std::to_string(static_cast<int>(mem_value)) + "%", margin + 20, base_y - used_height / 2, 18, colors.memory);
    drawText("CACHE", margin + 20, base_y - used_height - cache_height / 2, 16, colors.network_recv);
    drawText("FREE", margin + 20, base_y - landscape_height + free_height / 2, 16, colors.accent);
}

void VulkanRenderer::renderNetworkFlow()
{
    drawText("NETWORK FLOW", 40, 120, 32, colors.text);

    float left_x = 100;
    float right_x = window_width - 100;

    // Download flow
    for (int i = 0; i < 15; ++i)
    {
        float progress = fmod(time_accumulator * 0.3f + i * 0.2f, 1.0f);
        float x = left_x + (right_x - left_x) * 0.3f;
        float y = 150 + progress * 300;
        drawGlowingCircle(x, y, 5 + 3 * sin(time_accumulator * 2 + i), colors.network_recv, 1.0f - progress * 0.5f);
    }

    // Upload flow
    for (int i = 0; i < 10; ++i)
    {
        float progress = fmod(time_accumulator * 0.4f + i * 0.3f, 1.0f);
        float x = right_x - (right_x - left_x) * 0.3f;
        float y = 450 - progress * 300;
        drawGlowingCircle(x, y, 4 + 2 * sin(time_accumulator * 3 + i), colors.network_send, 1.0f - progress * 0.5f);
    }

    float recv_value = network_recv_data.empty() ? 1024.0f : network_recv_data.back();
    float send_value = network_send_data.empty() ? 256.0f : network_send_data.back();

    drawText("DOWNLOAD", 50, 200, 24, colors.network_recv);
    drawText(std::to_string(static_cast<int>(recv_value)) + " KB/s", 50, 230, 20, colors.network_recv);
    drawText("UPLOAD", window_width - 150, 400, 24, colors.network_send);
    drawText(std::to_string(static_cast<int>(send_value)) + " KB/s", window_width - 150, 430, 20, colors.network_send);
}

void VulkanRenderer::renderProcessRain()
{
    drawText("PROCESS RAIN", 40, 120, 32, colors.text);

    std::string chars = "0123456789ABCDEF";
    int stream_count = 20;

    for (int stream = 0; stream < stream_count; ++stream)
    {
        float x = 50 + stream * (window_width - 100) / stream_count;
        float stream_speed = 100.0f + (stream % 3) * 50.0f;
        float stream_offset = fmod(time_accumulator * stream_speed, window_height + 200);

        for (int i = 0; i < 15; ++i)
        {
            float y = stream_offset - i * 30;
            if (y < 0 || y > window_height)
                continue;

            float intensity = std::max(0.0f, std::min(1.0f, (window_height - y) / window_height));
            int char_index = (stream * 7 + i * 3 + static_cast<int>(time_accumulator * 10)) % chars.length();
            std::string char_str(1, chars[char_index]);

            NVGcolor fade_color = nvgRGBA(0, 255, 102, static_cast<int>(255 * intensity));
            drawText(char_str, x, y, 16, fade_color);
        }
    }
}

void VulkanRenderer::renderDiskActivity()
{
    drawText("DISK ACTIVITY", 40, 120, 32, colors.text);

    float center_x = window_width / 2.0f;
    float center_y = window_height / 2.0f;
    float disk_radius = 100.0f;

    nvgBeginPath(vg);
    nvgCircle(vg, center_x, center_y, disk_radius);
    nvgStrokeColor(vg, colors.accent);
    nvgStrokeWidth(vg, 3.0f);
    nvgStroke(vg);

    int spoke_count = 8;
    for (int i = 0; i < spoke_count; ++i)
    {
        float angle = (2.0f * M_PI * i) / spoke_count + time_accumulator * 2.0f;
        float inner_radius = 20.0f;
        float outer_radius = disk_radius * (0.7f + 0.3f * sin(time_accumulator * 3.0f + i));

        float x1 = center_x + cos(angle) * inner_radius;
        float y1 = center_y + sin(angle) * inner_radius;
        float x2 = center_x + cos(angle) * outer_radius;
        float y2 = center_y + sin(angle) * outer_radius;

        nvgBeginPath(vg);
        nvgMoveTo(vg, x1, y1);
        nvgLineTo(vg, x2, y2);
        nvgStrokeColor(vg, colors.accent);
        nvgStrokeWidth(vg, 2.0f);
        nvgStroke(vg);
    }

    drawText("DISK I/O", center_x - 40, center_y + 10, 18, colors.text);
}

void VulkanRenderer::renderOverviewDashboard()
{
    drawText("SYSTEM OVERVIEW", 40, 50, 32, colors.text);

    // Summary text
    double uptime = Tools::system_uptime();
    int hours = static_cast<int>(uptime / 3600);
    int minutes = (static_cast<int>(uptime) % 3600) / 60;
    char textbuf[64];
    snprintf(textbuf, sizeof(textbuf), "Uptime: %dh %dm", hours, minutes);
    drawText(textbuf, 40, 90, 18, colors.text);

    auto cpu_info = collector.getCpuInfo();
    auto load_avg = cpu_info.load_avg;
    snprintf(textbuf, sizeof(textbuf), "Load Avg: %.2f %.2f %.2f", load_avg[0], load_avg[1], load_avg[2]);
    drawText(textbuf, 40, 110, 18, colors.text);

    float margin = 40.0f;
    float spacing = 20.0f;
    float top_y = 100.0f;
    float bottom_y = window_height / 2.0f + spacing / 2.0f;

    float cell_width = (window_width - margin * 2 - spacing) / 2.0f;
    float cell_height = (window_height - top_y - margin - spacing) / 2.0f;

    // --- Top Row ---

    // Mini CPU cores (Top-Left)
    float cpu_cell_x = margin;
    float cpu_cell_y = top_y;
    float mini_center_x_cpu = cpu_cell_x + cell_width / 2.0f;
    float mini_center_y_cpu = cpu_cell_y + cell_height / 2.0f - 20.0f; // Shift up for label

    drawText("CPU", cpu_cell_x + 10, cpu_cell_y + 10, 20, colors.text);
    for (int i = 0; i < 4; ++i)
    {
        float angle = (2.0f * M_PI * i) / 4.0f + time_accumulator * 0.5f; // Add rotation
        float core_x = mini_center_x_cpu + cos(angle) * 40;
        float core_y = mini_center_y_cpu + sin(angle) * 40;

        float core_usage = cpu_data.empty() ? (30.0f + 40.0f * sin(time_accumulator * 0.3f + i * 0.5f)) : cpu_data.back();
        core_usage = std::clamp(core_usage, 0.0f, 100.0f);
        float intensity = core_usage / 100.0f;

        drawGlowingCircle(core_x, core_y, 8 + intensity * 5, colors.cpu, intensity);
    }
    std::string cpu_val_str = cpu_data.empty() ? "N/A" : std::to_string(static_cast<int>(cpu_data.back())) + "%";
    drawText(cpu_val_str, mini_center_x_cpu - 20, mini_center_y_cpu + 60, 22, colors.cpu);

    // Mini memory bars (Top-Right)
    float mem_cell_x = margin + cell_width + spacing;
    float mem_cell_y = top_y;
    float mem_bar_x = mem_cell_x + 20;
    float mem_bar_y = mem_cell_y + cell_height / 2.0f - 10.0f; // Center bar vertically
    float mem_bar_width = cell_width - 40;
    float mem_bar_height = 40;
    float mem_value = memory_data.empty() ? 45.0f : memory_data.back();
    mem_value = std::clamp(mem_value, 0.0f, 100.0f);

    drawText("MEMORY", mem_cell_x + 10, mem_cell_y + 10, 20, colors.text);
    nvgBeginPath(vg);
    nvgRect(vg, mem_bar_x, mem_bar_y, mem_bar_width, mem_bar_height);
    nvgFillColor(vg, nvgRGBA(colors.memory.r, colors.memory.g, colors.memory.b, 100));
    nvgFill(vg);

    nvgBeginPath(vg);
    nvgRect(vg, mem_bar_x, mem_bar_y, mem_bar_width * (mem_value / 100.0f), mem_bar_height);
    nvgFillColor(vg, colors.memory);
    nvgFill(vg);
    std::string mem_val_str = std::to_string(static_cast<int>(mem_value)) + "%";
    drawText(mem_val_str, mem_bar_x + mem_bar_width / 2.0f - 20, mem_bar_y + mem_bar_height + 5, 22, colors.memory);

    // --- Bottom Row ---

    // Mini network flow (Bottom-Left)
    float net_cell_x = margin;
    float net_cell_y = bottom_y;
    float net_flow_x = net_cell_x + cell_width / 2.0f;
    float net_flow_y_start = net_cell_y + cell_height * 0.3f;
    float net_flow_y_end = net_cell_y + cell_height * 0.7f;

    drawText("NETWORK", net_cell_x + 10, net_cell_y + 10, 20, colors.text);
    drawFlowingParticles(net_flow_x - 30, net_flow_y_start, net_flow_x - 30, net_flow_y_end, colors.network_recv, 7);
    drawFlowingParticles(net_flow_x + 30, net_flow_y_end, net_flow_x + 30, net_flow_y_start, colors.network_send, 5); // Reversed direction for upload

    float recv_kb = network_recv_data.empty() ? 0.0f : network_recv_data.back();
    float send_kb = network_send_data.empty() ? 0.0f : network_send_data.back();
    std::string net_recv_str = "IN: " + std::to_string(static_cast<int>(recv_kb)) + " KB/s";
    std::string net_send_str = "OUT: " + std::to_string(static_cast<int>(send_kb)) + " KB/s";
    drawText(net_recv_str, net_cell_x + 20, net_cell_y + cell_height - 50, 18, colors.network_recv);
    drawText(net_send_str, net_cell_x + 20, net_cell_y + cell_height - 25, 18, colors.network_send);

    // Mini Disk Activity (Bottom-Right)
    float disk_cell_x = margin + cell_width + spacing;
    float disk_cell_y = bottom_y;
    float disk_center_x = disk_cell_x + cell_width / 2.0f;
    float disk_center_y = disk_cell_y + cell_height / 2.0f - 10.0f;
    float disk_radius_outer = cell_height * 0.3f;
    float disk_radius_inner = cell_height * 0.15f;

    drawText("DISK I/O", disk_cell_x + 10, disk_cell_y + 10, 20, colors.text);
    // Simulate disk activity with a pulsing outer ring
    float disk_activity_pulse = 0.8f + 0.2f * sin(time_accumulator * 5.0f);
    nvgBeginPath(vg);
    nvgCircle(vg, disk_center_x, disk_center_y, disk_radius_outer * disk_activity_pulse);
    nvgFillColor(vg, nvgRGBA(colors.accent.r, colors.accent.g, colors.accent.b, 150));
    nvgFill(vg);

    nvgBeginPath(vg);
    nvgCircle(vg, disk_center_x, disk_center_y, disk_radius_inner);
    nvgFillColor(vg, colors.accent);
    nvgFill(vg);
    drawText("Activity...", disk_center_x - 45, disk_center_y + disk_radius_outer + 10, 18, colors.accent);
}

// New Combo Dashboard Implementation
void VulkanRenderer::renderComboDashboard()
{
    // Title for the mode
    drawText("COMBO DASHBOARD", window_width / 2.0f - 150, 120, 32, colors.text);

    // Placeholder: Display top 3 processes as a starting point
    float p_y = 200.0f;
    float p_x = 50.0f;
    drawText("Top Processes:", p_x, p_y, 20, colors.accent);
    p_y += 30;

    nvgFontSize(vg, 16);
    nvgFillColor(vg, colors.text);
    char text_buffer[256];

    for (size_t i = 0; i < std::min(process_list.size(), (size_t)5); ++i)
    {
        const auto &p = process_list[i];
        // Trying p.mem (common member name for memory in KiB or bytes)
        snprintf(text_buffer, sizeof(text_buffer), "PID: %-6d %-20.20s CPU: %5.1f%% MEM: %7lu KB",
                 p.pid, p.name.c_str(), p.cpu_p, p.mem);
        drawText(text_buffer, p_x, p_y + (i * 20.0f), 16, colors.text);
    }

    // Placeholder for other elements like mini graphs - can be added later
    // For example, draw a mini CPU graph:
    if (!cpu_data.empty())
    {
        drawGraph(cpu_data, window_width - 250, 200, 200, 100, colors.cpu, "CPU");
    }
    if (!memory_data.empty())
    {
        drawGraph(memory_data, window_width - 250, 320, 200, 100, colors.memory, "Memory");
    }
}

// Helper methods implementation
void VulkanRenderer::drawGraph(const std::vector<float> &data, float x, float y, float w, float h, NVGcolor color, const std::string &label)
{
    if (data.empty())
        return;

    nvgBeginPath(vg);
    nvgRect(vg, x, y, w, h);
    nvgFillColor(vg, nvgRGBA(0, 0, 0, 100));
    nvgFill(vg);

    float max_val = *std::max_element(data.begin(), data.end());
    if (max_val < 10.0f)
        max_val = 10.0f;

    nvgBeginPath(vg);
    for (size_t i = 0; i < data.size(); ++i)
    {
        float px = x + (i * w) / (data.size() - 1);
        float py = y + h - (data[i] / max_val * h);

        if (i == 0)
            nvgMoveTo(vg, px, py);
        else
            nvgLineTo(vg, px, py);
    }
    nvgStrokeColor(vg, color);
    nvgStrokeWidth(vg, 3.0f);
    nvgStroke(vg);

    drawText(label, x, y - 10, 14, color);

    if (!data.empty())
    {
        std::string value_str = std::to_string(static_cast<int>(data.back())) + "%";
        drawText(value_str, x + w - 50, y - 10, 14, color);
    }
}

void VulkanRenderer::drawText(const std::string &text, float x, float y, float size, NVGcolor color)
{
    if (color.a == 0)
        color = colors.text;

    nvgFontSize(vg, size);
    nvgFillColor(vg, color);
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    nvgText(vg, x, y, text.c_str(), nullptr);
}

void VulkanRenderer::drawGlowingCircle(float x, float y, float radius, NVGcolor color, float intensity)
{
    nvgBeginPath(vg);
    nvgCircle(vg, x, y, radius * 1.5f);
    NVGpaint glow = nvgRadialGradient(vg, x, y, radius * 0.5f, radius * 1.5f,
                                      nvgRGBA(color.r * 255, color.g * 255, color.b * 255, static_cast<int>(100 * intensity)),
                                      nvgRGBA(color.r * 255, color.g * 255, color.b * 255, 0));
    nvgFillPaint(vg, glow);
    nvgFill(vg);

    nvgBeginPath(vg);
    nvgCircle(vg, x, y, radius);
    nvgFillColor(vg, nvgRGBA(color.r * 255, color.g * 255, color.b * 255, static_cast<int>(255 * intensity)));
    nvgFill(vg);
}

void VulkanRenderer::drawFlowingParticles(float x1, float y1, float x2, float y2, NVGcolor color, int count)
{
    for (int i = 0; i < count; ++i)
    {
        float t = fmod(time_accumulator * 0.5f + i * 0.2f, 1.0f);
        float x = x1 + (x2 - x1) * t;
        float y = y1 + (y2 - y1) * t;

        float size = 3 + 2 * sin(time_accumulator * 3 + i);
        drawGlowingCircle(x, y, size, color, 1.0f - t * 0.5f);
    }
}

void VulkanRenderer::resize(int width, int height)
{
    window_width = width;
    window_height = height;
    recreateSwapChain();
}

void VulkanRenderer::recreateSwapChain()
{
    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    while (width == 0 || height == 0)
    {
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(device);

    cleanupSwapChain();

    createSwapChain();
    createImageViews();
    createFramebuffers();
}

void VulkanRenderer::cleanupSwapChain()
{
    for (auto framebuffer : swapChainFramebuffers)
    {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }

    for (auto imageView : swapChainImageViews)
    {
        vkDestroyImageView(device, imageView, nullptr);
    }

    vkDestroySwapchainKHR(device, swapChain, nullptr);
}

void VulkanRenderer::cleanup()
{
    // Only cleanup if we have a valid device
    if (device != VK_NULL_HANDLE)
    {
        cleanupSwapChain();

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
            vkDestroyFence(device, inFlightFences[i], nullptr);
        }

        vkDestroyCommandPool(device, commandPool, nullptr);

        if (vg)
        {
            nvgDeleteVk(vg);
        }

        vkDestroyRenderPass(device, renderPass, nullptr);
        vkDestroyDevice(device, nullptr);
    }

    if (enableValidationLayers && debugMessenger != VK_NULL_HANDLE)
    {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr)
        {
            func(instance, debugMessenger, nullptr);
        }
    }

    if (surface != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(instance, surface, nullptr);
    }

    if (instance != VK_NULL_HANDLE)
    {
        vkDestroyInstance(instance, nullptr);
    }
}