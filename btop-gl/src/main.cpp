#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <memory>
#include <chrono>

#include "../include/btop_renderer.hpp"
#include "../include/btop_glue.hpp"

// Global variables
std::unique_ptr<BtopRenderer> renderer;
bool fullscreen = false;
int windowed_width = 1280;
int windowed_height = 720;

// Callback functions
void framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
    glViewport(0, 0, width, height);
    if (renderer)
    {
        renderer->resize(width, height);
    }
}

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
            // Toggle fullscreen
            if (fullscreen)
            {
                glfwSetWindowMonitor(window, nullptr, 100, 100, windowed_width, windowed_height, 0);
                fullscreen = false;
            }
            else
            {
                GLFWmonitor *monitor = glfwGetPrimaryMonitor();
                const GLFWvidmode *mode = glfwGetVideoMode(monitor);
                glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
                fullscreen = true;
            }
            break;
        case GLFW_KEY_F:
            // Toggle fullscreen (alternative key)
            if (!fullscreen)
            {
                GLFWmonitor *monitor = glfwGetPrimaryMonitor();
                const GLFWvidmode *mode = glfwGetVideoMode(monitor);
                glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
                fullscreen = true;
            }
            break;
        }
    }
}

void error_callback(int error, const char *description)
{
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

int main(int argc, char *argv[])
{
    // Parse command line arguments
    bool start_fullscreen = false;
    bool screensaver_mode = false;

    for (int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];
        if (arg == "--fullscreen" || arg == "-f")
        {
            start_fullscreen = true;
        }
        else if (arg == "--screensaver" || arg == "-s")
        {
            screensaver_mode = true;
            start_fullscreen = true;
        }
        else if (arg == "--help" || arg == "-h")
        {
            std::cout << "btop-gl - High-FPS OpenGL System Monitor\n\n";
            std::cout << "Usage: " << argv[0] << " [OPTIONS]\n\n";
            std::cout << "Options:\n";
            std::cout << "  -f, --fullscreen    Start in fullscreen mode\n";
            std::cout << "  -s, --screensaver   Run as screensaver (implies fullscreen)\n";
            std::cout << "  -h, --help          Show this help message\n\n";
            std::cout << "Controls:\n";
            std::cout << "  ESC, Q              Quit\n";
            std::cout << "  F11, F              Toggle fullscreen\n";
            return 0;
        }
    }

    // Initialize GLFW
    glfwSetErrorCallback(error_callback);

    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    // Configure GLFW
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4); // Enable multisampling

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    if (screensaver_mode)
    {
        glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
        glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);
    }

    // Create window
    GLFWmonitor *monitor = start_fullscreen ? glfwGetPrimaryMonitor() : nullptr;
    const GLFWvidmode *mode = glfwGetVideoMode(glfwGetPrimaryMonitor());

    int width = start_fullscreen ? mode->width : windowed_width;
    int height = start_fullscreen ? mode->height : windowed_height;

    GLFWwindow *window = glfwCreateWindow(width, height, "btop++ OpenGL", monitor, nullptr);
    if (!window)
    {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    fullscreen = start_fullscreen;

    // Set callbacks
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetKeyCallback(window, key_callback);

    // Enable VSync
    glfwSwapInterval(1);

    // Initialize GLEW
    if (glewInit() != GLEW_OK)
    {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        glfwTerminate();
        return -1;
    }

    // Print OpenGL info
    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
    std::cout << "Renderer: " << glGetString(GL_RENDERER) << std::endl;

    // Enable features
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

    try
    {
        // Initialize data collector
        auto &collector = BtopGLCollector::getInstance();
        if (!collector.initialize())
        {
            std::cerr << "Failed to initialize btop data collector" << std::endl;
            glfwTerminate();
            return -1;
        }

        // Start data collection
        collector.start();

        // Create renderer
        renderer = std::make_unique<BtopRenderer>(width, height);
        if (!renderer->initialize())
        {
            std::cerr << "Failed to initialize renderer" << std::endl;
            glfwTerminate();
            return -1;
        }

        // Configure renderer for screensaver mode
        if (screensaver_mode)
        {
            renderer->setAnimationSpeed(0.5f); // Slower animations for screensaver
        }
        else
        {
            renderer->setAnimationSpeed(1.0f);
        }

        std::cout << "btop-gl initialized successfully!" << std::endl;
        std::cout << "Press ESC or Q to quit, F11 or F to toggle fullscreen" << std::endl;

        // FPS calculation
        auto last_time = std::chrono::high_resolution_clock::now();
        int frame_count = 0;
        float fps = 0.0f;

        // Main loop
        while (!glfwWindowShouldClose(window))
        {
            // Calculate FPS
            auto current_time = std::chrono::high_resolution_clock::now();
            auto delta_time = std::chrono::duration<float>(current_time - last_time).count();
            frame_count++;

            if (delta_time >= 1.0f)
            {
                fps = frame_count / delta_time;
                frame_count = 0;
                last_time = current_time;

                // Update window title with FPS (unless in screensaver mode)
                if (!screensaver_mode)
                {
                    std::string title = "btop++ OpenGL - " + std::to_string(static_cast<int>(fps)) + " FPS";
                    glfwSetWindowTitle(window, title.c_str());
                }
            }

            // Update and render
            renderer->update();
            renderer->render();

            // Swap buffers and poll events
            glfwSwapBuffers(window);
            glfwPollEvents();
        }

        // Cleanup
        collector.stop();
        renderer.reset();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwTerminate();
    return 0;
}