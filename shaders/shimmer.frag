#version 440
// A highlight that sweeps across a layer from left to right: "Mixing" while two songs overlap.

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float phase; // 0 to 1: the highlight from just before the left edge to just past the right one
    float base;  // the opacity away from the highlight
    float band;  // half the highlight's width, as a fraction of the item's width
};

layout(binding = 1) uniform sampler2D source;

void main()
{
    float centre = mix(-band, 1.0 + band, phase);
    float glow = 1.0 - smoothstep(0.0, 1.0, abs(qt_TexCoord0.x - centre) / band);
    // Premultiplied, so scaling all four channels is correct.
    fragColor = texture(source, qt_TexCoord0) * (mix(base, 1.0, glow) * qt_Opacity);
}
