#pragma once
#include "render/render_node.h"
#include <string>

namespace dm::render {

// iter3: WIC 加载图片
bool loadImageWic(const std::string& path, ImageData& out);

} // namespace dm::render