#version 330 core

out vec4 FragColor;

uniform vec3 color;
uniform float alpha;
uniform float time;

void main() {
    // Enhanced multi-layered glow effect
    float pulse = sin(time * 3.0) * 0.3 + 0.7;
    float fastPulse = sin(time * 8.0) * 0.1 + 0.9;
    
    // Create a breathing effect
    float breathe = sin(time * 1.5) * 0.2 + 0.8;
    
    // Combine effects for dynamic glow
    float glow = pulse * fastPulse * breathe;
    
    // Add rainbow shimmer effect
    vec3 shimmer = vec3(
        sin(time * 2.0 + 0.0) * 0.1 + 0.9,
        sin(time * 2.0 + 2.094) * 0.1 + 0.9,
        sin(time * 2.0 + 4.188) * 0.1 + 0.9
    );
    
    // Final color with enhanced glow and shimmer
    vec3 finalColor = color * glow * shimmer;
    
    // Add some extra brightness
    finalColor = finalColor * 1.3;
    
    FragColor = vec4(finalColor, alpha);
} 