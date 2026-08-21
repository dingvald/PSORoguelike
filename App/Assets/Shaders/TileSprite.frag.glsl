#version 450

// SDL_GPU's SPIR-V resource binding convention: fragment-stage sampled
// textures use descriptor set 2 (set 3 is reserved for fragment uniform
// buffers, unused here) -- see SDL_gpu.h's SDL_CreateGPUShader doc block.
layout(set = 2, binding = 0) uniform sampler2D AtlasTexture;

layout(location = 0) in vec2 in_uv;
layout(location = 1) in vec4 in_color1;
layout(location = 2) in vec4 in_color2;

layout(location = 0) out vec4 out_color;

void main()
{
    vec4 texel = texture(AtlasTexture, in_uv);
    float grey = texel.r; // authored greyscale, R == G == B by convention

    // Hard two-tone threshold at 0.5, boundary assigned to color_2's side.
    vec4 chosen = (grey < 0.5) ? in_color1 : in_color2;

    // Straight (non-premultiplied) alpha -- matches the pipeline's
    // SRC_ALPHA / ONE_MINUS_SRC_ALPHA blend state.
    float alpha = texel.a * chosen.a;
    out_color = vec4(chosen.rgb, alpha);
}
