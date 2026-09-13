#pragma once
#include "render/render_node.h"
#include <string>
#include <vector>

namespace dm::render {

struct CssDecl {
    std::string property;
    std::string value;
};

// 单个选择器段（不含空格）
// "div.foo#bar" → tag="div", id="bar", classes=["foo"]
struct CssSelector {
    std::string tag;
    std::string id;
    std::vector<std::string> classes;
};

// 一条 CSS 规则
// ".card p" → parts = [{classes:["card"]}, {tag:"p"}]
// "div#main" → parts = [{tag:"div", id:"main"}]
struct CssRule {
    std::vector<CssSelector> parts;   // 从左（祖先）到右（后代）
    std::vector<CssDecl> decls;

    int specificity() const {
        int s = 0;
        for (const auto& p : parts) {
            if (!p.id.empty()) s += 100;
            s += (int)p.classes.size() * 10;
            if (!p.tag.empty()) s += 1;
        }
        return s;
    }
};

std::vector<CssRule> parseCss(const std::string& css);

// 匹配整个后代选择器链：最右 part 匹配 node，左侧 parts 沿 parent 链匹配
bool matchesSelector(const RenderNode* node, const CssRule& rule);

} // namespace dm::render