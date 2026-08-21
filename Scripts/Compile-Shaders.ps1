<#
.SYNOPSIS
    Compiles App/Assets/Shaders/*.glsl sources into SPIR-V bytecode (.spv).

.DESCRIPTION
    TileGpuPipeline (Core/Source/Engine/Render) loads precompiled SPIR-V at
    runtime and hands it directly to SDL_CreateGPUShader -- the GPU device is
    created against the "vulkan" driver specifically (Application::Initialize),
    which consumes SPIR-V natively with no cross-compilation step. GLSL is
    compiled offline via glslangValidator (a build-only vcpkg tool, not linked
    into the app).

    Re-run this whenever a .glsl shader source changes -- the compiled
    .spv files have no other source of truth and must be committed/kept in
    sync alongside the .glsl they were built from.
#>

$ShadersPath = Join-Path $PSScriptRoot "..\App\Assets\Shaders"
$GlslangValidator = Join-Path $PSScriptRoot "..\vcpkg_installed\x64-windows\tools\glslang\glslangValidator.exe"

if (-not (Test-Path $GlslangValidator))
{
    Write-Error "glslangValidator.exe not found at $GlslangValidator -- run vcpkg install first."
    exit 1
}

function Compile-Shader
{
    param(
        [string]$SourceName,
        [string]$Stage
    )

    $SourcePath = Join-Path $ShadersPath $SourceName
    $OutputPath = $SourcePath -replace '\.glsl$', '.spv'

    & $GlslangValidator -V -S $Stage -o $OutputPath $SourcePath
    if ($LASTEXITCODE -ne 0)
    {
        Write-Error "glslangValidator failed to compile $SourceName"
        exit 1
    }

    Write-Host "Wrote $OutputPath"
}

Compile-Shader -SourceName "TileSprite.vert.glsl" -Stage "vert"
Compile-Shader -SourceName "TileSprite.frag.glsl" -Stage "frag"
