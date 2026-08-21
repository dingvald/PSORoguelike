#include "Engine/Render/ShaderCompiler.h"

#include <SDL3/SDL.h>

#include <fstream>
#include <vector>

namespace psr {

namespace {
    std::vector<std::uint8_t> ReadFileToBytes(const std::filesystem::path& path)
    {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file)
            return {};

        std::streamsize size = file.tellg();
        if (size <= 0)
            return {};
        file.seekg(0, std::ios::beg);

        std::vector<std::uint8_t> bytes(static_cast<size_t>(size));
        if (!file.read(reinterpret_cast<char*>(bytes.data()), size))
            return {};
        return bytes;
    }
} // namespace

GpuShaderPtr CompileGraphicsShaderFromSpirvFile(SDL_GPUDevice& device, const std::filesystem::path& spirv_path,
                                                SDL_GPUShaderStage stage, const ShaderResourceCounts& resource_counts)
{
    std::vector<std::uint8_t> bytecode = ReadFileToBytes(spirv_path);
    if (bytecode.empty())
    {
        SDL_Log("ShaderCompiler: failed to read '%s'", spirv_path.string().c_str());
        return {};
    }

    SDL_GPUShaderCreateInfo create_info{};
    create_info.code_size = bytecode.size();
    create_info.code = bytecode.data();
    create_info.entrypoint = "main";
    create_info.format = SDL_GPU_SHADERFORMAT_SPIRV;
    create_info.stage = stage;
    create_info.num_samplers = resource_counts.num_samplers;
    create_info.num_storage_textures = resource_counts.num_storage_textures;
    create_info.num_storage_buffers = resource_counts.num_storage_buffers;
    create_info.num_uniform_buffers = resource_counts.num_uniform_buffers;
    create_info.props = 0;

    SDL_GPUShader* shader = SDL_CreateGPUShader(&device, &create_info);
    if (!shader)
    {
        SDL_Log("ShaderCompiler: SDL_CreateGPUShader failed for '%s': %s", spirv_path.string().c_str(), SDL_GetError());
        return {};
    }

    return GpuShaderPtr{shader, GpuDeviceResourceDeleter<SDL_GPUShader, SDL_ReleaseGPUShader>{&device}};
}

} // namespace psr
