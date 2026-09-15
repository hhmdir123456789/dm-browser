#pragma once
#include <string>
#include <vector>
#include <memory>
#include <map>
#include "render/length.h"

namespace dm::render {

struct ImageData {
    int width = 0;
    int height = 0;
    std::vector<unsigned char> bgra;   // 4 bytes/pixel, 预乘
    bool valid() const { return width > 0 && height > 0 && !bgra.empty(); }
};

struct ComputedStyle {
    std::string color;
    std::string backgroundColor;
    std::string fontSize;
    std::string fontWeight;
    std::string fontStyle;
    std::string fontFamily;
    std::string display;
    std::string position;
    std::string textAlign;
    std::string flexDirection;
    std::string justifyContent;
    std::string alignItems;
    std::string flexWrap;
    std::string lineHeight;
    std::string verticalAlign;
    std::string boxSizing;
    std::string overflow;
    std::string backgroundImage;
    std::string zIndex;
    std::string floatDir;
    std::string clear;
    std::string transform;

    float opacity = 1.0f;
    Length right, bottom;
    bool hasRight = false, hasBottom = false;

    std::string listStyleType;
    std::string listStylePosition;
    std::vector<std::string> fontFamilyList;
    std::string backgroundImageRaw;

    bool lineHeightIsNumber = false;
    bool lineHeightIsNormal = false;

    // iter3: @font-face 用到的私有字体
    std::string resolvedFontFamily;

    // grid
    std::string gridTemplateColumns;
    Length gridGap;
    Length gridColumnGap;
    Length gridRowGap;
    bool hasGridGap = false;
    bool hasGridColumnGap = false;
    bool hasGridRowGap = false;

    Length marginTop, marginBottom, marginLeft, marginRight;
    Length paddingTop, paddingBottom, paddingLeft, paddingRight;
    Length borderWidth;
    Length width, height;
    Length minWidth, maxWidth;
    Length minHeight, maxHeight;
    Length lineHeightPx;
    Length left, top;
    Length borderRadius;

    bool hasLeft = false, hasTop = false;
    bool hasWidth = false, hasHeight = false;
    bool hasMinWidth = false, hasMaxWidth = false;
    bool hasMinHeight = false, hasMaxHeight = false;
    bool hasZIndex = false;
    bool hasBorderRadius = false;

    int zIndexValue = 0;

    std::string borderStyle;
    std::string borderColor;

    std::map<std::string, std::string> cssVars;

    bool isBorderBox() const { return boxSizing == "border-box"; }
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

    // iter3: 通用属性
    std::map<std::string, std::string> attrs;

    // iter3: 图片数据
    ImageData image;
    float intrinsicW = 0;
    float intrinsicH = 0;

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