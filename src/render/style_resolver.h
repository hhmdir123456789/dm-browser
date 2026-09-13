#pragma once
#include "render/render_node.h"
#include "render/css_parser.h"
#include <vector>

namespace dm::render {

void resolveStyles(RenderNode* root, const std::vector<CssRule>& rules);

std::vector<std::string> extractStyleBlocks(const std::string& html);

} // namespace dm::render