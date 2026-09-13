#pragma once
#include "render/render_node.h"
#include <string>
#include <memory>

namespace dm::render {

// 解析 HTML 字符串，返回根节点（tag = "#document"）
// 极简实现：标签、id/class、文本；跳过 <!DOCTYPE>、注释、script/style 内容
std::unique_ptr<RenderNode> parseHtml(const std::string& html);

} // namespace dm::render