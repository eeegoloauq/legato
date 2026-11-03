#version 440
// Soft color blobs drifting over a base color (docs/design.md, "Generated covers"). Drawn per pixel
// in one pass, so drifting costs no blur and no offscreen texture.

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 itemSize;
    float radius;
    float dpr;
    // 0..2π over one cycle. Every motion uses whole multiples of it, so the loop has no seam.
    float phase;
    vec4 color1;
    vec4 color2;
    vec4 color3;
};

float blob(vec2 p, vec2 center, float size)
{
    vec2 d = p - center;
    return exp(-dot(d, d) / (size * size));
}

void main()
{
    float aspect = itemSize.x / max(itemSize.y, 1.0);
    vec2 p = vec2(qt_TexCoord0.x * aspect, qt_TexCoord0.y);
    float t = phase;

    vec2 c2 = vec2((0.15 + 0.20 * sin(t) + 0.06 * sin(2.0 * t)) * aspect, 0.20 + 0.18 * cos(t));
    vec2 c3 = vec2((0.85 - 0.22 * sin(t + 1.3)) * aspect, 0.85 - 0.16 * sin(2.0 * t + 0.4));
    vec2 c4 = vec2((0.50 + 0.25 * cos(t + 2.1)) * aspect, 0.55 + 0.22 * sin(t - 0.7));

    vec3 color = color1.rgb;
    color = mix(color, color2.rgb, 0.9 * blob(p, c2, 0.55));
    color = mix(color, color3.rgb, 0.9 * blob(p, c3, 0.60));
    color = mix(color, mix(color2.rgb, color3.rgb, 0.45), 0.7 * blob(p, c4, 0.40));

    // A little noise keeps the soft gradients from banding.
    float noise = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453);
    color += (noise - 0.5) / 255.0;

    vec2 halfSize = itemSize * 0.5;
    vec2 q = abs(qt_TexCoord0 * itemSize - halfSize) - (halfSize - vec2(radius));
    float distance = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
    float coverage = clamp(0.5 - distance * dpr, 0.0, 1.0);

    fragColor = vec4(color, 1.0) * coverage * qt_Opacity;
}
