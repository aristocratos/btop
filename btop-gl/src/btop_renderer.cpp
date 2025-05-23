#include "../include/btop_renderer.hpp"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

BtopRenderer::BtopRenderer(int width, int height)
    : window_width(width), window_height(height),
      frame_time(0.0f), animation_speed(1.0f), time_accumulator(0.0f),
      graph_history_size(100), collector(BtopGLCollector::getInstance()),
      current_mode(VisualizationMode::CLASSIC_GRAPHS), mode_transition_time(0.0f)
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

    // Auto-cycle modes every 10 seconds for screensaver effect
    mode_transition_time += frame_time;
    if (mode_transition_time > 10.0f)
    {
        cycleMode();
    }

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

    // Render mode indicator
    std::string mode_name;
    switch (current_mode)
    {
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

    // Display current mode and time remaining
    renderText("BTOP++ GL - " + mode_name, 0.02f, 0.95f, 0.04f);
    float time_remaining = 10.0f - mode_transition_time;
    renderText("NEXT: " + std::to_string(static_cast<int>(time_remaining)) + "s", 0.8f, 0.95f, 0.03f);

    // Render based on current mode
    switch (current_mode)
    {
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

void BtopRenderer::renderLabelsAndValues()
{
    // Render actual text labels using our line-based text system

    // Get current values
    float cpu_value = cpu_total_graph.values.empty() ? 0.0f : cpu_total_graph.values.back();
    float mem_value = memory_used_graph.values.empty() ? 0.0f : memory_used_graph.values.back();
    float net_recv_value = network_recv_graph.values.empty() ? 0.0f : network_recv_graph.values.back();
    float net_send_value = network_send_graph.values.empty() ? 0.0f : network_send_graph.values.back();

    // Render CPU label and value
    renderText("CPU", layout.cpu_graph_area[0], layout.cpu_graph_area[1] + layout.cpu_graph_area[3] + 0.02f, 0.03f);
    renderNumber(cpu_value, layout.cpu_graph_area[0] + 0.15f, layout.cpu_graph_area[1] + layout.cpu_graph_area[3] + 0.02f, 0.025f, CPU_COLOR);

    // Render Memory label and value
    renderText("MEM", layout.memory_graph_area[0], layout.memory_graph_area[1] + layout.memory_graph_area[3] + 0.02f, 0.03f);
    renderNumber(mem_value, layout.memory_graph_area[0] + 0.15f, layout.memory_graph_area[1] + layout.memory_graph_area[3] + 0.02f, 0.025f, MEMORY_COLOR);

    // Render Network labels and values
    renderText("NET IN", layout.network_graph_area[0], layout.network_graph_area[1] + layout.network_graph_area[3] + 0.02f, 0.025f);
    renderNumber(net_recv_value, layout.network_graph_area[0] + 0.2f, layout.network_graph_area[1] + layout.network_graph_area[3] + 0.02f, 0.02f, NETWORK_RECV_COLOR);

    renderText("OUT", layout.network_graph_area[0] + 0.5f, layout.network_graph_area[1] + layout.network_graph_area[3] + 0.02f, 0.025f);
    renderNumber(net_send_value, layout.network_graph_area[0] + 0.65f, layout.network_graph_area[1] + layout.network_graph_area[3] + 0.02f, 0.02f, NETWORK_SEND_COLOR);

    // Render title
    renderText("BTOP++ OPENGL", 0.02f, 0.95f, 0.04f);
}

void BtopRenderer::renderText(const std::string &text, float x, float y, float scale)
{
    float char_width = scale * 0.8f;
    for (size_t i = 0; i < text.length(); ++i)
    {
        renderCharacter(text[i], x + i * char_width, y, scale, HIGHLIGHT_COLOR);
    }
}

void BtopRenderer::renderCharacter(char c, float x, float y, float scale, const std::array<float, 3> &color)
{
    auto lines = getCharacterLines(c);
    if (lines.empty())
        return;

    if (!line_shader)
        return;
    line_shader->use();

    // Setup projection matrix
    float projection[16] = {
        2.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 2.0f, 0.0f, 0.0f,
        0.0f, 0.0f, -1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f, 1.0f};

    glUniformMatrix4fv(glGetUniformLocation(line_shader->ID, "projection"), 1, GL_FALSE, projection);
    glUniform2f(glGetUniformLocation(line_shader->ID, "offset"), 0.0f, 0.0f);
    glUniform2f(glGetUniformLocation(line_shader->ID, "scale"), 1.0f, 1.0f);
    glUniform3f(glGetUniformLocation(line_shader->ID, "color"), color[0], color[1], color[2]);
    glUniform1f(glGetUniformLocation(line_shader->ID, "alpha"), 1.0f);
    glUniform1f(glGetUniformLocation(line_shader->ID, "time"), time_accumulator);

    glBindVertexArray(vao_lines);

    // Draw each line segment for the character
    for (const auto &line : lines)
    {
        float vertices[] = {
            x + line[0] * scale, y + line[1] * scale,
            x + line[2] * scale, y + line[3] * scale};

        glBindBuffer(GL_ARRAY_BUFFER, vbo_lines);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)0);

        glLineWidth(3.0f);
        glDrawArrays(GL_LINES, 0, 2);
    }
}

void BtopRenderer::renderNumber(float value, float x, float y, float scale, const std::array<float, 3> &color)
{
    std::string num_str = std::to_string(static_cast<int>(value)) + "%";
    float char_width = scale * 0.6f;
    for (size_t i = 0; i < num_str.length(); ++i)
    {
        renderCharacter(num_str[i], x + i * char_width, y, scale, color);
    }
}

std::vector<std::array<float, 4>> BtopRenderer::getCharacterLines(char c)
{
    // Simple 7-segment style font using line segments
    // Each line is [x1, y1, x2, y2] in normalized coordinates (0-1)
    switch (c)
    {
    case 'A':
        return {
            {0.0f, 0.0f, 0.5f, 1.0f}, {0.5f, 1.0f, 1.0f, 0.0f}, // /\
            {0.25f, 0.5f, 0.75f, 0.5f} // -
        };
    case 'B':
        return {
            {0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.7f, 1.0f}, {0.7f, 1.0f, 0.7f, 0.5f}, {0.0f, 0.5f, 0.7f, 0.5f}, {0.7f, 0.5f, 0.7f, 0.0f}, {0.0f, 0.0f, 0.7f, 0.0f}};
    case 'C':
        return {
            {1.0f, 0.2f, 0.8f, 0.0f}, {0.8f, 0.0f, 0.2f, 0.0f}, {0.2f, 0.0f, 0.0f, 0.2f}, {0.0f, 0.2f, 0.0f, 0.8f}, {0.0f, 0.8f, 0.2f, 1.0f}, {0.2f, 1.0f, 0.8f, 1.0f}, {0.8f, 1.0f, 1.0f, 0.8f}};
    case 'E':
        return {
            {0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.5f, 0.7f, 0.5f}, {0.0f, 0.0f, 1.0f, 0.0f}};
    case 'G':
        return {
            {1.0f, 0.8f, 0.8f, 1.0f}, {0.8f, 1.0f, 0.2f, 1.0f}, {0.2f, 1.0f, 0.0f, 0.8f}, {0.0f, 0.8f, 0.0f, 0.2f}, {0.0f, 0.2f, 0.2f, 0.0f}, {0.2f, 0.0f, 0.8f, 0.0f}, {0.8f, 0.0f, 1.0f, 0.2f}, {1.0f, 0.2f, 1.0f, 0.5f}, {1.0f, 0.5f, 0.6f, 0.5f}};
    case 'I':
        return {
            {0.2f, 0.0f, 0.8f, 0.0f}, {0.5f, 0.0f, 0.5f, 1.0f}, {0.2f, 1.0f, 0.8f, 1.0f}};
    case 'L':
        return {
            {0.0f, 1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 0.0f}};
    case 'M':
        return {
            {0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.5f, 0.5f}, {0.5f, 0.5f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 0.0f}};
    case 'N':
        return {
            {0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 1.0f, 1.0f}};
    case 'O':
        return {
            {0.2f, 0.0f, 0.8f, 0.0f}, {0.8f, 0.0f, 1.0f, 0.2f}, {1.0f, 0.2f, 1.0f, 0.8f}, {1.0f, 0.8f, 0.8f, 1.0f}, {0.8f, 1.0f, 0.2f, 1.0f}, {0.2f, 1.0f, 0.0f, 0.8f}, {0.0f, 0.8f, 0.0f, 0.2f}, {0.0f, 0.2f, 0.2f, 0.0f}};
    case 'P':
        return {
            {0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.8f, 1.0f}, {0.8f, 1.0f, 0.8f, 0.5f}, {0.8f, 0.5f, 0.0f, 0.5f}};
    case 'T':
        return {
            {0.0f, 1.0f, 1.0f, 1.0f}, {0.5f, 1.0f, 0.5f, 0.0f}};
    case 'U':
        return {
            {0.0f, 1.0f, 0.0f, 0.2f}, {0.0f, 0.2f, 0.2f, 0.0f}, {0.2f, 0.0f, 0.8f, 0.0f}, {0.8f, 0.0f, 1.0f, 0.2f}, {1.0f, 0.2f, 1.0f, 1.0f}};
    case '+':
        return {
            {0.5f, 0.2f, 0.5f, 0.8f}, {0.2f, 0.5f, 0.8f, 0.5f}};
    case '0':
        return {
            {0.2f, 0.0f, 0.8f, 0.0f}, {0.8f, 0.0f, 1.0f, 0.2f}, {1.0f, 0.2f, 1.0f, 0.8f}, {1.0f, 0.8f, 0.8f, 1.0f}, {0.8f, 1.0f, 0.2f, 1.0f}, {0.2f, 1.0f, 0.0f, 0.8f}, {0.0f, 0.8f, 0.0f, 0.2f}, {0.0f, 0.2f, 0.2f, 0.0f}};
    case '1':
        return {{0.5f, 0.0f, 0.5f, 1.0f}};
    case '2':
        return {
            {0.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 0.5f}, {1.0f, 0.5f, 0.0f, 0.5f}, {0.0f, 0.5f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 0.0f}};
    case '3':
        return {
            {0.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 0.5f}, {1.0f, 0.5f, 0.5f, 0.5f}, {1.0f, 0.5f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f}};
    case '4':
        return {
            {0.0f, 1.0f, 0.0f, 0.5f}, {0.0f, 0.5f, 1.0f, 0.5f}, {1.0f, 1.0f, 1.0f, 0.0f}};
    case '5':
        return {
            {1.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 0.5f}, {0.0f, 0.5f, 1.0f, 0.5f}, {1.0f, 0.5f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f}};
    case '6':
        return {
            {1.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 1.0f, 0.5f}, {1.0f, 0.5f, 0.0f, 0.5f}};
    case '7':
        return {
            {0.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 0.0f}};
    case '8':
        return {
            {0.0f, 0.5f, 1.0f, 0.5f}, {0.2f, 0.0f, 0.8f, 0.0f}, {0.8f, 0.0f, 1.0f, 0.2f}, {1.0f, 0.2f, 1.0f, 0.8f}, {1.0f, 0.8f, 0.8f, 1.0f}, {0.8f, 1.0f, 0.2f, 1.0f}, {0.2f, 1.0f, 0.0f, 0.8f}, {0.0f, 0.8f, 0.0f, 0.2f}, {0.0f, 0.2f, 0.2f, 0.0f}};
    case '9':
        return {
            {1.0f, 0.5f, 0.0f, 0.5f}, {0.0f, 0.5f, 0.0f, 1.0f}, {0.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f}};
    case '%':
        return {
            {0.0f, 0.0f, 1.0f, 1.0f}, {0.2f, 0.8f, 0.3f, 1.0f}, {0.7f, 0.0f, 0.8f, 0.2f}};
    case ' ':
        return {};
    default:
        return {{0.0f, 0.0f, 1.0f, 1.0f}}; // Default line for unknown chars
    }
}

void BtopRenderer::renderClassicGraphs()
{
    // Original graph-based visualization
    renderLabelsAndValues();

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

void BtopRenderer::renderCpuCores()
{
    // Artistic CPU core visualization - each core as a pulsing orb
    if (!quad_shader)
        return;

    quad_shader->use();

    float projection[16] = {
        2.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 2.0f, 0.0f, 0.0f,
        0.0f, 0.0f, -1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f, 1.0f};

    glUniformMatrix4fv(glGetUniformLocation(quad_shader->ID, "projection"), 1, GL_FALSE, projection);
    glUniform1f(glGetUniformLocation(quad_shader->ID, "time"), time_accumulator);
    glUniform2f(glGetUniformLocation(quad_shader->ID, "size"), 1.0f, 1.0f);
    glUniform1i(glGetUniformLocation(quad_shader->ID, "renderMode"), 2); // Animated mode

    glBindVertexArray(vao_quads);

    // Simulate 8 CPU cores arranged in a circle
    float center_x = 0.5f;
    float center_y = 0.5f;
    float radius = 0.3f;
    int core_count = 8;

    for (int i = 0; i < core_count; ++i)
    {
        float angle = (2.0f * M_PI * i) / core_count;
        float core_x = center_x + cos(angle) * radius;
        float core_y = center_y + sin(angle) * radius;

        // Simulate CPU usage for each core
        float core_usage = 30.0f + 40.0f * sin(time_accumulator * 0.3f + i * 0.5f);
        float core_size = 0.06f + (core_usage / 100.0f) * 0.04f;

        // Color intensity based on usage
        float intensity = core_usage / 100.0f;
        glUniform3f(glGetUniformLocation(quad_shader->ID, "color"),
                    CPU_COLOR[0] * intensity, CPU_COLOR[1] * intensity, CPU_COLOR[2] * intensity);

        glUniform2f(glGetUniformLocation(quad_shader->ID, "offset"),
                    core_x - core_size / 2, core_y - core_size / 2);
        glUniform2f(glGetUniformLocation(quad_shader->ID, "scale"), core_size, core_size);
        glUniform1f(glGetUniformLocation(quad_shader->ID, "alpha"), 0.8f + intensity * 0.2f);

        glDrawArrays(GL_TRIANGLES, 0, 6);

        // Render core label
        renderText("C" + std::to_string(i), core_x - 0.01f, core_y - 0.12f, 0.02f);
        renderNumber(core_usage, core_x - 0.02f, core_y - 0.15f, 0.015f, CPU_COLOR);
    }

    // Central CPU label
    renderText("CPU CORES", center_x - 0.08f, center_y - 0.02f, 0.03f);
}

void BtopRenderer::renderMemoryLandscape()
{
    // Memory visualization as a terrain-like landscape
    if (!quad_shader)
        return;

    quad_shader->use();

    float projection[16] = {
        2.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 2.0f, 0.0f, 0.0f,
        0.0f, 0.0f, -1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f, 1.0f};

    glUniformMatrix4fv(glGetUniformLocation(quad_shader->ID, "projection"), 1, GL_FALSE, projection);
    glUniform1f(glGetUniformLocation(quad_shader->ID, "time"), time_accumulator);
    glUniform2f(glGetUniformLocation(quad_shader->ID, "size"), 1.0f, 1.0f);
    glUniform1i(glGetUniformLocation(quad_shader->ID, "renderMode"), 1); // Gradient mode

    glBindVertexArray(vao_quads);

    // Create memory "terrain" with different layers
    float base_y = 0.1f;
    float mem_value = memory_used_graph.values.empty() ? 45.0f : memory_used_graph.values.back();

    // Used memory (bottom layer)
    float used_height = (mem_value / 100.0f) * 0.3f;
    glUniform2f(glGetUniformLocation(quad_shader->ID, "offset"), 0.1f, base_y);
    glUniform2f(glGetUniformLocation(quad_shader->ID, "scale"), 0.8f, used_height);
    glUniform3f(glGetUniformLocation(quad_shader->ID, "color"),
                MEMORY_COLOR[0], MEMORY_COLOR[1], MEMORY_COLOR[2]);
    glUniform1f(glGetUniformLocation(quad_shader->ID, "alpha"), 0.8f);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Cache layer (middle)
    float cache_height = 0.15f;
    glUniform2f(glGetUniformLocation(quad_shader->ID, "offset"), 0.1f, base_y + used_height);
    glUniform2f(glGetUniformLocation(quad_shader->ID, "scale"), 0.8f, cache_height);
    glUniform3f(glGetUniformLocation(quad_shader->ID, "color"),
                SECONDARY_COLOR[0], SECONDARY_COLOR[1], SECONDARY_COLOR[2]);
    glUniform1f(glGetUniformLocation(quad_shader->ID, "alpha"), 0.6f);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Available memory (top layer)
    float avail_height = 0.4f - used_height - cache_height;
    glUniform2f(glGetUniformLocation(quad_shader->ID, "offset"), 0.1f, base_y + used_height + cache_height);
    glUniform2f(glGetUniformLocation(quad_shader->ID, "scale"), 0.8f, avail_height);
    glUniform3f(glGetUniformLocation(quad_shader->ID, "color"),
                ACCENT_COLOR[0], ACCENT_COLOR[1], ACCENT_COLOR[2]);
    glUniform1f(glGetUniformLocation(quad_shader->ID, "alpha"), 0.4f);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Labels
    renderText("MEMORY LANDSCAPE", 0.02f, 0.85f, 0.04f);
    renderText("USED", 0.02f, base_y + used_height / 2, 0.025f);
    renderNumber(mem_value, 0.15f, base_y + used_height / 2, 0.02f, MEMORY_COLOR);
    renderText("CACHE", 0.02f, base_y + used_height + cache_height / 2, 0.02f);
    renderText("FREE", 0.02f, base_y + used_height + cache_height + avail_height / 2, 0.02f);
}

void BtopRenderer::renderNetworkFlow()
{
    // Network visualization with flowing particles
    renderText("NETWORK FLOW", 0.02f, 0.85f, 0.04f);

    if (!quad_shader)
        return;

    quad_shader->use();

    float projection[16] = {
        2.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 2.0f, 0.0f, 0.0f,
        0.0f, 0.0f, -1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f, 1.0f};

    glUniformMatrix4fv(glGetUniformLocation(quad_shader->ID, "projection"), 1, GL_FALSE, projection);
    glUniform1f(glGetUniformLocation(quad_shader->ID, "time"), time_accumulator);
    glUniform2f(glGetUniformLocation(quad_shader->ID, "size"), 1.0f, 1.0f);
    glUniform1i(glGetUniformLocation(quad_shader->ID, "renderMode"), 2); // Animated mode

    glBindVertexArray(vao_quads);

    // Simulate flowing data packets
    int packet_count = 20;
    for (int i = 0; i < packet_count; ++i)
    {
        float flow_progress = fmod(time_accumulator * 0.2f + i * 0.1f, 1.0f);

        // Download packets (blue, flowing down)
        float down_x = 0.2f + (i % 5) * 0.15f;
        float down_y = 0.8f - flow_progress * 0.6f;

        glUniform2f(glGetUniformLocation(quad_shader->ID, "offset"), down_x, down_y);
        glUniform2f(glGetUniformLocation(quad_shader->ID, "scale"), 0.02f, 0.03f);
        glUniform3f(glGetUniformLocation(quad_shader->ID, "color"),
                    NETWORK_RECV_COLOR[0], NETWORK_RECV_COLOR[1], NETWORK_RECV_COLOR[2]);
        glUniform1f(glGetUniformLocation(quad_shader->ID, "alpha"), 1.0f - flow_progress);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // Upload packets (orange, flowing up)
        float up_x = 0.6f + (i % 5) * 0.08f;
        float up_y = 0.2f + flow_progress * 0.6f;

        glUniform2f(glGetUniformLocation(quad_shader->ID, "offset"), up_x, up_y);
        glUniform2f(glGetUniformLocation(quad_shader->ID, "scale"), 0.015f, 0.025f);
        glUniform3f(glGetUniformLocation(quad_shader->ID, "color"),
                    NETWORK_SEND_COLOR[0], NETWORK_SEND_COLOR[1], NETWORK_SEND_COLOR[2]);
        glUniform1f(glGetUniformLocation(quad_shader->ID, "alpha"), 1.0f - flow_progress);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    // Network stats
    float recv_value = network_recv_graph.values.empty() ? 1024.0f : network_recv_graph.values.back();
    float send_value = network_send_graph.values.empty() ? 256.0f : network_send_graph.values.back();

    renderText("DOWNLOAD", 0.02f, 0.7f, 0.03f);
    renderNumber(recv_value, 0.02f, 0.65f, 0.025f, NETWORK_RECV_COLOR);
    renderText("KB/S", 0.15f, 0.65f, 0.02f);

    renderText("UPLOAD", 0.6f, 0.3f, 0.03f);
    renderNumber(send_value, 0.6f, 0.25f, 0.025f, NETWORK_SEND_COLOR);
    renderText("KB/S", 0.73f, 0.25f, 0.02f);
}

void BtopRenderer::renderProcessRain()
{
    // Matrix-style digital rain showing process activity
    renderText("PROCESS RAIN", 0.02f, 0.85f, 0.04f);

    // Simulate falling text streams
    if (!line_shader)
        return;

    line_shader->use();

    float projection[16] = {
        2.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 2.0f, 0.0f, 0.0f,
        0.0f, 0.0f, -1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f, 1.0f};

    glUniformMatrix4fv(glGetUniformLocation(line_shader->ID, "projection"), 1, GL_FALSE, projection);
    glUniform2f(glGetUniformLocation(line_shader->ID, "offset"), 0.0f, 0.0f);
    glUniform2f(glGetUniformLocation(line_shader->ID, "scale"), 1.0f, 1.0f);
    glUniform1f(glGetUniformLocation(line_shader->ID, "time"), time_accumulator);

    // Create vertical streams of "process data"
    int stream_count = 15;
    std::string process_chars = "0123456789ABCDEF";

    for (int stream = 0; stream < stream_count; ++stream)
    {
        float x = 0.05f + stream * 0.06f;
        float stream_speed = 0.1f + (stream % 3) * 0.05f;
        float stream_offset = fmod(time_accumulator * stream_speed, 1.2f);

        // Render characters falling down
        for (int i = 0; i < 20; ++i)
        {
            float y = 0.9f - stream_offset - i * 0.04f;
            if (y < -0.1f)
                continue;

            // Character intensity fades as it falls
            float intensity = std::max(0.0f, std::min(1.0f, (y + 0.1f) / 1.0f));

            glUniform3f(glGetUniformLocation(line_shader->ID, "color"),
                        CPU_COLOR[0] * intensity, CPU_COLOR[1] * intensity, CPU_COLOR[2] * intensity);
            glUniform1f(glGetUniformLocation(line_shader->ID, "alpha"), intensity);

            // Pick a "random" character based on position and time
            int char_index = (stream * 7 + i * 3 + static_cast<int>(time_accumulator * 10)) % process_chars.length();
            renderCharacter(process_chars[char_index], x, y, 0.02f, {CPU_COLOR[0] * intensity, CPU_COLOR[1] * intensity, CPU_COLOR[2] * intensity});
        }
    }
}

void BtopRenderer::renderDiskActivity()
{
    // Spinning disk visualization
    renderText("DISK ACTIVITY", 0.02f, 0.85f, 0.04f);

    if (!line_shader)
        return;

    line_shader->use();

    float projection[16] = {
        2.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 2.0f, 0.0f, 0.0f,
        0.0f, 0.0f, -1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f, 1.0f};

    glUniformMatrix4fv(glGetUniformLocation(line_shader->ID, "projection"), 1, GL_FALSE, projection);
    glUniform2f(glGetUniformLocation(line_shader->ID, "offset"), 0.0f, 0.0f);
    glUniform2f(glGetUniformLocation(line_shader->ID, "scale"), 1.0f, 1.0f);
    glUniform3f(glGetUniformLocation(line_shader->ID, "color"),
                ACCENT_COLOR[0], ACCENT_COLOR[1], ACCENT_COLOR[2]);
    glUniform1f(glGetUniformLocation(line_shader->ID, "alpha"), 0.8f);
    glUniform1f(glGetUniformLocation(line_shader->ID, "time"), time_accumulator);

    glBindVertexArray(vao_lines);

    // Draw spinning disk
    float center_x = 0.5f;
    float center_y = 0.5f;
    float disk_radius = 0.2f;
    int segments = 36;

    // Outer ring
    std::vector<float> circle_vertices;
    for (int i = 0; i <= segments; ++i)
    {
        float angle = (2.0f * M_PI * i) / segments;
        circle_vertices.push_back(center_x + cos(angle) * disk_radius);
        circle_vertices.push_back(center_y + sin(angle) * disk_radius);
    }

    glBindBuffer(GL_ARRAY_BUFFER, vbo_lines);
    glBufferData(GL_ARRAY_BUFFER, circle_vertices.size() * sizeof(float),
                 circle_vertices.data(), GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)0);

    glLineWidth(3.0f);
    glDrawArrays(GL_LINE_STRIP, 0, circle_vertices.size() / 2);

    // Spinning activity indicators
    int activity_lines = 8;
    for (int i = 0; i < activity_lines; ++i)
    {
        float angle = (2.0f * M_PI * i) / activity_lines + time_accumulator * 2.0f;
        float inner_radius = 0.05f;
        float outer_radius = disk_radius * (0.7f + 0.3f * sin(time_accumulator * 3.0f + i));

        float vertices[] = {
            center_x + cos(angle) * inner_radius, center_y + sin(angle) * inner_radius,
            center_x + cos(angle) * outer_radius, center_y + sin(angle) * outer_radius};

        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)0);

        glDrawArrays(GL_LINES, 0, 2);
    }

    renderText("DISK I/O", center_x - 0.04f, center_y - 0.02f, 0.025f);
}

void BtopRenderer::renderOverviewDashboard()
{
    // Combined dashboard view with mini versions of all visualizations
    renderText("SYSTEM OVERVIEW", 0.02f, 0.95f, 0.04f);

    // Mini CPU cores (top left)
    if (quad_shader)
    {
        quad_shader->use();

        float projection[16] = {
            2.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 2.0f, 0.0f, 0.0f,
            0.0f, 0.0f, -1.0f, 0.0f,
            -1.0f, -1.0f, 0.0f, 1.0f};

        glUniformMatrix4fv(glGetUniformLocation(quad_shader->ID, "projection"), 1, GL_FALSE, projection);
        glUniform1f(glGetUniformLocation(quad_shader->ID, "time"), time_accumulator);
        glUniform1i(glGetUniformLocation(quad_shader->ID, "renderMode"), 2);
        glBindVertexArray(vao_quads);

        // Mini CPU visualization
        for (int i = 0; i < 4; ++i)
        {
            float core_x = 0.05f + i * 0.03f;
            float core_y = 0.8f;
            float core_usage = 30.0f + 40.0f * sin(time_accumulator * 0.3f + i * 0.5f);
            float core_size = 0.02f;

            float intensity = core_usage / 100.0f;
            glUniform3f(glGetUniformLocation(quad_shader->ID, "color"),
                        CPU_COLOR[0] * intensity, CPU_COLOR[1] * intensity, CPU_COLOR[2] * intensity);
            glUniform2f(glGetUniformLocation(quad_shader->ID, "offset"), core_x, core_y);
            glUniform2f(glGetUniformLocation(quad_shader->ID, "scale"), core_size, core_size);
            glUniform1f(glGetUniformLocation(quad_shader->ID, "alpha"), 0.8f);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }
    }

    renderText("CPU", 0.05f, 0.75f, 0.02f);

    // Mini memory bars (middle)
    renderText("MEM", 0.3f, 0.75f, 0.02f);

    // Mini network flow (right)
    renderText("NET", 0.7f, 0.75f, 0.02f);

    // Classic graphs at bottom
    if (!cpu_total_graph.vertices.empty())
    {
        renderGraph(cpu_total_graph, 0.05f, 0.1f, 0.4f, 0.15f);
    }
    if (!memory_used_graph.vertices.empty())
    {
        renderGraph(memory_used_graph, 0.55f, 0.1f, 0.4f, 0.15f);
    }
}