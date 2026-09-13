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

std::vector<int> parsePxList(const std::string& v) {
    std::vector<int> out;
    std::istringstream ss(v);
    std::string tok;
    while (ss >> tok) {
        out.push_back(parsePx(tok));
    }
    return out;
}

void applyBox(const std::string& v,
              int& top, int& right, int& bottom, int& left) {
    auto vals = parsePxList(v);
    if (vals.empty()) return;
    if (vals.size() == 1) {
        top = right = bottom = left = vals[0];
    } else if (vals.size() == 2) {
        top = bottom = vals[0];
        right = left = vals[1];
    } else if (vals.size() == 3) {
        top = vals[0];
        right = left = vals[1];
        bottom = vals[2];
    } else {
        top = vals[0]; right = vals[1]; bottom = vals[2]; left = vals[3];
    }
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
            std::string v = d.value;

            // 去掉 !important
            auto imp = v.rfind("!important");
            if (imp != std::string::npos) v = v.substr(0, imp);
            // 去掉尾部空格
            while (!v.empty() && (v.back() == ' ' || v.back() == '\t'))
                v.pop_back();

            if (p == "color") style.color = v;
            else if (p == "background-color") style.backgroundColor = v;
            else if (p == "background") {
                auto hash = v.find('#');
                if (hash != std::string::npos && hash + 7 <= v.size()) {
                    style.backgroundColor = v.substr(hash, 7);
                }
            }
            else if (p == "font-size") style.fontSize = v;
            else if (p == "font-weight") style.fontWeight = v;
            else if (p == "font-family") style.fontFamily = v;
            else if (p == "display") style.display = v;
            else if (p == "position") style.position = v;
            else if (p == "text-align") style.textAlign = v;
            else if (p == "flex-direction") style.flexDirection = v;
            else if (p == "left") { style.left = parsePx(v); style.hasLeft = true; }
            else if (p == "top")  { style.top = parsePx(v);  style.hasTop = true; }
            else if (p == "margin-top") style.marginTop = parsePx(v);
            else if (p == "margin-bottom") style.marginBottom = parsePx(v);
            else if (p == "margin-left") style.marginLeft = parsePx(v);
            else if (p == "margin-right") style.marginRight = parsePx(v);
            else if (p == "margin") applyBox(v, style.marginTop, style.marginRight,
                                             style.marginBottom, style.marginLeft);
            else if (p == "padding-top") style.paddingTop = parsePx(v);
            else if (p == "padding-bottom") style.paddingBottom = parsePx(v);
            else if (p == "padding-left") style.paddingLeft = parsePx(v);
            else if (p == "padding-right") style.paddingRight = parsePx(v);
            else if (p == "padding") applyBox(v, style.paddingTop, style.paddingRight,
                                              style.paddingBottom, style.paddingLeft);
            else if (p == "border-width") style.borderWidth = parsePx(v);
            else if (p == "border-style") style.borderStyle = v;
            else if (p == "border-color") style.borderColor = v;
            else if (p == "border") {
                auto vals = parsePxList(v);
                if (!vals.empty()) style.borderWidth = vals[0];
                if (v.find("solid") != std::string::npos) style.borderStyle = "solid";
                else if (v.find("dashed") != std::string::npos) style.borderStyle = "dashed";
                auto hash = v.find('#');
                if (hash != std::string::npos && hash + 7 <= v.size()) {
                    style.borderColor = v.substr(hash, 7);
                }
            }
        }
    }
}

void resolveRecursive(RenderNode* node,
                      const std::vector<CssRule>& rules,
                      const ComputedStyle* parentStyle) {
    if (node->isText) {
        if (parentStyle) {
            node->style = *parentStyle;
        } else {
            node->style = ComputedStyle{};
            node->style.display = "inline";
            node->style.position = "static";
            node->style.fontWeight = "400";
            node->style.color = "0,0,0";
            node->style.fontSize = "16px";
            node->style.backgroundColor = "0,0,0,0";
        }
        return;
    }

    node->style = ComputedStyle{};
    node->style.display = defaultDisplay(node->tag);

    node->style.position = "static";
    node->style.fontWeight = "400";
    node->style.color = "0,0,0";
    node->style.fontSize = "16px";
    node->style.backgroundColor = "0,0,0,0";
    node->style.flexDirection = "row";

    if (parentStyle) {
        if (!parentStyle->color.empty()) node->style.color = parentStyle->color;
        if (!parentStyle->fontSize.empty()) node->style.fontSize = parentStyle->fontSize;
        if (!parentStyle->fontWeight.empty()) node->style.fontWeight = parentStyle->fontWeight;
        if (!parentStyle->textAlign.empty()) node->style.textAlign = parentStyle->textAlign;
        if (!parentStyle->fontFamily.empty()) node->style.fontFamily = parentStyle->fontFamily;
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
    root->style.flexDirection = "row";
    for (auto& c : root->children) {
        resolveRecursive(c.get(), rules, nullptr);
    }
}

} // namespace dm::render