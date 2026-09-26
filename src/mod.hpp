#pragma once

#include "webgpu/webgpu_cpp.h"

namespace slugcat::interlace {

struct UniformsInterlace {
    float sizeX;
    float sizeY;
};

extern wgpu::Device gDevice;

}
