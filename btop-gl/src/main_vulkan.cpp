#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>
#include <memory>

#include "../include/vulkan_renderer.hpp"

// Global renderer instance
std::unique_ptr<VulkanRenderer> renderer;

// Callback for window resize
void framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
    if (renderer)
    {
        renderer->resize(width, height);
    }
}

// Callback for key input
void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    if (action == GLFW_PRESS)
    {
        switch (key)
        {
        case GLFW_KEY_ESCAPE:
        case GLFW_KEY_Q:
            glfwSetWindowShouldClose(window, true);
            break;
        case GLFW_KEY_F11:
        case GLFW_KEY_F:
            // Toggle fullscreen
            {
                static bool fullscreen = false;
                static int windowed_x, windowed_y, windowed_width, windowed_height;

                if (!fullscreen)
                {
                    // Save windowed position and size
                    glfwGetWindowPos(window, &windowed_x, &windowed_y);
                    glfwGetWindowSize(window, &windowed_width, &windowed_height);

                    // Go fullscreen
                    GLFWmonitor *monitor = glfwGetPrimaryMonitor();
                    const GLFWvidmode *mode = glfwGetVideoMode(monitor);
                    glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
                    fullscreen = true;
                }
                else
                {
                    // Go windowed
                    glfwSetWindowMonitor(window, nullptr, windowed_x, windowed_y, windowed_width, windowed_height, 0);
                    fullscreen = false;
                }
            }
            break;
        case GLFW_KEY_SPACE:
            // Manual mode cycling
            if (renderer)
            {
                renderer->cycleMode();
            }
            break;
        }
    }
}

int main(int argc, char *argv[])
{
    // Parse command line arguments
    bool fullscreen = false;
    int width = 1920;
    int height = 1080;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--fullscreen" || arg == "-f")
        {
            fullscreen = true;
        }
        else if (arg == "--width" || arg == "-w")
        {
            if (i + 1 < argc)
            {
                width = std::atoi(argv[++i]);
            }
        }
        else if (arg == "--height" || arg == "-h")
        {
            if (i + 1 < argc)
            {
                height = std::atoi(argv[++i]);
            }
        }
        else if (arg == "--help")
        {
            std::cout << "btop-gl Vulkan Edition - High-FPS System Monitor Screensaver\n"
                      << "Usage: " << argv[0] << " [options]\n"
                      << "Options:\n"
                      << "  --fullscreen, -f    Start in fullscreen mode\n"
                      << "  --width, -w <num>   Set window width (default: 1920)\n"
                      << "  --height, -h <num>  Set window height (default: 1080)\n"
                      << "  --help              Show this help message\n"
                      << "\nControls:\n"
                      << "  ESC/Q               Quit\n"
                      << "  F11/F               Toggle fullscreen\n"
                      << "  SPACE               Cycle visualization mode\n"
                      << std::endl;
            return 0;
        }
    }

    // Initialize GLFW
    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    // Tell GLFW not to create OpenGL context
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    // Create window
    GLFWwindow *window;
    if (fullscreen)
    {
        GLFWmonitor *monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode *mode = glfwGetVideoMode(monitor);
        window = glfwCreateWindow(mode->width, mode->height, "btop++ Vulkan - System Monitor", monitor, nullptr);
        width = mode->width;
        height = mode->height;
    }
    else
    {
        window = glfwCreateWindow(width, height, "btop++ Vulkan - System Monitor", nullptr, nullptr);
    }

    if (!window)
    {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    // Check Vulkan support
    if (!glfwVulkanSupported())
    {
        std::cerr << "Vulkan not supported!" << std::endl;
        glfwTerminate();
        return -1;
    }

    // Set callbacks
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetKeyCallback(window, key_callback);

    // Create and initialize renderer
    renderer = std::make_unique<VulkanRenderer>(window, width, height);
    if (!renderer->initialize())
    {
        std::cerr << "Failed to initialize Vulkan renderer" << std::endl;
        glfwTerminate();
        return -1;
    }

    std::cout << "btop-gl Vulkan edition initialized successfully!" << std::endl;
    std::cout << "Press ESC or Q to quit, F11 or F to toggle fullscreen, SPACE to cycle modes" << std::endl;

    // Main render loop
    int frame_count = 0;
    auto last_fps_time = std::chrono::high_resolution_clock::now();

    while (!glfwWindowShouldClose(window))
    {
        // Poll events
        glfwPollEvents();

        // Update renderer
        renderer->update();

        // Render
        renderer->render();

        // Calculate and display FPS
        frame_count++;
        auto current_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - last_fps_time);
        if (duration.count() >= 1000)
        {
            float fps = frame_count * 1000.0f / duration.count();
            std::string title = "btop++ Vulkan - " + std::to_string(static_cast<int>(fps)) + " FPS";
            glfwSetWindowTitle(window, title.c_str());

            frame_count = 0;
            last_fps_time = current_time;
        }
    }

    // Cleanup
    renderer.reset();
    glfwTerminate();
    return 0;
}