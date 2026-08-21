#include "Engine/Render/TileGpuPipeline.h"

#include "Engine/Render/ShaderCompiler.h"

#include <cstddef>
#include <cstring>

namespace psr {

namespace {
    constexpr SDL_GPUTextureFormat kRenderTargetFormat = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;

    template <typename T, void (*ReleaseFn)(SDL_GPUDevice*, T*)>
    GpuDeviceResourcePtr<T, ReleaseFn> MakeGpuPtr(T* resource, SDL_GPUDevice* device)
    {
        return GpuDeviceResourcePtr<T, ReleaseFn>(resource, GpuDeviceResourceDeleter<T, ReleaseFn>{device});
    }
} // namespace

TileGpuPipeline::TileGpuPipeline(SDL_Renderer& renderer, const std::filesystem::path& vertex_shader_path,
                                 const std::filesystem::path& fragment_shader_path)
    : m_device(SDL_GetGPURendererDevice(&renderer))
{
    if (!m_device)
    {
        SDL_Log("TileGpuPipeline: renderer is not GPU-backed (SDL_GetGPURendererDevice returned null)");
        return;
    }

    GpuShaderPtr vertex_shader = CompileGraphicsShaderFromSpirvFile(*m_device, vertex_shader_path,
                                                                    SDL_GPU_SHADERSTAGE_VERTEX, ShaderResourceCounts{});

    ShaderResourceCounts fragment_resource_counts{};
    fragment_resource_counts.num_samplers = 1;
    GpuShaderPtr fragment_shader = CompileGraphicsShaderFromSpirvFile(
        *m_device, fragment_shader_path, SDL_GPU_SHADERSTAGE_FRAGMENT, fragment_resource_counts);

    if (!vertex_shader || !fragment_shader)
        return;

    SDL_GPUVertexBufferDescription vertex_buffer_description{};
    vertex_buffer_description.slot = 0;
    vertex_buffer_description.pitch = sizeof(TileVertex);
    vertex_buffer_description.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    vertex_buffer_description.instance_step_rate = 0;

    SDL_GPUVertexAttribute vertex_attributes[4]{};
    vertex_attributes[0] = {0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, static_cast<Uint32>(offsetof(TileVertex, x))};
    vertex_attributes[1] = {1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, static_cast<Uint32>(offsetof(TileVertex, u))};
    vertex_attributes[2] = {2, 0, SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM,
                            static_cast<Uint32>(offsetof(TileVertex, color_1))};
    vertex_attributes[3] = {3, 0, SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM,
                            static_cast<Uint32>(offsetof(TileVertex, color_2))};

    SDL_GPUColorTargetBlendState blend_state{};
    blend_state.enable_blend = true;
    blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;

    SDL_GPUColorTargetDescription color_target_description{};
    color_target_description.format = kRenderTargetFormat;
    color_target_description.blend_state = blend_state;

    SDL_GPUGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.vertex_shader = vertex_shader.get();
    pipeline_info.fragment_shader = fragment_shader.get();
    pipeline_info.vertex_input_state.vertex_buffer_descriptions = &vertex_buffer_description;
    pipeline_info.vertex_input_state.num_vertex_buffers = 1;
    pipeline_info.vertex_input_state.vertex_attributes = vertex_attributes;
    pipeline_info.vertex_input_state.num_vertex_attributes = 4;
    pipeline_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pipeline_info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    pipeline_info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    pipeline_info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    pipeline_info.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
    pipeline_info.target_info.color_target_descriptions = &color_target_description;
    pipeline_info.target_info.num_color_targets = 1;
    pipeline_info.target_info.has_depth_stencil_target = false;

    SDL_GPUGraphicsPipeline* pipeline = SDL_CreateGPUGraphicsPipeline(m_device, &pipeline_info);
    if (!pipeline)
    {
        SDL_Log("TileGpuPipeline: SDL_CreateGPUGraphicsPipeline failed: %s", SDL_GetError());
        return;
    }
    m_pipeline = MakeGpuPtr<SDL_GPUGraphicsPipeline, SDL_ReleaseGPUGraphicsPipeline>(pipeline, m_device);

    SDL_GPUSamplerCreateInfo sampler_info{};
    sampler_info.min_filter = SDL_GPU_FILTER_NEAREST;
    sampler_info.mag_filter = SDL_GPU_FILTER_NEAREST;
    sampler_info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    sampler_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;

    SDL_GPUSampler* sampler = SDL_CreateGPUSampler(m_device, &sampler_info);
    if (!sampler)
    {
        SDL_Log("TileGpuPipeline: SDL_CreateGPUSampler failed: %s", SDL_GetError());
        m_pipeline.reset();
        return;
    }
    m_sampler = MakeGpuPtr<SDL_GPUSampler, SDL_ReleaseGPUSampler>(sampler, m_device);
}

void TileGpuPipeline::EnsureRenderTarget(SDL_Renderer& renderer, int width, int height)
{
    if (m_render_target && m_render_target_width == width && m_render_target_height == height)
        return;

    SDL_GPUTextureCreateInfo texture_info{};
    texture_info.type = SDL_GPU_TEXTURETYPE_2D;
    texture_info.format = kRenderTargetFormat;
    texture_info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    texture_info.width = static_cast<Uint32>(width);
    texture_info.height = static_cast<Uint32>(height);
    texture_info.layer_count_or_depth = 1;
    texture_info.num_levels = 1;
    texture_info.sample_count = SDL_GPU_SAMPLECOUNT_1;

    SDL_GPUTexture* texture = SDL_CreateGPUTexture(m_device, &texture_info);
    if (!texture)
    {
        SDL_Log("TileGpuPipeline: SDL_CreateGPUTexture (render target) failed: %s", SDL_GetError());
        return;
    }

    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetPointerProperty(props, SDL_PROP_TEXTURE_CREATE_GPU_TEXTURE_POINTER, texture);
    SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_FORMAT_NUMBER, SDL_PIXELFORMAT_RGBA32);
    SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_ACCESS_NUMBER, SDL_TEXTUREACCESS_TARGET);
    SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_WIDTH_NUMBER, width);
    SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_HEIGHT_NUMBER, height);
    SDL_Texture* wrapped = SDL_CreateTextureWithProperties(&renderer, props);
    SDL_DestroyProperties(props);

    if (!wrapped)
    {
        SDL_Log("TileGpuPipeline: SDL_CreateTextureWithProperties failed: %s", SDL_GetError());
        SDL_ReleaseGPUTexture(m_device, texture);
        return;
    }

    // Defaults to SDL_BLENDMODE_NONE, which makes the SDL_RenderTexture composite
    // below a literal overwrite -- fine for a single Draw() per frame (the erased
    // area is already the cleared background), but wrong the moment a second Draw()
    // composites its own transparent-cleared render target over what the first one
    // already drew.
    SDL_SetTextureBlendMode(wrapped, SDL_BLENDMODE_BLEND);

    m_render_target = MakeGpuPtr<SDL_GPUTexture, SDL_ReleaseGPUTexture>(texture, m_device);
    m_render_target_view.reset(wrapped);
    m_render_target_width = width;
    m_render_target_height = height;
}

void TileGpuPipeline::EnsureVertexBufferCapacity(std::size_t vertex_count)
{
    if (vertex_count <= m_vertex_buffer_capacity)
        return;

    Uint32 size_bytes = static_cast<Uint32>(vertex_count * sizeof(TileVertex));

    SDL_GPUBufferCreateInfo buffer_info{};
    buffer_info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    buffer_info.size = size_bytes;
    SDL_GPUBuffer* buffer = SDL_CreateGPUBuffer(m_device, &buffer_info);
    if (!buffer)
    {
        SDL_Log("TileGpuPipeline: SDL_CreateGPUBuffer failed: %s", SDL_GetError());
        return;
    }

    SDL_GPUTransferBufferCreateInfo transfer_info{};
    transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transfer_info.size = size_bytes;
    SDL_GPUTransferBuffer* transfer_buffer = SDL_CreateGPUTransferBuffer(m_device, &transfer_info);
    if (!transfer_buffer)
    {
        SDL_Log("TileGpuPipeline: SDL_CreateGPUTransferBuffer failed: %s", SDL_GetError());
        SDL_ReleaseGPUBuffer(m_device, buffer);
        return;
    }

    m_vertex_buffer = MakeGpuPtr<SDL_GPUBuffer, SDL_ReleaseGPUBuffer>(buffer, m_device);
    m_transfer_buffer = MakeGpuPtr<SDL_GPUTransferBuffer, SDL_ReleaseGPUTransferBuffer>(transfer_buffer, m_device);
    m_vertex_buffer_capacity = vertex_count;
}

void TileGpuPipeline::Draw(SDL_Renderer& renderer, SDL_GPUTexture& atlas_texture, std::span<const TileVertex> vertices,
                           int window_width, int window_height)
{
    if (!IsLoaded() || vertices.empty())
        return;

    EnsureRenderTarget(renderer, window_width, window_height);
    if (!m_render_target || !m_render_target_view)
        return;

    EnsureVertexBufferCapacity(vertices.size());
    if (!m_vertex_buffer || !m_transfer_buffer)
        return;

    SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(m_device);
    if (!command_buffer)
    {
        SDL_Log("TileGpuPipeline: SDL_AcquireGPUCommandBuffer failed: %s", SDL_GetError());
        return;
    }

    void* mapped = SDL_MapGPUTransferBuffer(m_device, m_transfer_buffer.get(), true);
    if (!mapped)
    {
        SDL_Log("TileGpuPipeline: SDL_MapGPUTransferBuffer failed: %s", SDL_GetError());
        return;
    }
    std::memcpy(mapped, vertices.data(), vertices.size() * sizeof(TileVertex));
    SDL_UnmapGPUTransferBuffer(m_device, m_transfer_buffer.get());

    SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(command_buffer);
    SDL_GPUTransferBufferLocation source{};
    source.transfer_buffer = m_transfer_buffer.get();
    source.offset = 0;
    SDL_GPUBufferRegion destination{};
    destination.buffer = m_vertex_buffer.get();
    destination.offset = 0;
    destination.size = static_cast<Uint32>(vertices.size() * sizeof(TileVertex));
    SDL_UploadToGPUBuffer(copy_pass, &source, &destination, true);
    SDL_EndGPUCopyPass(copy_pass);

    SDL_GPUColorTargetInfo color_target_info{};
    color_target_info.texture = m_render_target.get();
    color_target_info.mip_level = 0;
    color_target_info.layer_or_depth_plane = 0;
    color_target_info.clear_color = SDL_FColor{0.0f, 0.0f, 0.0f, 0.0f};
    color_target_info.load_op = SDL_GPU_LOADOP_CLEAR;
    color_target_info.store_op = SDL_GPU_STOREOP_STORE;
    // m_render_target is reused for every Draw() call this frame, but SDL's own
    // SDL_RenderTexture composite of a PREVIOUS call may still be "bound" (queued,
    // not yet executed) when this call clears and redraws the same texture --
    // exactly the cross-command-buffer data dependency SDL_GPU's cycling exists to
    // prevent (see SDL_gpu.h's "A Note On Cycling"). Without this, every composite
    // this frame ends up sampling whichever Draw() call happened to run last.
    color_target_info.cycle = true;

    SDL_GPURenderPass* render_pass = SDL_BeginGPURenderPass(command_buffer, &color_target_info, 1, nullptr);
    SDL_BindGPUGraphicsPipeline(render_pass, m_pipeline.get());

    SDL_GPUBufferBinding vertex_binding{};
    vertex_binding.buffer = m_vertex_buffer.get();
    vertex_binding.offset = 0;
    SDL_BindGPUVertexBuffers(render_pass, 0, &vertex_binding, 1);

    SDL_GPUTextureSamplerBinding sampler_binding{};
    sampler_binding.texture = &atlas_texture;
    sampler_binding.sampler = m_sampler.get();
    SDL_BindGPUFragmentSamplers(render_pass, 0, &sampler_binding, 1);

    SDL_DrawGPUPrimitives(render_pass, static_cast<Uint32>(vertices.size()), 1, 0, 0);
    SDL_EndGPURenderPass(render_pass);

    SDL_SubmitGPUCommandBuffer(command_buffer);

    SDL_RenderTexture(&renderer, m_render_target_view.get(), nullptr, nullptr);

    // SDL_RenderTexture doesn't composite immediately -- SDL batches 2D render
    // commands and only actually executes them at a flush point (e.g.
    // SDL_RenderPresent). Draw() reuses this single m_render_target across every
    // call in a frame, clearing and redrawing it each time via our own,
    // eagerly-submitted SDL_GPU command buffer above. Without an explicit flush
    // here, all of this frame's still-batched SDL_RenderTexture calls end up
    // sampling whatever m_render_target holds by the time SDL finally executes
    // them -- i.e. only the *last* Draw() call's content, composited N times over
    // an otherwise-empty background. Flushing after every composite forces SDL to
    // actually consume this call's content right now, before the next Draw() call
    // overwrites it.
    SDL_FlushRenderer(&renderer);
}

} // namespace psr
