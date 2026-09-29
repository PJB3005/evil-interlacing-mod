#pragma once

#include "webgpu/webgpu_cpp.h"

namespace slugcat::interlace::pipelines {

constexpr uint32_t kVertexDrawCount = 3;

extern wgpu::Sampler gSampler;
extern wgpu::BindGroupLayout gBindGroupLayout;

wgpu::RenderPipeline const& InterlacePipelineFor(wgpu::TextureFormat colorFormat, wgpu::TextureFormat depthFormat);

void Init();

} // namespace slugcat::interlace::pipelines
