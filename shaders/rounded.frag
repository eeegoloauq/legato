#version 440
// A texture with rounded, antialiased corners. Used for covers: one quad, no offscreen pass,
// unlike a mask effect per cover.

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 itemSize; // logical pixels
    float radius;
    float dpr;
};

layout(binding = 1) uniform sampler2D source;

void main()
{
    vec2 halfSize = itemSize * 0.5;
    vec2 q = abs(qt_TexCoord0 * itemSize - halfSize) - (halfSize - vec2(radius));
    float distance = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
    // One device pixel of antialiasing.
    float coverage = clamp(0.5 - distance * dpr, 0.0, 1.0);
    // The texture is premultiplied, so scaling all four channels is correct.
    fragColor = texture(source, qt_TexCoord0) * coverage * qt_Opacity;
}
