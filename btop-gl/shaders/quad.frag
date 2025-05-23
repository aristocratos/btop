#version 330 core

in vec2 TexCoord;
in vec2 FragPos;
out vec4 FragColor;

uniform vec3 color;
uniform float alpha;
uniform float time;
uniform vec2 size;
uniform int renderMode; // 0 = solid, 1 = gradient, 2 = animated

void main() {
    vec3 finalColor = color;
    float finalAlpha = alpha;
    
    if (renderMode == 0) {
        // Solid mode with subtle pulse
        float pulse = sin(time * 2.0) * 0.15 + 0.85;
        finalColor = color * pulse;
        
    } else if (renderMode == 1) {
        // Enhanced vertical gradient with glow
        float gradient = 1.0 - TexCoord.y;
        gradient = pow(gradient, 1.5); // Make it more dramatic
        
        // Add some horizontal variation
        float hVariation = sin(TexCoord.x * 3.14159) * 0.1 + 0.9;
        
        // Create a glow at the bottom
        float bottomGlow = pow(1.0 - TexCoord.y, 3.0) * 1.5;
        
        finalColor = mix(color * 0.2, color * (1.2 + bottomGlow), gradient * hVariation);
        
    } else if (renderMode == 2) {
        // Animated mode with multiple effects
        float pulse = sin(time * 4.0 + FragPos.x * 0.1) * 0.3 + 0.7;
        float wave = sin(time * 2.0 + FragPos.y * 0.05) * 0.2 + 0.8;
        
        // Add animated border effect
        vec2 center = vec2(0.5);
        float dist = distance(TexCoord, center);
        float border = smoothstep(0.3, 0.5, dist);
        
        // Create ripple effect
        float ripple = sin(dist * 20.0 - time * 5.0) * 0.1 + 0.9;
        
        finalColor = color * pulse * wave * ripple;
        finalAlpha = alpha * (1.0 - border * 0.2);
        
        // Add some sparkle
        float sparkle = sin(time * 10.0 + FragPos.x * FragPos.y * 50.0) * 0.1 + 0.9;
        finalColor *= sparkle;
    }
    
    // Global enhancement
    finalColor = finalColor * 1.2; // Make everything brighter
    
    FragColor = vec4(finalColor, finalAlpha);
} 