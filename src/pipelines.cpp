#include "pipelines.hpp"

#include "fmt/os.h"
#include "mod.hpp"
#include "mods/svc/log.hpp"
#include "mods/svc/resource.h"

#include <stdexcept>
#include <string>

namespace slugcat::interlace::pipelines {

wgpu::Sampler gSampler;
wgpu::BindGroupLayout gBindGroupLayout;
wgpu::BindGroupLayout gBlitBindGroupLayout;
wgpu::RenderPipeline gBlitPipeline;

namespace {

using namespace std::string_view_literals;

wgpu::ShaderModule gVertexModule;
wgpu::ShaderModule gInterlaceModule;
wgpu::ShaderModule gBlitModule;
wgpu::PipelineLayout gPipelineLayout;

wgpu::TextureFormat gLastColorFormat;
wgpu::TextureFormat gLastDepthFormat;
wgpu::RenderPipeline gCachedPipeline;

std::string LoadShaderSource(char const *path) {
    mods::log::trace("Loading shader {} source...", path);

    ResourceBuffer buffer RESOURCE_BUFFER_INIT;
    auto const result = svc_resource->load(mod_ctx, path, &buffer);
    if (result != MOD_OK) {
        throw std::runtime_error("Failed to load shader source");
    }

    std::string str(static_cast<const char *>(buffer.data),
                    static_cast<const char *>(buffer.data) + buffer.size);

    svc_resource->free(mod_ctx, &buffer);

    return str;
}

wgpu::ShaderModule CompileShaderModule(char const *path) {
    auto const shaderSource = LoadShaderSource(path);
    wgpu::ShaderSourceWGSL shaderSourceWgsl;
    shaderSourceWgsl.code = std::string_view(shaderSource);
    wgpu::ShaderModuleDescriptor const shaderModuleDesc{
        .nextInChain = &shaderSourceWgsl,
        .label = path,
    };

    return gDevice.CreateShaderModule(&shaderModuleDesc);
}

wgpu::RenderPipeline MakeInterlacePipeline(wgpu::TextureFormat const colorFormat,
                                           wgpu::TextureFormat const depthFormat) {
    wgpu::DepthStencilState const depthStencilState{
        .format = depthFormat,
        .depthWriteEnabled = false,
        .depthCompare = wgpu::CompareFunction::Always,
    };

    wgpu::ColorTargetState const colorTargets[]{
        {
            .format = colorFormat,
        },
    };

    wgpu::FragmentState const fragmentState{
        .module = gInterlaceModule,
        .targetCount = std::size(colorTargets),
        .targets = colorTargets,
    };

    wgpu::RenderPipelineDescriptor const desc{
        .label = "Interlace pipeline"sv,
        .layout = gPipelineLayout,
        .vertex =
            {
                .module = gVertexModule,
                .entryPoint = "vs_main"sv,
            },
        .primitive =
            {
                .topology = wgpu::PrimitiveTopology::TriangleList,
            },
        .depthStencil = &depthStencilState,
        .fragment = &fragmentState,
    };

    return gDevice.CreateRenderPipeline(&desc);
}

} // namespace

wgpu::RenderPipeline const &InterlacePipelineFor(wgpu::TextureFormat const colorFormat,
                                                 wgpu::TextureFormat const depthFormat) {
    if (gLastColorFormat == colorFormat && gLastDepthFormat == depthFormat) {
        return gCachedPipeline;
    }

    gCachedPipeline = MakeInterlacePipeline(colorFormat, depthFormat);
    gLastColorFormat = colorFormat;
    gLastDepthFormat = depthFormat;

    return gCachedPipeline;
}

void Init() {
    gVertexModule = CompileShaderModule("epic_vertex.wgsl");
    gInterlaceModule = CompileShaderModule("interlace.wgsl");
    gBlitModule = CompileShaderModule("blit.wgsl");

    constexpr static wgpu::BindGroupLayoutEntry entries[]{
        {
            .binding = 0,
            .visibility = wgpu::ShaderStage::Fragment,
            .sampler = {.type = wgpu::SamplerBindingType::Filtering},
        },
        {
            .binding = 1,
            .visibility = wgpu::ShaderStage::Fragment,
            .texture =
                {
                    .sampleType = wgpu::TextureSampleType::Float,
                    .viewDimension = wgpu::TextureViewDimension::e2D,
                },
        },
        {
            .binding = 2,
            .visibility = wgpu::ShaderStage::Fragment,
            .texture =
                {
                    .sampleType = wgpu::TextureSampleType::Float,
                    .viewDimension = wgpu::TextureViewDimension::e2D,
                },
        },
        {
            .binding = 3,
            .visibility = wgpu::ShaderStage::Fragment,
            .buffer =
                {
                    .type = wgpu::BufferBindingType::Uniform,
                    .hasDynamicOffset = false,
                    .minBindingSize = sizeof(UniformsInterlace),
                },
        },
    };

    constexpr static wgpu::BindGroupLayoutDescriptor bgLayoutDesc{
        .entryCount = std::size(entries),
        .entries = entries,
    };

    gBindGroupLayout = gDevice.CreateBindGroupLayout(&bgLayoutDesc);

    constexpr wgpu::PipelineLayoutDescriptor pipelineLayoutDesc{
        .bindGroupLayoutCount = 1,
        .bindGroupLayouts = &gBindGroupLayout,
    };

    gPipelineLayout = gDevice.CreatePipelineLayout(&pipelineLayoutDesc);

    constexpr static wgpu::SamplerDescriptor samplerDesc{
        .label = "Interlacing sampler",
        .addressModeU = wgpu::AddressMode::ClampToEdge,
        .addressModeV = wgpu::AddressMode::ClampToEdge,
        .magFilter = wgpu::FilterMode::Linear,
        .minFilter = wgpu::FilterMode::Linear,
    };

    gSampler = gDevice.CreateSampler(&samplerDesc);

    constexpr static wgpu::ColorTargetState colorTargets[]{
        {
            .format = kBlitTargetFormat,
        },
    };

    wgpu::FragmentState const fragmentState{
        .module = gBlitModule,
        .targetCount = std::size(colorTargets),
        .targets = colorTargets,
    };

    wgpu::RenderPipelineDescriptor const blitDesc{
        .label = "Blit pipeline"sv,
        .vertex = {.module = gVertexModule, .entryPoint = "vs_main"sv},
        .primitive =
            {
                .topology = wgpu::PrimitiveTopology::TriangleList,
            },
        .fragment = &fragmentState,
    };

    gBlitPipeline = gDevice.CreateRenderPipeline(&blitDesc);
    gBlitBindGroupLayout = gBlitPipeline.GetBindGroupLayout(0);
}

} // namespace slugcat::interlace::pipelines
