#pragma once

#include "Engine/Render/GpuResource.h"

#include <SDL3/SDL_gpu.h>

#include <cstdint>
#include <filesystem>

namespace psr {

// Resource counts a shader declares -- mirrors the fields SDL_GPUShaderCreateInfo
// needs beyond the raw bytecode itself. Zero-initialized members mean "this
// shader binds none of this resource kind".
struct ShaderResourceCounts
{
    std::uint32_t num_samplers = 0;
    std::uint32_t num_storage_textures = 0;
    std::uint32_t num_storage_buffers = 0;
    std::uint32_t num_uniform_buffers = 0;
};

// Reads spirv_path's contents (precompiled SPIR-V bytecode, produced
// offline from the .glsl sources by Scripts/Compile-Shaders.ps1) and hands
// it directly to SDL_CreateGPUShader -- device is created against the
// "vulkan" GPU driver specifically (Application::Initialize), which
// consumes SPIR-V natively with no cross-compilation step. Returns a null
// GpuShaderPtr on failure (logged via SDL_Log).
GpuShaderPtr CompileGraphicsShaderFromSpirvFile(SDL_GPUDevice& device, const std::filesystem::path& spirv_path,
                                                SDL_GPUShaderStage stage, const ShaderResourceCounts& resource_counts);

} // namespace psr
