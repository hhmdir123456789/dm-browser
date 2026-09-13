#pragma once
#include "render/render_node.h"
#include "learn/snapshot.h"
#include <string>

namespace dm::render {

dm::learn::PageSnapshot toPageSnapshot(const RenderNode* root,
                                        const std::string& url,
                                        const std::string& title,
                                        int viewportW, int viewportH);

std::string dumpSnapshotJson(const RenderNode* root,
                              const std::string& url,
                              const std::string& title,
                              int viewportW, int viewportH);

} // namespace dm::render