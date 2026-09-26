#include "mod.hpp"

#include "config.hpp"
#include "mod_helpers.hpp"
#include "pipelines.hpp"

#include "mods/service.hpp"
#include "mods/svc/gfx.h"
#include "mods/svc/log.hpp"

#include "webgpu/webgpu_cpp.h"

namespace slugcat::interlace {

wgpu::Device gDevice;

namespace {

using namespace mod_helpers;
using namespace std::string_view_literals;

struct DrawPayload {
    bool evenField;
    bool halfsies;
    wgpu::TextureView color;
    GfxRange uniformRange;
};

struct CopyPayload {
    wgpu::TextureView color;
    uint32_t width;
    uint32_t height;
    GfxRange uniformRange;
};

GfxDrawTypeHandle gDrawTypeHandle;
GfxComputeTypeHandle gCopyTypeHandle;

bool gIsEvenField;
uint32_t gLastWidth;
uint32_t gLastHeight;
wgpu::Texture gLastField;
wgpu::TextureView gLastFieldView;

void StageHook(ModContext *, const GfxStageContext *, void *) {
    constexpr GfxResolveDesc desc{
        .struct_size = sizeof(desc),
        .color = true,
        .depth = false,
        .normal = 0,
    };

    GfxResolvedTargets resolved GFX_RESOLVED_TARGETS_INIT;

    auto const result = svc_gfx->resolve_pass(mod_ctx, &desc, &resolved);
    if (result != MOD_OK) {
        mods::log::error("Failed to resolve frame! {}", ResultMessage(result));
        return;
    }

    gIsEvenField ^= true;
    auto const halfsies = config::GetHalfsiesMode();

    UniformsInterlace const uniforms{
        static_cast<float>(resolved.width),
        static_cast<float>(resolved.height),
    };

    GfxRange range;
    CheckResult(svc_gfx->push_uniform(mod_ctx, &uniforms, sizeof(uniforms), &range),
                "push_uniform");

    auto *payload = new DrawPayload{
        gIsEvenField,
        halfsies,
        resolved.color,
        range,
    };

    CheckResult(svc_gfx->push_draw(mod_ctx, gDrawTypeHandle, &payload, sizeof(payload)),
                "push_draw");

    auto actuallyCopy = true;
    if (halfsies) {
        actuallyCopy = gIsEvenField;
    }

    if (actuallyCopy) {
        auto *payload2 = new CopyPayload{
            resolved.color,
            resolved.width,
            resolved.height,
            range,
        };
        CheckResult(svc_gfx->push_compute(mod_ctx, gCopyTypeHandle, &payload2, sizeof(payload2)),
                    "push_compute");
    }
}

void DrawHook(ModContext *,
              const GfxDrawContext *draw_ctx,
              const void *payload,
              size_t payload_size,
              void *) {
    assert(payload_size == sizeof(DrawPayload const *));
    std::unique_ptr<DrawPayload const> const payloadData(
        *static_cast<DrawPayload const *const *>(payload));

    if (draw_ctx->layout.sample_count != 1 || draw_ctx->layout.color_attachment_count != 1) {
        return;
    }

    if (!gLastField) {
        return;
    }

    wgpu::TextureView even;
    wgpu::TextureView odd;

    if (payloadData->halfsies && !payloadData->evenField) {
        even = gLastFieldView;
        odd = gLastFieldView;
    } else {
        even = gIsEvenField ? payloadData->color : gLastFieldView;
        odd = gIsEvenField ? gLastFieldView : payloadData->color;
    }

    wgpu::BindGroupEntry const bindGroupEntries[]{
        {
            .binding = 0,
            .sampler = pipelines::gSampler,
        },
        {
            .binding = 1,
            .textureView = even,
        },
        {
            .binding = 2,
            .textureView = odd,
        },
        {
            .binding = 3,
            .buffer = draw_ctx->uniform_buffer,
            .offset = payloadData->uniformRange.offset,
            .size = payloadData->uniformRange.size,
        },
    };

    wgpu::BindGroupDescriptor const bindGroupDesc{
        .layout = pipelines::gBindGroupLayout,
        .entryCount = std::size(bindGroupEntries),
        .entries = bindGroupEntries,
    };

    auto const bindGroup = gDevice.CreateBindGroup(&bindGroupDesc);

    wgpu::RenderPassEncoder const encoder(draw_ctx->pass);
    encoder.SetPipeline(pipelines::InterlacePipelineFor(
        static_cast<wgpu::TextureFormat>(
            draw_ctx->layout.color_attachments[GFX_SCENE_COLOR_ATTACHMENT_INDEX].format),
        static_cast<wgpu::TextureFormat>(draw_ctx->layout.depth_stencil_format)));

    encoder.SetBindGroup(0, bindGroup);
    encoder.Draw(pipelines::kVertexDrawCount);
}

void CopyHook(ModContext *,
              GfxComputeContext const *compute_ctx,
              const void *payload,
              size_t payload_size,
              void *) {
    assert(payload_size == sizeof(CopyPayload const *));
    std::unique_ptr<CopyPayload const> const payloadData(
        *static_cast<CopyPayload const *const *>(payload));

    if (gLastWidth != payloadData->width || gLastHeight != payloadData->height) {
        wgpu::TextureDescriptor const descriptor{
            .label = "gLastField"sv,
            .usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::RenderAttachment,
            .dimension = wgpu::TextureDimension::e2D,
            .size = {payloadData->width, payloadData->height},
            .format = pipelines::kBlitTargetFormat,
        };

        gLastField = gDevice.CreateTexture(&descriptor);
        gLastFieldView = gLastField.CreateView();
    }

    wgpu::CommandEncoder const encoder(compute_ctx->encoder);

    wgpu::RenderPassColorAttachment const attachment{
        .view = gLastFieldView,
        .loadOp = wgpu::LoadOp::Clear, // TODO: Move to undefined when it's allowed
        .storeOp = wgpu::StoreOp::Store,
    };

    wgpu::RenderPassDescriptor const passDescriptor{
        .label = "Interlace copy pass"sv,
        .colorAttachmentCount = 1,
        .colorAttachments = &attachment,
    };

    auto const passEncoder = encoder.BeginRenderPass(&passDescriptor);
    passEncoder.SetPipeline(pipelines::gBlitPipeline);

    wgpu::BindGroupEntry const entries[]{
        {
            .binding = 0,
            .textureView = payloadData->color,
        },
        {
            .binding = 1,
            .sampler = pipelines::gSampler,
        },
        {
            .binding = 2,
            .buffer = compute_ctx->uniform_buffer,
            .offset = payloadData->uniformRange.offset,
            .size = payloadData->uniformRange.size,
        },
    };

    wgpu::BindGroupDescriptor const bgDesc{
        .layout = pipelines::gBlitBindGroupLayout,
        .entryCount = std::size(entries),
        .entries = entries,
    };

    auto const bg = gDevice.CreateBindGroup(&bgDesc);
    passEncoder.SetBindGroup(0, bg);
    passEncoder.Draw(pipelines::kVertexDrawCount);
    passEncoder.End();
}

constexpr GfxStageHookDesc gHookDesc = {
    .struct_size = sizeof(gHookDesc),
    .callback = StageHook,
};

constexpr GfxDrawTypeDesc gDrawType = {
    .struct_size = sizeof(gDrawType),
    .label = "Interlace deez",
    .draw = DrawHook,
};

constexpr GfxComputeTypeDesc gCopyType = {
    .struct_size = sizeof(gCopyType),
    .label = "Copy deez",
    .callback = CopyHook,
};
} // namespace

extern "C" {
MOD_EXPORT ModResult mod_initialize(ModError *) {
    GfxStageHookHandle handle;
    auto result =
        svc_gfx->register_stage_hook(mod_ctx, GFX_STAGE_FRAME_AFTER_HUD, &gHookDesc, &handle);
    if (result != MOD_OK) {
        mods::log::error("Failed to register stage hook: {}", ResultMessage(result));
        return MOD_ERROR;
    }

    result = svc_gfx->register_draw_type(mod_ctx, &gDrawType, &gDrawTypeHandle);
    if (result != MOD_OK) {
        mods::log::error("Failed to register draw type: {}", ResultMessage(result));
        return MOD_ERROR;
    }

    result = svc_gfx->register_compute_type(mod_ctx, &gCopyType, &gCopyTypeHandle);
    if (result != MOD_OK) {
        mods::log::error("Failed to register compute type: {}", ResultMessage(result));
        return MOD_ERROR;
    }

    GfxDeviceInfo device_info GFX_DEVICE_INFO_INIT;
    result = svc_gfx->get_device_info(mod_ctx, &device_info);
    if (result != MOD_OK) {
        mods::log::error("Failed to get device info: {}", ResultMessage(result));
        return MOD_ERROR;
    }

    gDevice = wgpu::Device(device_info.device);

    pipelines::Init();
    config::Init();

    mods::log::info("May your fram- I MEAN FIELDS be clean");
    return MOD_OK;
}

MOD_EXPORT ModResult mod_update(ModError *) { return MOD_OK; }

MOD_EXPORT ModResult mod_shutdown(ModError *) { return MOD_OK; }
}

} // namespace slugcat::interlace
