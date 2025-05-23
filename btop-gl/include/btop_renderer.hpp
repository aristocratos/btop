#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <memory>
#include <chrono>
#include <unordered_map>
#include <deque>

#include "shader.hpp"
#include "btop_glue.hpp"

class BtopRenderer
{
public:
    BtopRenderer(int width, int height);
    ~BtopRenderer();

    // Initialize OpenGL context and resources
    bool initialize();

    // Main render loop
    void render();

    // Update data from collector
    void update();

    // Handle window resize
    void resize(int width, int height);

    // Get frame time for FPS display
    float getFrameTime() const { return frame_time; }

    // Configuration
    void setAnimationSpeed(float speed) { animation_speed = speed; }
    void setGraphHistory(size_t history) { graph_history_size = history; }

    // Mode system for cycling through different visualizations
    enum class VisualizationMode
    {
        CLASSIC_GRAPHS = 0, // Traditional line graphs
        CPU_CORES,          // Individual CPU core visualization
        MEMORY_LANDSCAPE,   // Memory usage as terrain
        NETWORK_FLOW,       // Particle-based network visualization
        PROCESS_RAIN,       // Matrix-style process display
        DISK_ACTIVITY,      // Disk I/O visualization
        OVERVIEW_DASHBOARD, // Combined artistic overview
        MODE_COUNT          // Total number of modes
    };

    void cycleMode()
    {
        current_mode = static_cast<VisualizationMode>((static_cast<int>(current_mode) + 1) % static_cast<int>(VisualizationMode::MODE_COUNT));
        mode_transition_time = 0.0f;
    }

    VisualizationMode getCurrentMode() const { return current_mode; }

private:
    // Window dimensions
    int window_width, window_height;

    // Shaders
    std::unique_ptr<Shader> line_shader;
    std::unique_ptr<Shader> quad_shader;
    std::unique_ptr<Shader> text_shader;

    // OpenGL objects
    GLuint vao_lines, vbo_lines;
    GLuint vao_quads, vbo_quads;
    GLuint vao_text, vbo_text;

    // Animation and timing
    std::chrono::high_resolution_clock::time_point last_frame_time;
    float frame_time;
    float animation_speed;
    float time_accumulator;

    // Graph settings
    size_t graph_history_size;

    // Data structures for visualization
    struct GraphData
    {
        std::deque<float> values;
        std::vector<float> vertices;
        GLuint vbo;
        float max_value;
        float current_value;
        std::array<float, 3> color;
    };

    struct BarData
    {
        float current_value;
        float target_value;
        float animated_value;
        std::array<float, 3> color;
        std::array<float, 4> position; // x, y, width, height
    };

    // CPU visualization data
    std::vector<GraphData> cpu_core_graphs;
    GraphData cpu_total_graph;
    std::vector<BarData> cpu_core_bars;
    BarData cpu_total_bar;

    // Memory visualization data
    GraphData memory_used_graph;
    BarData memory_used_bar;
    BarData memory_cached_bar;
    BarData memory_available_bar;

    // Network visualization data
    GraphData network_recv_graph;
    GraphData network_send_graph;
    BarData network_recv_bar;
    BarData network_send_bar;

    // Colors for different components - Enhanced for visual appeal
    static constexpr std::array<float, 3> CPU_COLOR = {0.0f, 1.0f, 0.4f};           // Bright neon green
    static constexpr std::array<float, 3> MEMORY_COLOR = {1.0f, 0.2f, 0.6f};        // Hot pink/magenta
    static constexpr std::array<float, 3> NETWORK_RECV_COLOR = {0.2f, 0.8f, 1.0f};  // Cyan blue
    static constexpr std::array<float, 3> NETWORK_SEND_COLOR = {1.0f, 0.6f, 0.0f};  // Orange
    static constexpr std::array<float, 3> BACKGROUND_COLOR = {0.08f, 0.08f, 0.12f}; // Deep blue-black

    // Additional artistic colors
    static constexpr std::array<float, 3> ACCENT_COLOR = {0.8f, 0.4f, 1.0f};    // Purple accent
    static constexpr std::array<float, 3> HIGHLIGHT_COLOR = {1.0f, 1.0f, 0.4f}; // Bright yellow
    static constexpr std::array<float, 3> SECONDARY_COLOR = {0.4f, 1.0f, 1.0f}; // Light cyan

    // Helper methods
    void initializeGraphData(GraphData &graph, const std::array<float, 3> &color);
    void initializeBarData(BarData &bar, const std::array<float, 3> &color,
                           const std::array<float, 4> &position);
    void updateGraphData(GraphData &graph, float new_value);
    void updateBarData(BarData &bar, float target_value, float delta_time);
    void renderGraph(const GraphData &graph, float x, float y, float width, float height);
    void renderBar(const BarData &bar);
    void renderText(const std::string &text, float x, float y, float scale);
    void renderLabelsAndValues();

    // Text rendering
    void renderCharacter(char c, float x, float y, float scale, const std::array<float, 3> &color);
    void renderNumber(float value, float x, float y, float scale, const std::array<float, 3> &color);
    std::vector<std::array<float, 4>> getCharacterLines(char c); // Returns line segments for character

    // Layout calculations
    void calculateLayout();
    struct Layout
    {
        // CPU section
        std::array<float, 4> cpu_graph_area; // x, y, width, height
        std::array<float, 4> cpu_bars_area;

        // Memory section
        std::array<float, 4> memory_graph_area;
        std::array<float, 4> memory_bars_area;

        // Network section
        std::array<float, 4> network_graph_area;
        std::array<float, 4> network_bars_area;

        // Process list area
        std::array<float, 4> process_area;
    } layout;

    // Data collector reference
    BtopGLCollector &collector;

    // Cached data
    Cpu::cpu_info last_cpu_info;
    Mem::mem_info last_mem_info;
    Net::net_info last_net_info;
    std::vector<Proc::proc_info> last_proc_info;

    // Utility methods
    float normalize(float value, float min_val, float max_val);
    std::array<float, 3> interpolateColor(const std::array<float, 3> &color1,
                                          const std::array<float, 3> &color2,
                                          float t);
    void setupBuffers();
    void createShaders();

    // Mode system variables
    VisualizationMode current_mode;
    float mode_transition_time;

    // Mode-specific rendering methods
    void renderClassicGraphs();
    void renderCpuCores();
    void renderMemoryLandscape();
    void renderNetworkFlow();
    void renderProcessRain();
    void renderDiskActivity();
    void renderOverviewDashboard();
};