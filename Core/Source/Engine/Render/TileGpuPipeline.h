#pragma once

#include "Engine/Render/GpuResource.h"
#include "Engine/Render/TileVertex.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

#include <cstddef>
#include <filesystem>
#include <memory>
#include <span>

namespace psr {

// Owns the custom SDL_GPU pipeline that draws greyscale-authored sprite
// quads with a per-vertex two-tone palette swap (color_1/color_2 read in
// the fragment shader) -- the mechanism explicitly requested over
// SDL_GPURenderState, since that can't carry a second per-vertex color.
// Reads the SDL_GPUDevice already owned by renderer (SDL_GetGPURendererDevice)
// rather than creating/owning a device of its own.
class TileGpuPipeline
{
public:
    // vertex_shader_path/fragment_shader_path point at precompiled SPIR-V
    // bytecode (.spv, produced offline from the .glsl sources by
    // Scripts/Compile-Shaders.ps1) -- the device is created against the
    // "vulkan" GPU driver specifically (Application::Initialize), which
    // consumes SPIR-V natively. Paths must outlive construction only (read
    // synchronously, not retained).
    TileGpuPipeline(SDL_Renderer& renderer, const std::filesystem::path& vertex_shader_path,
                    const std::filesystem::path& fragment_shader_path);

    TileGpuPipeline(const TileGpuPipeline&) = delete;
    TileGpuPipeline& operator=(const TileGpuPipeline&) = delete;
    TileGpuPipeline(TileGpuPipeline&&) = delete;
    TileGpuPipeline& operator=(TileGpuPipeline&&) = delete;

    // false if shader compilation/pipeline creation failed -- callers
    // should skip drawing rather than dereference a null pipeline.
    bool IsLoaded() const { return m_pipeline != nullptr; }

    // vertices: a flat triangle list (6 verts/quad, no index buffer),
    // positions already NDC-space. Draws into an internal off-screen
    // SDL_GPUTexture sized to window_width x window_height (recreated on
    // size change), then composites that texture into renderer's current
    // render target via SDL_RenderTexture -- callers must issue this
    // before any later SDL_Renderer draws this frame that should layer on
    // top (e.g. RmlUi).
    void Draw(SDL_Renderer& renderer, SDL_GPUTexture& atlas_texture, std::span<const TileVertex> vertices,
              int window_width, int window_height);

private:
    void EnsureRenderTarget(SDL_Renderer& renderer, int width, int height);
    void EnsureVertexBufferCapacity(std::size_t vertex_count);

    SDL_GPUDevice* m_device = nullptr; // non-owning: renderer owns it.

    GpuGraphicsPipelinePtr m_pipeline;
    GpuSamplerPtr m_sampler;
    GpuBufferPtr m_vertex_buffer;
    GpuTransferBufferPtr m_transfer_buffer;
    std::size_t m_vertex_buffer_capacity = 0;

    GpuTexturePtr m_render_target;

    struct SDLTextureDeleter
    {
        void operator()(SDL_Texture* texture) const noexcept
        {
            if (texture)
                SDL_DestroyTexture(texture);
        }
    };
    // Declared after m_render_target so it is destroyed first (reverse
    // declaration order) -- the wrapper SDL_Texture must go before the
    // underlying SDL_GPUTexture it wraps is released.
    std::unique_ptr<SDL_Texture, SDLTextureDeleter> m_render_target_view;
    int m_render_target_width = 0;
    int m_render_target_height = 0;
};

} // namespace psr
