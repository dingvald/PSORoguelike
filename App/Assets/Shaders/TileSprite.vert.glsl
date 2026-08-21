#version 450

// Positions are pre-baked into clip space on the CPU (TileRenderer::Draw),
// so this is a pure passthrough -- no uniform buffer needed.

layout(location = 0) in vec2 in_position;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec4 in_color1;
layout(location = 3) in vec4 in_color2;

layout(location = 0) out vec2 out_uv;
layout(location = 1) out vec4 out_color1;
layout(location = 2) out vec4 out_color2;

void main()
{
    gl_Position = vec4(in_position, 0.0, 1.0);
    out_uv = in_uv;
    out_color1 = in_color1;
    out_color2 = in_color2;
}
