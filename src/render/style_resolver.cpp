#include "render/style_resolver.h"
#include <cctype>
#include <sstream>
#include <algorithm>

namespace dm::render {

namespace {

int parsePx(const std::string& v) {
    std::string s;
    for (char c : v) {
        if (std::isdigit((unsigned char)c) || c == '-' || c == '+') s += c;
        else break;
    }
    if (s.empty()) return 0;
    try { return std::stoi(s); } catch (...) { return 0; }
}

std::string defaultDisplay(const std::string& tag) {
    static const char* kInline[] = {
        "a","span","b","i","em","strong","small","sub","sup",
        "code","kbd","label","br","img","input", nullptr
    };
    for (int i = 0; kInline[i]; ++i) {
        if (tag == kInline[i]) return "inline";
    }
    static const char* kNone[] = { "script", "style", "head", "meta", "link", nullptr };
    for (int i = 0; kNone[i]; ++i) {
        if (tag == kNone[i]) return "none";
    }
    return "block";
}

void applyRules(const RenderNode* node,
                const std::vector<CssRule>& rules,
                ComputedStyle& style) {
    std::vector<const CssRule*> matched;
    for (const auto& r : rules) {
        if (matchesSelector(node, r)) {
            matched.push_back(&r);
        }
    }
    std::sort(matched.begin(), matched.end(),
        [](const CssRule* a, const CssRule* b) {
            return a->specificity() < b->specificity();
        });

    for (const auto* r : matched) {
        for (const auto& d : r->decls) {
            const std::string& p = d.property;
            const std::string& v = d.value;
            if (p == "color") style.color = v;
            else if (p == "background-color") style.backgroundColor = v;
            else if (p == "font-size") style.fontSize = v;
            else if (p == "font-weight") style.fontWeight = v;
            else if (p == "display") style.display = v;
            else if (p == "position") style.position = v;
            else if (p == "text-align") style.textAlign = v;
            else if (p == "margin-top") style.marginTop = parsePx(v);
            else if (p == "margin-bottom") style.marginBottom = parsePx(v);
            else if (p == "margin-left") style.marginLeft = parsePx(v);
            else if (p == "margin-right") style.marginRight = parsePx(v);
            else if (p == "margin") {
                int n = parsePx(v);
                style.marginTop = style.marginBottom = n;
                style.marginLeft = style.marginRight = n;
            }
            else if (p == "padding-top") style.paddingTop = parsePx(v);
            else if (p == "padding-bottom") style.paddingBottom = parsePx(v);
            else if (p == "padding-left") style.paddingLeft = parsePx(v);
            else if (p == "padding-right") style.paddingRight = parsePx(v);
            else if (p == "padding") {
                int n = parsePx(v);
                style.paddingTop = style.paddingBottom = n;
                style.paddingLeft = style.paddingRight = n;
            }
            else if (p == "border-width") style.borderWidth = parsePx(v);
            else if (p == "border-style") style.borderStyle = v;
            else if (p == "border-color") style.borderColor = v;
        }
    }
}

void resolveRecursive(RenderNode* node,
                      const std::vector<CssRule>& rules,
                      const ComputedStyle* parentStyle) {
    node->style = ComputedStyle{};
    node->style.display = defaultDisplay(node->tag);

    // 默认值（跟 Chromium 初始值对齐）
    node->style.position = "static";
    node->style.fontWeight = "400";
    node->style.color = "0,0,0";
    node->style.fontSize = "16px";
    node->style.backgroundColor = "0,0,0,0";

    // 从父继承（覆盖默认值）
    if (parentStyle) {
        if (!parentStyle->color.empty())
            node->style.color = parentStyle->color;
        if (!parentStyle->fontSize.empty())
            node->style.fontSize = parentStyle->fontSize;
        if (!parentStyle->fontWeight.empty())
            node->style.fontWeight = parentStyle->fontWeight;
        if (!parentStyle->textAlign.empty())
            node->style.textAlign = parentStyle->textAlign;
    }

    applyRules(node, rules, node->style);

    for (auto& c : node->children) {
        resolveRecursive(c.get(), rules, &node->style);
    }
}

} // namespace

std::vector<std::string> extractStyleBlocks(const std::string& html) {
    std::vector<std::string> out;
    size_t i = 0;
    while (i < html.size()) {
        size_t start = html.find("<style", i);
        if (start == std::string::npos) break;
        size_t gt = html.find('>', start);
        if (gt == std::string::npos) break;
        size_t end = html.find("</style>", gt);
        if (end == std::string::npos) break;
        out.push_back(html.substr(gt + 1, end - gt - 1));
        i = end + 8;
    }
    return out;
}

void resolveStyles(RenderNode* root, const std::vector<CssRule>& rules) {
    if (!root) return;
    root->style = ComputedStyle{};
    root->style.display = "block";
    root->style.position = "static";
    root->style.fontWeight = "400";
    root->style.color = "0,0,0";
    root->style.fontSize = "16px";
    root->style.backgroundColor = "0,0,0,0";
    for (auto& c : root->children) {
        resolveRecursive(c.get(), rules, nullptr);
    }
}

} // namespace dm::render