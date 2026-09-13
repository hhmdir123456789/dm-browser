#include "render/snapshot_dumper.h"
#include "learn/snapshot_parser.h"
#include <sstream>
#include <cctype>

namespace dm::render {

namespace {

std::string lower(const std::string& s) {
    std::string out = s;
    for (char& c : out) c = (char)std::tolower((unsigned char)c);
    return out;
}

// 构造稳定路径，与 WebView2 采集格式一致：html>body.0>div.0>p.0
// 规则：html 不带序号，其他所有节点带 ".N"（同 tag 兄弟中的序号）
void buildPath(const RenderNode* node, std::string& out) {
    if (!node) return;
    if (node->tag == "#document") return;

    if (node->parent && node->parent->tag != "#document") {
        buildPath(node->parent, out);
        if (!out.empty()) out += ">";
    }

    std::string tagLower = lower(node->tag);
    out += tagLower;

    if (tagLower != "html") {
        int sameTagIdx = 0;
        if (node->parent) {
            for (const auto& sib : node->parent->children) {
                if (sib.get() == node) break;
                if (lower(sib->tag) == tagLower) sameTagIdx++;
            }
        }
        out += ".";
        out += std::to_string(sameTagIdx);
    }
}

// 非可视节点：不进快照，但递归子节点
bool isNonVisual(const std::string& tag) {
    return tag == "html" || tag == "head" ||
           tag == "title" || tag == "meta" ||
           tag == "link" || tag == "base" ||
           tag == "script" || tag == "style" ||
           tag == "#document";
}

void collectNodes(const RenderNode* node,
                  std::vector<dm::learn::NodeSnapshot>& out,
                  int& maxDepth,
                  int& totalCount) {
    if (!node) return;

    // 文本节点 / 非可视节点：不进快照，只递归子节点
    if (node->isText || isNonVisual(lower(node->tag))) {
        for (const auto& c : node->children) {
            collectNodes(c.get(), out, maxDepth, totalCount);
        }
        return;
    }

    dm::learn::NodeSnapshot s;
    std::string path;
    buildPath(node, path);
    s.path = path;
    s.tag = lower(node->tag);
    s.id = node->id;
    s.className = node->className;
    s.depth = node->depth;
    s.childCount = (int)node->children.size();
    s.textPreview = node->text.substr(0, 80);

    s.style.color = node->style.color;
    s.style.backgroundColor = node->style.backgroundColor;
    s.style.fontSize = node->style.fontSize;
    s.style.fontWeight = node->style.fontWeight;
    s.style.display = node->style.display;
    s.style.position = node->style.position;
    s.style.textAlign = node->style.textAlign;
    s.style.marginTop = node->style.marginTop;
    s.style.marginBottom = node->style.marginBottom;
    s.style.marginLeft = node->style.marginLeft;
    s.style.marginRight = node->style.marginRight;
    s.style.paddingTop = node->style.paddingTop;
    s.style.paddingBottom = node->style.paddingBottom;
    s.style.paddingLeft = node->style.paddingLeft;
    s.style.paddingRight = node->style.paddingRight;
    s.style.borderWidth = node->style.borderWidth;
    s.style.borderStyle = node->style.borderStyle;

    s.layout.x = node->layout.x;
    s.layout.y = node->layout.y;
    s.layout.w = node->layout.w;
    s.layout.h = node->layout.h;

    out.push_back(std::move(s));
    totalCount++;
    if (node->depth > maxDepth) maxDepth = node->depth;

    for (const auto& c : node->children) {
        collectNodes(c.get(), out, maxDepth, totalCount);
    }
}

} // namespace

dm::learn::PageSnapshot toPageSnapshot(const RenderNode* root,
                                        const std::string& url,
                                        const std::string& title,
                                        int viewportW, int viewportH) {
    dm::learn::PageSnapshot snap;
    snap.url = url;
    snap.title = title;
    snap.viewportWidth = viewportW;
    snap.viewportHeight = viewportH;

    int maxDepth = 0;
    int totalCount = 0;
    collectNodes(root, snap.nodes, maxDepth, totalCount);
    snap.totalNodes = totalCount;
    snap.maxDepth = maxDepth;

    return snap;
}

std::string dumpSnapshotJson(const RenderNode* root,
                              const std::string& url,
                              const std::string& title,
                              int viewportW, int viewportH) {
    auto snap = toPageSnapshot(root, url, title, viewportW, viewportH);
    return dm::learn::serializeSnapshot(snap);
}

} // namespace dm::render