#pragma once
#include "render/render_node.h"

namespace dm::render {

void layoutTree(RenderNode* root, int viewportWidth, int viewportHeight);

} // namespace dm::render