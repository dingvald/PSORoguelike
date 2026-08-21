#pragma once

#include "Engine/Math/Vec2.h"
#include "Engine/Render/GpuResource.h"
#include "Engine/Render/TextureAtlasMath.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <unordered_map>

namespace psr {

// Recursively packs every .png found under a directory into one shared
// SDL_GPUTexture (sampled directly by TileGpuPipeline's fragment shader),
// and answers "what pixel rect on that texture is this sprite" by
// hashed_string(source image filename stem) + a per-image uv grid pick via
// TextureAtlasMath.h's ComputeSpriteSourceRect. renderer is only needed
// transiently during construction (to resolve the shared SDL_GPUDevice via
// SDL_GetGPURendererDevice and to upload the composited pixels) -- not
// retained as a member.
class TextureAtlas
{
public:
    TextureAtlas(SDL_Renderer& renderer, const std::filesystem::path& directory);

    TextureAtlas(const TextureAtlas&) = delete;
    TextureAtlas& operator=(const TextureAtlas&) = delete;
    TextureAtlas(TextureAtlas&&) noexcept = default;
    TextureAtlas& operator=(TextureAtlas&&) noexcept = default;

    // false if no source images loaded -- callers should skip drawing
    // rather than dereference a null texture.
    bool IsLoaded() const { return m_gpu_texture != nullptr; }

    SDL_GPUTexture* GetGpuTexture() const { return m_gpu_texture.get(); }

    // Pixel dimensions of the packed atlas -- needed by callers to
    // normalize GetSourceRect's pixel-space rect into [0,1] UVs.
    Vec2 GetSize() const { return m_size; }

    // texture_id: entt::hashed_string of a source image's filename stem
    // (e.g. "player" for "player.png"). texture_width/texture_height: one
    // sprite cell's size within that source image. uv_x/uv_y: zero-based
    // grid column/row within that image. Returns nullopt if texture_id
    // wasn't found among packed images -- callers should skip drawing
    // rather than guess.
    std::optional<SDL_FRect> GetSourceRect(std::uint32_t texture_id, int texture_width, int texture_height, int uv_x,
                                           int uv_y) const;

private:
    GpuTexturePtr m_gpu_texture;
    Vec2 m_size;
    std::unordered_map<std::uint32_t, TextureAtlasCellRect> m_placements;
};

} // namespace psr
