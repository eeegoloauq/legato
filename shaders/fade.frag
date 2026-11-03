#version 440
// Fades the left and right edges of a layer to transparent, where scrolling text runs out of view.

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float fadeLeft;  // 0 sharp, 1 faded
    float fadeRight;
    float edge;      // fade width as a fraction of the item's width
};

layout(binding = 1) uniform sampler2D source;

void main()
{
    float x = qt_TexCoord0.x;
    float left = mix(1.0, smoothstep(0.0, edge, x), fadeLeft);
    float right = mix(1.0, smoothstep(0.0, edge, 1.0 - x), fadeRight);
    // Premultiplied, so scaling all four channels is correct.
    fragColor = texture(source, qt_TexCoord0) * (left * right * qt_Opacity);
}
