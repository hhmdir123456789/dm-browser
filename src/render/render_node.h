#pragma once
#include <string>
#include <vector>
#include <memory>

namespace dm::render {

struct ComputedStyle {
    std::string color;
    std::string backgroundColor;
    std::string fontSize;
    std::string fontWeight;
    std::string fontFamily;
    std::string display;
    std::string position;
    std::string textAlign;
    std::string flexDirection;

    int marginTop = 0, marginBottom = 0, marginLeft = 0, marginRight = 0;
    int paddingTop = 0, paddingBottom = 0, paddingLeft = 0, paddingRight = 0;

    int borderWidth = 0;
    std::string borderStyle;
    std::string borderColor;

    int left = 0, top = 0;
    bool hasLeft = false, hasTop = false;
};

struct Layout {
    float x = 0, y = 0, w = 0, h = 0;
};

struct RenderNode {
    std::string tag;
    std::string id;
    std::string className;
    std::string text;
    bool isText = false;

    RenderNode* parent = nullptr;
    std::vector<std::unique_ptr<RenderNode>> children;
    int depth = 0;

    ComputedStyle style;
    Layout layout;

    RenderNode* appendChild(std::unique_ptr<RenderNode> child) {
        child->parent = this;
        child->depth = depth + 1;
        RenderNode* raw = child.get();
        children.push_back(std::move(child));
        return raw;
    }
};

} // namespace dm::render