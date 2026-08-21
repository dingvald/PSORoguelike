#pragma once

#include <SDL3/SDL_gpu.h>

#include <memory>

namespace psr {

// Generic RAII deleter for SDL_GPU objects whose release function takes
// (SDL_GPUDevice*, T*) -- e.g. SDL_ReleaseGPUTexture, SDL_ReleaseGPUBuffer.
// Stores the device pointer (non-owning) since these release calls need it.
template <typename T, void (*ReleaseFn)(SDL_GPUDevice*, T*)> struct GpuDeviceResourceDeleter
{
    SDL_GPUDevice* device = nullptr;

    void operator()(T* resource) const noexcept
    {
        if (resource && device)
            ReleaseFn(device, resource);
    }
};

template <typename T, void (*ReleaseFn)(SDL_GPUDevice*, T*)>
using GpuDeviceResourcePtr = std::unique_ptr<T, GpuDeviceResourceDeleter<T, ReleaseFn>>;

using GpuTexturePtr = GpuDeviceResourcePtr<SDL_GPUTexture, SDL_ReleaseGPUTexture>;
using GpuBufferPtr = GpuDeviceResourcePtr<SDL_GPUBuffer, SDL_ReleaseGPUBuffer>;
using GpuTransferBufferPtr = GpuDeviceResourcePtr<SDL_GPUTransferBuffer, SDL_ReleaseGPUTransferBuffer>;
using GpuSamplerPtr = GpuDeviceResourcePtr<SDL_GPUSampler, SDL_ReleaseGPUSampler>;
using GpuGraphicsPipelinePtr = GpuDeviceResourcePtr<SDL_GPUGraphicsPipeline, SDL_ReleaseGPUGraphicsPipeline>;
using GpuShaderPtr = GpuDeviceResourcePtr<SDL_GPUShader, SDL_ReleaseGPUShader>;

} // namespace psr
