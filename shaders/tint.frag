#version 440
// The colored top of an album or artist page: two soft ellipses in
// the cover's colors, fading out toward the bottom.

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 color1;
    vec4 color2;
};

// 1 at the center of an ellipse with radii `r`, 0 at its edge and beyond.
float ellipse(vec2 p, vec2 center, vec2 r)
{
    return 1.0 - smoothstep(0.0, 1.0, length((p - center) / r));
}

void main()
{
    vec2 p = qt_TexCoord0;
    float a1 = 0.5 * ellipse(p, vec2(0.15, 0.0), vec2(1.1, 0.9) * 0.7);
    float a2 = 0.42 * ellipse(p, vec2(0.9, 0.05), vec2(0.8, 0.8) * 0.7);
    // Solid down to 30 %, then gone by the bottom.
    float mask = 1.0 - smoothstep(0.3, 1.0, p.y);
    // Premultiplied: the second ellipse over the first.
    vec3 rgb = color2.rgb * a2 + color1.rgb * a1 * (1.0 - a2);
    float alpha = a2 + a1 * (1.0 - a2);
    fragColor = vec4(rgb, alpha) * (mask * 0.9 * qt_Opacity);
}
