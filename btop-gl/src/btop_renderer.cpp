#include "../include/btop_renderer.hpp"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <cstring>

BtopRenderer::BtopRenderer(int width, int height)
    : window_width(width), window_height(height),
      frame_time(0.0f), animation_speed(1.0f), time_accumulator(0.0f),
      graph_history_size(100), collector(BtopGLCollector::getInstance())
{

    last_frame_time = std::chrono::high_resolution_clock::now();
}

BtopRenderer::~BtopRenderer()
{
    // Cleanup OpenGL resources
    if (vao_lines)
        glDeleteVertexArrays(1, &vao_lines);
    if (vbo_lines)
        glDeleteBuffers(1, &vbo_lines);
    if (vao_quads)
        glDeleteVertexArrays(1, &vao_quads);
    if (vbo_quads)
        glDeleteBuffers(1, &vbo_quads);
}

bool BtopRenderer::initialize()
{
    try
    {
        createShaders();
        setupBuffers();
        calculateLayout();

        // Initialize graph data structures
        initializeGraphData(cpu_total_graph, CPU_COLOR);
        initializeGraphData(memory_used_graph, MEMORY_COLOR);
        initializeGraphData(network_recv_graph, NETWORK_RECV_COLOR);
        initializeGraphData(network_send_graph, NETWORK_SEND_COLOR);

        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Failed to initialize renderer: " << e.what() << std::endl;
        return false;
    }
}

void BtopRenderer::createShaders()
{
    line_shader = std::make_unique<Shader>("shaders/line.vert", "shaders/line.frag");
    quad_shader = std::make_unique<Shader>("shaders/quad.vert", "shaders/quad.frag");
}

void BtopRenderer::setupBuffers()
{
    // Setup line VAO/VBO for graphs
    glGenVertexArrays(1, &vao_lines);
    glGenBuffers(1, &vbo_lines);

    glBindVertexArray(vao_lines);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_lines);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    // Setup quad VAO/VBO for bars and backgrounds
    glGenVertexArrays(1, &vao_quads);
    glGenBuffers(1, &vbo_quads);

    float quad_vertices[] = {
        // positions   // texture coords
        0.0f, 1.0f, 0.0f, 1.0f,
        1.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f,

        0.0f, 1.0f, 0.0f, 1.0f,
        1.0f, 1.0f, 1.0f, 1.0f,
        1.0f, 0.0f, 1.0f, 0.0f};

    glBindVertexArray(vao_quads);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_quads);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void BtopRenderer::calculateLayout()
{
    float margin = 0.02f;
    float section_spacing = 0.05f;

    // CPU section (top left)
    layout.cpu_graph_area = {margin, 0.5f + section_spacing, 0.45f, 0.4f};
    layout.cpu_bars_area = {margin, 0.5f, 0.45f, section_spacing};

    // Memory section (top right)
    layout.memory_graph_area = {0.5f + margin, 0.5f + section_spacing, 0.45f, 0.4f};
    layout.memory_bars_area = {0.5f + margin, 0.5f, 0.45f, section_spacing};

    // Network section (bottom)
    layout.network_graph_area = {margin, margin + section_spacing, 0.95f, 0.4f};
    layout.network_bars_area = {margin, margin, 0.95f, section_spacing};
}

void BtopRenderer::initializeGraphData(GraphData &graph, const std::array<float, 3> &color)
{
    graph.color = color;
    graph.max_value = 100.0f;
    graph.current_value = 0.0f;
    graph.values.clear();
    graph.vertices.clear();

    glGenBuffers(1, &graph.vbo);
}

void BtopRenderer::initializeBarData(BarData &bar, const std::array<float, 3> &color,
                                     const std::array<float, 4> &position)
{
    bar.color = color;
    bar.position = position;
    bar.current_value = 0.0f;
    bar.target_value = 0.0f;
    bar.animated_value = 0.0f;
}

void BtopRenderer::update()
{
    // Update timing
    auto current_time = std::chrono::high_resolution_clock::now();
    frame_time = std::chrono::duration<float>(current_time - last_frame_time).count();
    last_frame_time = current_time;
    time_accumulator += frame_time * animation_speed;

    // Get latest data from collector
    auto cpu_info = collector.getCpuInfo();
    auto mem_info = collector.getMemInfo();
    auto net_info = collector.getNetInfo();

    // Update CPU graph
    if (!cpu_info.cpu_percent.empty() && !cpu_info.cpu_percent.at("total").empty())
    {
        float cpu_total = static_cast<float>(cpu_info.cpu_percent.at("total").back());
        updateGraphData(cpu_total_graph, cpu_total);
    }

    // Update Memory graph
    if (mem_info.stats.count("used") && mem_info.stats.count("available"))
    {
        uint64_t used = mem_info.stats.at("used");
        uint64_t total = used + mem_info.stats.at("available");
        float mem_percent = total > 0 ? (static_cast<float>(used) / total * 100.0f) : 0.0f;
        updateGraphData(memory_used_graph, mem_percent);
    }

    // Update Network graphs
    if (!net_info.bandwidth.empty())
    {
        if (net_info.bandwidth.count("download") && !net_info.bandwidth.at("download").empty())
        {
            float recv_speed = static_cast<float>(net_info.bandwidth.at("download").back());
            updateGraphData(network_recv_graph, recv_speed / 1024.0f); // Convert to KB/s
        }
        if (net_info.bandwidth.count("upload") && !net_info.bandwidth.at("upload").empty())
        {
            float send_speed = static_cast<float>(net_info.bandwidth.at("upload").back());
            updateGraphData(network_send_graph, send_speed / 1024.0f); // Convert to KB/s
        }
    }
}

void BtopRenderer::updateGraphData(GraphData &graph, float new_value)
{
    graph.current_value = new_value;
    graph.values.push_back(new_value);

    // Maintain history size
    while (graph.values.size() > graph_history_size)
    {
        graph.values.pop_front();
    }

    // Update max value for scaling - make it more responsive
    if (new_value > graph.max_value)
    {
        graph.max_value = new_value * 1.2f; // Add more headroom for dramatic effect
    }

    // Ensure minimum scale for visibility
    if (graph.max_value < 10.0f)
    {
        graph.max_value = 10.0f;
    }

    // Generate vertices for the graph
    graph.vertices.clear();
    graph.vertices.reserve(graph.values.size() * 2);

    for (size_t i = 0; i < graph.values.size(); ++i)
    {
        float x = static_cast<float>(i) / (graph_history_size - 1);
        float y = graph.max_value > 0 ? graph.values[i] / graph.max_value : 0.0f;

        // Clamp y to [0, 1] and add some baseline for visibility
        y = std::clamp(y, 0.0f, 1.0f);
        if (y < 0.05f && graph.values[i] > 0)
        {
            y = 0.05f; // Minimum visible height
        }

        graph.vertices.push_back(x);
        graph.vertices.push_back(y);
    }

    // Update VBO
    glBindBuffer(GL_ARRAY_BUFFER, graph.vbo);
    glBufferData(GL_ARRAY_BUFFER, graph.vertices.size() * sizeof(float),
                 graph.vertices.data(), GL_DYNAMIC_DRAW);
}

void BtopRenderer::render()
{
    // Clear screen with enhanced background
    glClearColor(BACKGROUND_COLOR[0], BACKGROUND_COLOR[1], BACKGROUND_COLOR[2], 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Setup projection matrix (orthographic)
    float projection[16] = {
        2.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 2.0f, 0.0f, 0.0f,
        0.0f, 0.0f, -1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f, 1.0f};

    // Add some test data if no real data is available
    if (cpu_total_graph.values.empty())
    {
        // Generate some test data for visibility
        float test_cpu = 30.0f + 20.0f * sin(time_accumulator * 0.5f);
        updateGraphData(cpu_total_graph, test_cpu);

        float test_mem = 45.0f + 15.0f * cos(time_accumulator * 0.3f);
        updateGraphData(memory_used_graph, test_mem);

        float test_net_recv = 1024.0f + 512.0f * sin(time_accumulator * 0.8f);
        updateGraphData(network_recv_graph, test_net_recv);

        float test_net_send = 256.0f + 128.0f * cos(time_accumulator * 1.2f);
        updateGraphData(network_send_graph, test_net_send);
    }

    // Render CPU graph with enhanced visibility
    if (!cpu_total_graph.vertices.empty())
    {
        renderGraph(cpu_total_graph,
                    layout.cpu_graph_area[0], layout.cpu_graph_area[1],
                    layout.cpu_graph_area[2], layout.cpu_graph_area[3]);
    }

    // Render Memory graph
    if (!memory_used_graph.vertices.empty())
    {
        renderGraph(memory_used_graph,
                    layout.memory_graph_area[0], layout.memory_graph_area[1],
                    layout.memory_graph_area[2], layout.memory_graph_area[3]);
    }

    // Render Network graphs
    float net_height = layout.network_graph_area[3] / 2.0f;
    if (!network_recv_graph.vertices.empty())
    {
        renderGraph(network_recv_graph,
                    layout.network_graph_area[0], layout.network_graph_area[1] + net_height,
                    layout.network_graph_area[2], net_height);
    }

    if (!network_send_graph.vertices.empty())
    {
        renderGraph(network_send_graph,
                    layout.network_graph_area[0], layout.network_graph_area[1],
                    layout.network_graph_area[2], net_height);
    }
}

void BtopRenderer::renderGraph(const GraphData &graph, float x, float y, float width, float height)
{
    if (graph.vertices.empty())
        return;

    line_shader->use();

    // Set uniforms
    float projection[16] = {
        2.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 2.0f, 0.0f, 0.0f,
        0.0f, 0.0f, -1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f, 1.0f};

    glUniformMatrix4fv(glGetUniformLocation(line_shader->ID, "projection"), 1, GL_FALSE, projection);
    glUniform2f(glGetUniformLocation(line_shader->ID, "offset"), x, y);
    glUniform2f(glGetUniformLocation(line_shader->ID, "scale"), width, height);
    glUniform3f(glGetUniformLocation(line_shader->ID, "color"),
                graph.color[0], graph.color[1], graph.color[2]);
    glUniform1f(glGetUniformLocation(line_shader->ID, "alpha"), 0.9f);
    glUniform1f(glGetUniformLocation(line_shader->ID, "time"), time_accumulator);

    // Bind and draw
    glBindVertexArray(vao_lines);
    glBindBuffer(GL_ARRAY_BUFFER, graph.vbo);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)0);

    // Make lines much thicker and more dramatic
    glLineWidth(4.0f);
    glDrawArrays(GL_LINE_STRIP, 0, graph.vertices.size() / 2);

    // Draw a second pass with thinner bright line for glow effect
    glUniform1f(glGetUniformLocation(line_shader->ID, "alpha"), 1.0f);
    glLineWidth(2.0f);
    glDrawArrays(GL_LINE_STRIP, 0, graph.vertices.size() / 2);
}

void BtopRenderer::resize(int width, int height)
{
    window_width = width;
    window_height = height;
    calculateLayout();
}

float BtopRenderer::normalize(float value, float min_val, float max_val)
{
    if (max_val <= min_val)
        return 0.0f;
    return std::clamp((value - min_val) / (max_val - min_val), 0.0f, 1.0f);
}

std::array<float, 3> BtopRenderer::interpolateColor(const std::array<float, 3> &color1,
                                                    const std::array<float, 3> &color2,
                                                    float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return {
        color1[0] + t * (color2[0] - color1[0]),
        color1[1] + t * (color2[1] - color1[1]),
        color1[2] + t * (color2[2] - color1[2])};
}