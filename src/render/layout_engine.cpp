#include "render/layout_engine.h"
#include "render/text_measure.h"
#include <algorithm>
#include <cctype>
#include <vector>
#include <string>

namespace dm::render {

namespace {

int fontSizePx(const std::string& fs) {
    std::string s;
    for (char c : fs) {
        if (std::isdigit((unsigned char)c) || c == '.') s += c;
        else break;
    }
    if (s.empty()) return 16;
    try { return (int)std::stod(s); } catch (...) { return 16; }
}

// 把文本切成可断行的最小单元
std::vector<std::string> tokenize(const std::string& text) {
    std::vector<std::string> out;
    size_t i = 0;
    while (i < text.size()) {
        unsigned char c = (unsigned char)text[i];
        if (c < 0x80) {
            if (c == ' ') { out.push_back(" "); i++; }
            else {
                size_t start = i;
                while (i < text.size()) {
                    unsigned char cc = (unsigned char)text[i];
                    if (cc < 0x80 && cc != ' ') i++;
                    else break;
                }
                out.push_back(text.substr(start, i - start));
            }
        } else if ((c & 0xF0) == 0xE0) { out.push_back(text.substr(i, 3)); i += 3; }
        else if ((c & 0xE0) == 0xC0) { out.push_back(text.substr(i, 2)); i += 2; }
        else if ((c & 0xF8) == 0xF0) { out.push_back(text.substr(i, 4)); i += 4; }
        else { out.push_back(text.substr(i, 1)); i++; }
    }
    return out;
}

bool isInline(const RenderNode* n) {
    return n->style.display == "inline" ||
           n->style.display == "inline-block";
}

bool isFlex(const RenderNode* n) {
    return n->style.display == "flex" ||
           n->style.display == "inline-flex";
}

// 内在宽度：内容最小宽度（flex 布局用）
float intrinsicWidth(const RenderNode* node) {
    if (!node) return 0;
    if (node->isText) {
        int fs = fontSizePx(node->style.fontSize);
        return measureText(node->text, fs, node->style.fontFamily);
    }
    if (node->tag == "img") return 50.0f;

    float w = 0;
    if (isInline(node) || isFlex(node)) {
        for (auto& c : node->children) w += intrinsicWidth(c.get());
    } else {
        for (auto& c : node->children) {
            float cw = intrinsicWidth(c.get());
            if (cw > w) w = cw;
        }
    }
    w += node->style.paddingLeft + node->style.paddingRight
       + node->style.borderWidth * 2;
    return w;
}

float layoutBlockChildren(RenderNode* parent, float x, float y, float width);

// 布局文本节点：支持自动换行
// 关键：单行时也要把行高纳入 lineMaxH，否则父级末尾 curY += lineMaxH 加 0
float layoutText(RenderNode* textNode, float& curX, float& curY,
                 float x, float width, float& lineMaxH) {
    int fs = fontSizePx(textNode->style.fontSize);
    if (fs <= 0) fs = 16;
    float lh = fs * 1.5f;

    // ← 关键修复：无论单行还是多行，行高都先纳入 lineMaxH
    if (lh > lineMaxH) lineMaxH = lh;

    auto tokens = tokenize(textNode->text);
    float startX = curX;
    float maxX = curX;
    float startY = curY;

    for (const auto& tok : tokens) {
        float tw = measureText(tok, fs, textNode->style.fontFamily);
        if (curX + tw > x + width && curX > x) {
            curY += lh;
            curX = x;
        }
        curX += tw;
        if (curX > maxX) maxX = curX;
    }

    textNode->layout.x = startX;
    textNode->layout.y = startY;
    textNode->layout.w = maxX - startX;
    textNode->layout.h = (curY - startY) + lh;

    return textNode->layout.h;
}

// inline 容器内容（span 等）
float layoutInlineContent(RenderNode* parent, float& curX, float baseY, float maxW) {
    float maxH = 0;
    for (auto& cp : parent->children) {
        RenderNode* c = cp.get();
        if (!c->isText && c->style.display == "none") continue;

        if (c->isText) {
            int fs = fontSizePx(c->style.fontSize); if (fs <= 0) fs = 16;
            float w = measureText(c->text, fs, c->style.fontFamily);
            float h = fs * 1.5f;
            c->layout = {curX, baseY, w, h};
            curX += w;
            if (h > maxH) maxH = h;
        } else if (c->tag == "img") {
            c->layout = {curX, baseY, 50.0f, 50.0f};
            curX += 50.0f;
            if (50.0f > maxH) maxH = 50.0f;
        } else if (isInline(c)) {
            float padL = (float)c->style.paddingLeft;
            float padR = (float)c->style.paddingRight;
            float subStart = curX;
            curX += padL;
            float innerH = layoutInlineContent(c, curX, baseY, maxW - (curX - subStart));
            curX += padR;
            c->layout = {subStart, baseY, curX - subStart, innerH};
            if (innerH > maxH) maxH = innerH;
        }
    }
    return maxH;
}

// 找 positioned ancestor
RenderNode* findPositionedAncestor(RenderNode* node) {
    RenderNode* p = node->parent;
    while (p) {
        if (p->style.position == "relative" ||
            p->style.position == "absolute" ||
            p->style.position == "fixed") {
            return p;
        }
        p = p->parent;
    }
    return nullptr;
}

// flex 容器：子元素横排
float layoutFlexChildren(RenderNode* parent, float x, float y, float width) {
    float padL = (float)parent->style.paddingLeft;
    float padR = (float)parent->style.paddingRight;
    float contentW = width - padL - padR;
    if (contentW < 0) contentW = 0;

    float contentX = x + padL;
    float contentY = y + parent->style.paddingTop;
    float maxH = 0;

    for (auto& cp : parent->children) {
        RenderNode* c = cp.get();
        if (!c->isText && c->style.display == "none") continue;

        float iw = intrinsicWidth(c);
        if (iw < 1) iw = 1;
        if (iw > contentW) iw = contentW;

        if (c->isText) {
            int fs = fontSizePx(c->style.fontSize); if (fs <= 0) fs = 16;
            float w = measureText(c->text, fs, c->style.fontFamily);
            float h = fs * 1.5f;
            c->layout = {contentX, contentY, w, h};
            contentX += w;
            if (h > maxH) maxH = h;
        } else if (c->tag == "img") {
            c->layout = {contentX, contentY, 50.0f, 50.0f};
            contentX += 50.0f;
            if (50.0f > maxH) maxH = 50.0f;
        } else {
            float childW = iw;
            float innerH = layoutBlockChildren(c, contentX, contentY, childW);
            c->layout.x = contentX;
            c->layout.y = contentY;
            c->layout.w = childW;
            c->layout.h = innerH + c->style.paddingTop + c->style.paddingBottom
                        + c->style.borderWidth * 2;
            contentX += childW;
            if (c->layout.h > maxH) maxH = c->layout.h;
        }
    }

    maxH += parent->style.paddingTop + parent->style.paddingBottom;
    return maxH;
}

// 块级布局主函数
float layoutBlockChildren(RenderNode* parent, float x, float y, float width) {
    if (isFlex(parent)) {
        return layoutFlexChildren(parent, x, y, width);
    }

    float curY = y;
    float curX = x;
    float lineMaxH = 0;
    float pendingMarginBottom = 0;

    for (auto& childPtr : parent->children) {
        RenderNode* c = childPtr.get();
        if (!c->isText && c->style.display == "none") {
            c->layout = {0, 0, 0, 0};
            continue;
        }

        // position: absolute / fixed
        if (c->style.position == "absolute" || c->style.position == "fixed") {
            RenderNode* anc = findPositionedAncestor(c);
            float baseX = anc ? anc->layout.x + anc->style.borderWidth : 0;
            float baseY = anc ? anc->layout.y + anc->style.paddingTop : 0;
            float absX = baseX + (c->style.hasLeft ? (float)c->style.left : 0);
            float absY = baseY + (c->style.hasTop ? (float)c->style.top : 0);
            float iw = intrinsicWidth(c);
            if (iw < 1) iw = 100;
            float innerH = layoutBlockChildren(c, absX, absY, iw);
            c->layout.x = absX;
            c->layout.y = absY;
            c->layout.w = iw;
            c->layout.h = innerH + c->style.paddingTop + c->style.paddingBottom;
            continue;
        }

        if (c->isText) {
            layoutText(c, curX, curY, x, width, lineMaxH);
            continue;
        }

        if (isInline(c)) {
            float fs = fontSizePx(c->style.fontSize); if (fs <= 0) fs = 16;
            float h = fs * 1.5f;

            if (c->tag == "img") {
                float w = 50.0f; h = 50.0f;
                if (curX + w > x + width && curX > x) {
                    curY += lineMaxH; curX = x; lineMaxH = 0;
                }
                c->layout = {curX, curY, w, h};
                curX += w;
                if (h > lineMaxH) lineMaxH = h;
            } else if (!c->children.empty()) {
                float padL = (float)c->style.paddingLeft;
                float padR = (float)c->style.paddingRight;
                float subStart = curX;
                curX += padL;
                float maxW = x + width - curX;
                float innerH = layoutInlineContent(c, curX, curY, maxW);
                curX += padR;
                c->layout = {subStart, curY, curX - subStart, innerH};
                if (innerH > lineMaxH) lineMaxH = innerH;
            } else {
                c->layout = {curX, curY, 0, h};
                if (h > lineMaxH) lineMaxH = h;
            }
        } else {
            // 块级
            if (lineMaxH > 0) {
                curY += lineMaxH;
                curX = x;
                lineMaxH = 0;
            }

            float mt = (float)c->style.marginTop;
            curY += std::max(mt, pendingMarginBottom);
            pendingMarginBottom = 0;

            float childX = x + c->style.marginLeft;
            float childW = width - c->style.marginLeft - c->style.marginRight;
            if (childW < 0) childW = 0;

            float contentW = childW - c->style.paddingLeft - c->style.paddingRight
                            - c->style.borderWidth * 2;
            if (contentW < 0) contentW = 0;

            c->layout.x = childX;
            c->layout.y = curY;
            c->layout.w = childW;

            float innerX = childX + c->style.paddingLeft + c->style.borderWidth;
            float innerY = curY + c->style.paddingTop + c->style.borderWidth;
            float innerH = layoutBlockChildren(c, innerX, innerY, contentW);

            float totalH = innerH
                          + c->style.paddingTop + c->style.paddingBottom
                          + c->style.borderWidth * 2;

            c->layout.h = totalH;

            curY += totalH;
            pendingMarginBottom = (float)c->style.marginBottom;
        }
    }

    if (lineMaxH > 0) curY += lineMaxH;
    if (pendingMarginBottom > 0) curY += pendingMarginBottom;

    return curY - y;
}

} // namespace

void layoutTree(RenderNode* root, int viewportWidth, int viewportHeight) {
    if (!root) return;

    root->layout = {0, 0, (float)viewportWidth, (float)viewportHeight};

    float y = 0;
    for (auto& c : root->children) {
        if (!c->isText && c->style.display == "none") continue;
        float h = layoutBlockChildren(c.get(), 0, y, (float)viewportWidth);
        c->layout.x = 0;
        c->layout.y = y;
        c->layout.w = (float)viewportWidth;
        c->layout.h = h;
        y += h;
    }
}

} // namespace dm::render