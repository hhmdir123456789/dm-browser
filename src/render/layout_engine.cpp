#include "render/layout_engine.h"
#include <algorithm>
#include <cctype>

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

bool isInline(const RenderNode* n) {
    return n->style.display == "inline";
}

float lineHeight(const RenderNode* n) {
    int fs = fontSizePx(n->style.fontSize);
    if (fs <= 0) fs = 16;
    return fs * 1.4f;
}

float layoutBlockChildren(RenderNode* parent, float x, float y, float width) {
    float curY = y;
    float curX = x;
    float lineMaxH = 0;

    for (auto& childPtr : parent->children) {
        RenderNode* c = childPtr.get();
        if (c->style.display == "none") {
            c->layout = {0, 0, 0, 0};
            continue;
        }

        if (isInline(c)) {
            float w = 0;
            float h = lineHeight(c);
            if (!c->text.empty()) {
                int fs = fontSizePx(c->style.fontSize);
                if (fs <= 0) fs = 16;
                w = (float)c->text.size() * fs * 0.55f;
            } else if (c->tag == "img") {
                w = 50; h = 50;
            }
            if (curX + w > x + width && curX > x) {
                curY += lineMaxH;
                curX = x;
                lineMaxH = 0;
            }
            c->layout = {curX, curY, w, h};
            curX += w;
            if (h > lineMaxH) lineMaxH = h;
        } else {
            if (lineMaxH > 0) {
                curY += lineMaxH;
                curX = x;
                lineMaxH = 0;
            }

            curY += c->style.marginTop;
            float childX = x + c->style.marginLeft;
            float childW = width - c->style.marginLeft - c->style.marginRight;

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

            if (c->children.empty() && !c->text.empty()) {
                totalH = std::max(totalH, lineHeight(c)
                                  + c->style.paddingTop + c->style.paddingBottom);
            }

            c->layout.h = totalH;

            curY += totalH + c->style.marginBottom;
        }
    }

    if (lineMaxH > 0) curY += lineMaxH;

    return curY - y;
}

} // namespace

void layoutTree(RenderNode* root, int viewportWidth, int viewportHeight) {
    if (!root) return;

    root->layout = {0, 0, (float)viewportWidth, (float)viewportHeight};

    float y = 0;
    for (auto& c : root->children) {
        if (c->style.display == "none") continue;
        float h = layoutBlockChildren(c.get(), 0, y, (float)viewportWidth);
        c->layout.x = 0;
        c->layout.y = y;
        c->layout.w = (float)viewportWidth;
        c->layout.h = h;
        y += h;
    }
}

} // namespace dm::render