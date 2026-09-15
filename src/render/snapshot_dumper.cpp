#include "render/snapshot_dumper.h"
#include "learn/snapshot_parser.h"
#include <sstream>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace dm::render {

namespace {

std::string lower(const std::string& s) {
    std::string out = s;
    for (char& c : out) c = (char)std::tolower((unsigned char)c);
    return out;
}

// hex → "r,g,b"
std::string hexToRgb(const std::string& hex) {
    if (hex.empty() || hex[0] != '#') return hex;
    unsigned int r = 0, g = 0, b = 0;
    if (hex.size() >= 7) {
        std::sscanf(hex.c_str() + 1, "%02x%02x%02x", &r, &g, &b);
    } else if (hex.size() >= 4) {
        char rs[3] = {hex[1], hex[1], 0};
        char gs[3] = {hex[2], hex[2], 0};
        char bs[3] = {hex[3], hex[3], 0};
        std::sscanf(rs, "%x", &r);
        std::sscanf(gs, "%x", &g);
        std::sscanf(bs, "%x", &b);
    } else {
        return hex;
    }
    return std::to_string(r) + "," + std::to_string(g) + "," + std::to_string(b);
}

// fontSize → px（继承父级）
int parseFontSizePx(const std::string& fs, int parentPx) {
    if (fs.empty()) return parentPx;
    try {
        if (fs.size() >= 2 && fs.substr(fs.size() - 2) == "px") {
            return (int)std::stod(fs.substr(0, fs.size() - 2));
        }
        if (!fs.empty() && fs.back() == '%') {
            return parentPx * std::stoi(fs.substr(0, fs.size() - 1)) / 100;
        }
        if (fs.size() >= 2 && fs.substr(fs.size() - 2) == "em") {
            return (int)(parentPx * std::stod(fs.substr(0, fs.size() - 2)));
        }
        if (fs.size() >= 3 && fs.substr(fs.size() - 3) == "rem") {
            return (int)(16 * std::stod(fs.substr(0, fs.size() - 3)));
        }
        return (int)(parentPx * std::stod(fs));
    } catch (...) {}
    return parentPx;
}

// 构造稳定路径
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
                  int& totalCount,
                  int parentFontSize = 16) {
    if (!node) return;

    if (node->isText || isNonVisual(lower(node->tag))) {
        int passFont = parentFontSize;
        if (!node->isText && !node->style.fontSize.empty()) {
            passFont = parseFontSizePx(node->style.fontSize, parentFontSize);
        }
        for (const auto& c : node->children) {
            collectNodes(c.get(), out, maxDepth, totalCount, passFont);
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

    int myFontSize = parseFontSizePx(node->style.fontSize, parentFontSize);

    s.style.color = hexToRgb(node->style.color);
    s.style.backgroundColor = hexToRgb(node->style.backgroundColor);
    s.style.fontSize = std::to_string(myFontSize) + "px";
    {
        std::string fw = node->style.fontWeight;
        if (fw == "bold") fw = "700";
        else if (fw == "normal" || fw.empty()) fw = "400";
        s.style.fontWeight = fw;
    }
    s.style.display = node->style.display;
    s.style.position = node->style.position;
    s.style.textAlign = node->style.textAlign;
    s.style.marginTop = (int)node->style.marginTop.value;
    s.style.marginBottom = (int)node->style.marginBottom.value;
    s.style.marginLeft = (int)node->style.marginLeft.value;
    s.style.marginRight = (int)node->style.marginRight.value;
    s.style.paddingTop = (int)node->style.paddingTop.value;
    s.style.paddingBottom = (int)node->style.paddingBottom.value;
    s.style.paddingLeft = (int)node->style.paddingLeft.value;
    s.style.paddingRight = (int)node->style.paddingRight.value;
    s.style.borderWidth = (int)node->style.borderWidth.value;
    s.style.borderStyle = node->style.borderStyle;

    // iter1: opacity / fontStyle
    {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%.2f", node->style.opacity);
        s.style.opacity = buf;
    }
    s.style.fontStyle = node->style.fontStyle;

    // iter2: listStyleType / backgroundImage
    s.style.listStyleType = node->style.listStyleType;
    if (!node->style.backgroundImageRaw.empty()) {
        s.style.backgroundImage = node->style.backgroundImageRaw;
    } else {
        s.style.backgroundImage = node->style.backgroundImage;
    }


    // iter3
    s.style.intrinsicW = (int)node->intrinsicW;
    s.style.intrinsicH = (int)node->intrinsicH;
        // iter4
    s.style.filter = node->style.filterRaw;
    s.layout.x = node->layout.x;
    s.layout.y = node->layout.y;
    s.layout.w = node->layout.w;
    s.layout.h = node->layout.h;

    out.push_back(std::move(s));
    totalCount++;
    if (node->depth > maxDepth) maxDepth = node->depth;

    for (const auto& c : node->children) {
        collectNodes(c.get(), out, maxDepth, totalCount, myFontSize);
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