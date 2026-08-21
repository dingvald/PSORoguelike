#include "Engine/Render/TextureAtlas.h"

#include "Engine/Render/TextureAtlasPacker.h"

#include <SDL3_image/SDL_image.h>
#include <entt/entt.hpp>

#include <cctype>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace psr {

namespace {
    constexpr int kMaxAtlasWidth = 1024;

    struct LoadedImage
    {
        std::uint32_t id = 0;
        SDL_Surface* surface = nullptr;
        std::filesystem::path path;
    };

    bool IsPngFile(const std::filesystem::directory_entry& entry)
    {
        if (!entry.is_regular_file())
            return false;
        std::string extension = entry.path().extension().string();
        for (char& c : extension)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return extension == ".png";
    }

    // Recursively loads every .png under directory, keyed by filename stem.
    // Later directory-iteration order wins on a stem collision (logged) --
    // iteration order is filesystem-dependent, but this is strictly a
    // content-authoring error path (two source images sharing a stem), not
    // one expected to matter in practice.
    std::vector<LoadedImage> LoadSourceImages(const std::filesystem::path& directory)
    {
        std::vector<LoadedImage> images;
        std::unordered_map<std::uint32_t, size_t> index_by_id;

        std::error_code error_code;
        for (const std::filesystem::directory_entry& entry :
             std::filesystem::recursive_directory_iterator(directory, error_code))
        {
            if (!IsPngFile(entry))
                continue;

            SDL_Surface* surface = IMG_Load(entry.path().string().c_str());
            if (!surface)
            {
                SDL_Log("TextureAtlas: failed to load '%s': %s", entry.path().string().c_str(), SDL_GetError());
                continue;
            }

            std::uint32_t id = entt::hashed_string{entry.path().stem().string().c_str()}.value();

            auto existing = index_by_id.find(id);
            if (existing != index_by_id.end())
            {
                SDL_Log("TextureAtlas: '%s' collides with '%s' (same filename stem) -- the later one wins",
                        entry.path().string().c_str(), images[existing->second].path.string().c_str());
                SDL_DestroySurface(images[existing->second].surface);
                images[existing->second] = LoadedImage{id, surface, entry.path()};
            }
            else
            {
                index_by_id.emplace(id, images.size());
                images.push_back(LoadedImage{id, surface, entry.path()});
            }
        }

        return images;
    }
} // namespace

TextureAtlas::TextureAtlas(SDL_Renderer& renderer, const std::filesystem::path& directory)
{
    std::vector<LoadedImage> images = LoadSourceImages(directory);
    if (images.empty())
    {
        SDL_Log("TextureAtlas: no .png source images found under '%s'", directory.string().c_str());
        return;
    }

    std::vector<PackerInputRect> rects;
    rects.reserve(images.size());
    for (const LoadedImage& image : images)
        rects.push_back(PackerInputRect{image.id, image.surface->w, image.surface->h});

    PackedAtlasLayout layout = PackTextureAtlas(std::move(rects), kMaxAtlasWidth);

    SDL_Surface* atlas_surface = SDL_CreateSurface(layout.atlas_width, layout.atlas_height, SDL_PIXELFORMAT_RGBA32);
    if (!atlas_surface)
    {
        SDL_Log("TextureAtlas: failed to create %dx%d composite surface: %s", layout.atlas_width, layout.atlas_height,
                SDL_GetError());
        for (const LoadedImage& image : images)
            SDL_DestroySurface(image.surface);
        return;
    }
    SDL_FillSurfaceRect(atlas_surface, nullptr, SDL_MapSurfaceRGBA(atlas_surface, 0, 0, 0, 0));

    std::unordered_map<std::uint32_t, SDL_Surface*> surface_by_id;
    for (const LoadedImage& image : images)
        surface_by_id.emplace(image.id, image.surface);

    for (const PackerPlacement& placement : layout.placements)
    {
        SDL_Surface* source = surface_by_id.at(placement.id);
        SDL_SetSurfaceBlendMode(source, SDL_BLENDMODE_NONE);
        SDL_Rect dst{placement.x, placement.y, placement.width, placement.height};
        SDL_BlitSurface(source, nullptr, atlas_surface, &dst);
    }

    for (const LoadedImage& image : images)
        SDL_DestroySurface(image.surface);

    // Upload the composited surface into an SDL_GPUTexture (sampled by
    // TileGpuPipeline's fragment shader), sharing the SDL_Renderer's own
    // GPU device rather than owning a separate one.
    SDL_GPUDevice* device = SDL_GetGPURendererDevice(&renderer);
    if (!device)
    {
        SDL_Log("TextureAtlas: renderer is not GPU-backed (SDL_GetGPURendererDevice returned null)");
        SDL_DestroySurface(atlas_surface);
        return;
    }

    SDL_GPUTextureCreateInfo texture_info{};
    texture_info.type = SDL_GPU_TEXTURETYPE_2D;
    texture_info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    texture_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    texture_info.width = static_cast<Uint32>(atlas_surface->w);
    texture_info.height = static_cast<Uint32>(atlas_surface->h);
    texture_info.layer_count_or_depth = 1;
    texture_info.num_levels = 1;
    texture_info.sample_count = SDL_GPU_SAMPLECOUNT_1;

    SDL_GPUTexture* gpu_texture = SDL_CreateGPUTexture(device, &texture_info);
    if (!gpu_texture)
    {
        SDL_Log("TextureAtlas: SDL_CreateGPUTexture failed: %s", SDL_GetError());
        SDL_DestroySurface(atlas_surface);
        return;
    }

    Uint32 row_bytes = static_cast<Uint32>(atlas_surface->w) * 4;
    Uint32 pixel_bytes = row_bytes * static_cast<Uint32>(atlas_surface->h);

    SDL_GPUTransferBufferCreateInfo transfer_info{};
    transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transfer_info.size = pixel_bytes;
    SDL_GPUTransferBuffer* transfer_buffer = SDL_CreateGPUTransferBuffer(device, &transfer_info);
    if (!transfer_buffer)
    {
        SDL_Log("TextureAtlas: SDL_CreateGPUTransferBuffer failed: %s", SDL_GetError());
        SDL_ReleaseGPUTexture(device, gpu_texture);
        SDL_DestroySurface(atlas_surface);
        return;
    }

    void* mapped = SDL_MapGPUTransferBuffer(device, transfer_buffer, false);
    if (!mapped)
    {
        SDL_Log("TextureAtlas: SDL_MapGPUTransferBuffer failed: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, transfer_buffer);
        SDL_ReleaseGPUTexture(device, gpu_texture);
        SDL_DestroySurface(atlas_surface);
        return;
    }

    // atlas_surface->pitch may exceed width*4 (row padding) -- copy row by
    // row into the tightly-packed transfer buffer rather than one memcpy.
    const auto* src_bytes = static_cast<const std::uint8_t*>(atlas_surface->pixels);
    auto* dst_bytes = static_cast<std::uint8_t*>(mapped);
    for (int row = 0; row < atlas_surface->h; ++row)
        std::memcpy(dst_bytes + row * row_bytes, src_bytes + row * atlas_surface->pitch, row_bytes);

    SDL_UnmapGPUTransferBuffer(device, transfer_buffer);

    SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(device);
    if (command_buffer)
    {
        SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(command_buffer);

        SDL_GPUTextureTransferInfo source{};
        source.transfer_buffer = transfer_buffer;
        source.offset = 0;
        source.pixels_per_row = static_cast<Uint32>(atlas_surface->w);
        source.rows_per_layer = static_cast<Uint32>(atlas_surface->h);

        SDL_GPUTextureRegion destination{};
        destination.texture = gpu_texture;
        destination.mip_level = 0;
        destination.layer = 0;
        destination.x = 0;
        destination.y = 0;
        destination.z = 0;
        destination.w = static_cast<Uint32>(atlas_surface->w);
        destination.h = static_cast<Uint32>(atlas_surface->h);
        destination.d = 1;

        SDL_UploadToGPUTexture(copy_pass, &source, &destination, false);
        SDL_EndGPUCopyPass(copy_pass);
        SDL_SubmitGPUCommandBuffer(command_buffer);
    }
    else
    {
        SDL_Log("TextureAtlas: SDL_AcquireGPUCommandBuffer failed: %s", SDL_GetError());
    }

    SDL_ReleaseGPUTransferBuffer(device, transfer_buffer);

    m_gpu_texture = GpuTexturePtr(gpu_texture, GpuDeviceResourceDeleter<SDL_GPUTexture, SDL_ReleaseGPUTexture>{device});
    m_size = Vec2{atlas_surface->w, atlas_surface->h};

    SDL_DestroySurface(atlas_surface);

    for (const PackerPlacement& placement : layout.placements)
        m_placements.emplace(placement.id,
                             TextureAtlasCellRect{placement.x, placement.y, placement.width, placement.height});
}

std::optional<SDL_FRect> TextureAtlas::GetSourceRect(std::uint32_t texture_id, int texture_width, int texture_height,
                                                     int uv_x, int uv_y) const
{
    auto it = m_placements.find(texture_id);
    if (it == m_placements.end())
        return std::nullopt;

    TextureAtlasCellRect rect = ComputeSpriteSourceRect(it->second, texture_width, texture_height, uv_x, uv_y);
    return SDL_FRect{static_cast<float>(rect.x), static_cast<float>(rect.y), static_cast<float>(rect.width),
                     static_cast<float>(rect.height)};
}

} // namespace psr
