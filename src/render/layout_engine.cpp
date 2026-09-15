#include "render/layout_engine.h"
#include "render/text_measure.h"
#include "render/length.h"
#include <algorithm>
#include <cctype>
#include <vector>
#include <string>
#include <cmath>
#include <sstream>

namespace dm::render {

namespace {

int g_viewportW = 1024;
int g_viewportH = 768;
const int kRootFontSize = 16;

int fontSizePx(const std::string& fs) {
    std::string s;
    for (char c : fs) {
        if (std::isdigit((unsigned char)c) || c == '.') s += c;
        else break;
    }
    if (s.empty()) return 16;
    try { return (int)std::stod(s); } catch (...) { return 16; }
}

int fontWeightNum(const std::string& fw) {
    if (fw == "bold") return 700;
    if (fw == "normal" || fw.empty()) return 400;
    try { return std::stoi(fw); } catch (...) { return 400; }
}

float lenToPx(const Length& l, int parentFontSize, float percentBase) {
    return resolveLength(l, parentFontSize, kRootFontSize, g_viewportW, g_viewportH, percentBase);
}

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
    return n->style.display == "inline" || n->style.display == "inline-block";
}
bool isFlex(const RenderNode* n) {
    return n->style.display == "flex" || n->style.display == "inline-flex";
}
bool isGrid(const RenderNode* n) {
    return n->style.display == "grid" || n->style.display == "inline-grid";
}
bool isHidden(const RenderNode* n) {
    if (n->isText) return false;
    return n->style.display == "none";
}
bool isListItem(const RenderNode* n) {
    return n->style.display == "list-item";
}

float computeLineHeight(const RenderNode* node, int fs) {
    if (node->style.lineHeightIsNormal) return fs * 1.5f;
    if (node->style.lineHeightIsNumber) return fs * node->style.lineHeightPx.value;
    if (!node->style.lineHeightPx.isZero()) {
        return node->style.lineHeightPx.value *
               (node->style.lineHeightPx.unit == Length::Em ? fs : 1);
    }
    return fs * 1.5f;
}

float measureTextForNode(const RenderNode* node, const std::string& text,
                         int fs, int fw) {
    if (!node->style.fontFamilyList.empty()) {
        return measureTextWithList(text, fs, node->style.fontFamilyList, fw);
    }
    return measureText(text, fs, node->style.fontFamily, fw);
}

float intrinsicWidth(const RenderNode* node) {
    if (!node) return 0;
    if (node->isText) {
        int fs = fontSizePx(node->style.fontSize);
        return measureTextForNode(node, node->text, fs,
                                  fontWeightNum(node->style.fontWeight));
    }

    // iter3: img 用真实尺寸
    if (node->tag == "img") {
        if (node->intrinsicW > 0) return node->intrinsicW;
        return 50.0f;
    }

    float w = 0;
    if (isInline(node) || isFlex(node)) {
        for (auto& c : node->children) w += intrinsicWidth(c.get());
    } else {
        for (auto& c : node->children) {
            float cw = intrinsicWidth(c.get());
            if (cw > w) w = cw;
        }
    }
    int fs = fontSizePx(node->style.fontSize);
    w += lenToPx(node->style.paddingLeft, fs, 0);
    w += lenToPx(node->style.paddingRight, fs, 0);
    w += lenToPx(node->style.borderWidth, fs, 0) * 2;
    if (isListItem(node)) w += 20.0f;
    return w;
}

float layoutBlockChildren(RenderNode* parent, float x, float y, float width);
float layoutGridChildren(RenderNode* parent, float x, float y, float width);
float layoutFlexChildren(RenderNode* parent, float x, float y, float width);
float layoutTableChildren(RenderNode* parent, float x, float y, float width);

struct FloatRegion {
    float leftX = 0;
    float rightX = 0;
    bool active = false;
};

static FloatRegion g_floatRegion;

static void availableLine(float x, float width, float& availX, float& availW) {
    availX = x;
    availW = width;
    if (g_floatRegion.active) {
        if (g_floatRegion.leftX > availX) {
            availW -= (g_floatRegion.leftX - availX);
            availX = g_floatRegion.leftX;
        }
        if (g_floatRegion.rightX > 0 && g_floatRegion.rightX < x + width) {
            availW -= (x + width - g_floatRegion.rightX);
        }
    }
    if (availW < 0) availW = 0;
}

float layoutText(RenderNode* textNode, float& curX, float& curY,
                 float x, float width, float& lineMaxH) {
    int fs = fontSizePx(textNode->style.fontSize);
    if (fs <= 0) fs = 16;

    float lh = computeLineHeight(textNode, fs);
    if (lh > lineMaxH) lineMaxH = lh;

    auto tokens = tokenize(textNode->text);
    float startX = curX;
    float maxX = curX;
    float startY = curY;
    int fw = fontWeightNum(textNode->style.fontWeight);

    for (const auto& tok : tokens) {
        float availX, availW;
        availableLine(x, width, availX, availW);
        float tw = measureTextForNode(textNode, tok, fs, fw);
        if (curX + tw > availX + availW && curX > availX) {
            curY += lh;
            curX = availX;
        }
        if (curX < availX) curX = availX;
        curX += tw;
        if (curX > maxX) maxX = curX;
    }

    textNode->layout.x = startX;
    textNode->layout.y = startY;
    textNode->layout.w = maxX - startX;
    textNode->layout.h = (curY - startY) + lh;

    std::string ta = textNode->style.textAlign;
    if (ta == "center") {
        float lineW = textNode->layout.w;
        float offset = (width - lineW) / 2;
        if (offset > 0) textNode->layout.x += offset;
    } else if (ta == "right") {
        float lineW = textNode->layout.w;
        float offset = width - lineW;
        if (offset > 0) textNode->layout.x += offset;
    }

    return textNode->layout.h;
}

float layoutInlineContent(RenderNode* parent, float& curX, float baseY, float maxW) {
    float maxH = 0;
    for (auto& cp : parent->children) {
        RenderNode* c = cp.get();
        if (!c->isText && isHidden(c)) continue;

        if (c->isText) {
            int fs = fontSizePx(c->style.fontSize); if (fs <= 0) fs = 16;
            int fw = fontWeightNum(c->style.fontWeight);
            float w = measureTextForNode(c, c->text, fs, fw);
            float h = computeLineHeight(c, fs);
            c->layout = {curX, baseY, w, h};
            curX += w;
            if (h > maxH) maxH = h;
        } else if (c->tag == "img") {
            float iw = c->intrinsicW > 0 ? c->intrinsicW : 50.0f;
            float ih = c->intrinsicH > 0 ? c->intrinsicH : 50.0f;
            c->layout = {curX, baseY, iw, ih};
            curX += iw;
            if (ih > maxH) maxH = ih;
        } else if (isInline(c)) {
            int fs = fontSizePx(c->style.fontSize); if (fs <= 0) fs = 16;
            float padL = lenToPx(c->style.paddingLeft, fs, 0);
            float padR = lenToPx(c->style.paddingRight, fs, 0);
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

// ============================================================
// iter3: table
// ============================================================
float layoutTableChildren(RenderNode* table, float x, float y, float width) {
    std::vector<RenderNode*> rows;
    for (auto& c : table->children) {
        if (c->isText) continue;
        if (c->tag == "tr") rows.push_back(c.get());
        else if (c->tag == "tbody" || c->tag == "thead" || c->tag == "tfoot") {
            for (auto& cc : c->children) {
                if (!cc->isText && cc->tag == "tr") rows.push_back(cc.get());
            }
        }
    }
    if (rows.empty()) return 0;

    int maxCols = 0;
    for (auto* r : rows) {
        int n = 0;
        for (auto& c : r->children) if (!c->isText && c->tag == "td") n++;
        if (n > maxCols) maxCols = n;
    }
    if (maxCols <= 0) return 0;

    int fs = fontSizePx(table->style.fontSize); if (fs <= 0) fs = 16;
    float padT = lenToPx(table->style.paddingTop, fs, 0);
    float padL = lenToPx(table->style.paddingLeft, fs, 0);
    float padR = lenToPx(table->style.paddingRight, fs, 0);
    float contentW = width - padL - padR;
    if (contentW < 0) contentW = 0;

    float colW = contentW / maxCols;
    float cy = y + padT;

    for (auto* r : rows) {
        float rowH = 0;
        float cx = x + padL;
        int col = 0;
        for (auto& c : r->children) {
            if (c->isText || c->tag != "td") continue;
            int cfs = fontSizePx(c->style.fontSize); if (cfs <= 0) cfs = 16;
            float cpadL = lenToPx(c->style.paddingLeft, cfs, 0);
            float cpadR = lenToPx(c->style.paddingRight, cfs, 0);
            float cpadT = lenToPx(c->style.paddingTop, cfs, 0);
            float cpadB = lenToPx(c->style.paddingBottom, cfs, 0);
            float innerW = colW - cpadL - cpadR;
            if (innerW < 0) innerW = 0;
            float innerH = layoutBlockChildren(c.get(),
                                                cx + cpadL, cy + cpadT, innerW);
            float totalH = innerH + cpadT + cpadB;
            c->layout.x = cx;
            c->layout.y = cy;
            c->layout.w = colW;
            c->layout.h = totalH;
            if (totalH > rowH) rowH = totalH;
            cx += colW;
            col++;
        }
        r->layout.x = x + padL;
        r->layout.y = cy;
        r->layout.w = contentW;
        r->layout.h = rowH;
        cy += rowH;
    }
    return cy - y;
}

// ============================================================
// flex
// ============================================================
float layoutFlexChildren(RenderNode* parent, float x, float y, float width) {
    int fs = fontSizePx(parent->style.fontSize); if (fs <= 0) fs = 16;
    float padL = lenToPx(parent->style.paddingLeft, fs, 0);
    float padR = lenToPx(parent->style.paddingRight, fs, 0);
    float padT = lenToPx(parent->style.paddingTop, fs, 0);
    float padB = lenToPx(parent->style.paddingBottom, fs, 0);
    float contentW = width - padL - padR;
    if (contentW < 0) contentW = 0;

    float contentX = x + padL;
    float contentY = y + padT;

    bool wrap = (parent->style.flexWrap == "wrap" || parent->style.flexWrap == "wrap-reverse");
    std::string jc = parent->style.justifyContent;
    std::string ai = parent->style.alignItems;

    std::vector<RenderNode*> items;
    for (auto& cp : parent->children) {
        RenderNode* c = cp.get();
        if (isHidden(c)) continue;
        items.push_back(c);
    }
    if (items.empty()) return padT + padB;

    struct ItemSize { float w, h; };
    std::vector<ItemSize> sizes(items.size());
    for (size_t k = 0; k < items.size(); ++k) {
        RenderNode* c = items[k];
        if (c->isText) {
            int cfs = fontSizePx(c->style.fontSize); if (cfs <= 0) cfs = 16;
            int fw = fontWeightNum(c->style.fontWeight);
            sizes[k].w = measureTextForNode(c, c->text, cfs, fw);
            sizes[k].h = computeLineHeight(c, cfs);
        } else if (c->tag == "img") {
            sizes[k].w = c->intrinsicW > 0 ? c->intrinsicW : 50.0f;
            sizes[k].h = c->intrinsicH > 0 ? c->intrinsicH : 50.0f;
        } else {
            int cfs = fontSizePx(c->style.fontSize); if (cfs <= 0) cfs = 16;
            if (c->style.hasWidth) {
                sizes[k].w = lenToPx(c->style.width, cfs, contentW);
            } else {
                sizes[k].w = intrinsicWidth(c);
            }
            if (sizes[k].w < 1) sizes[k].w = 1;
            if (sizes[k].w > contentW) sizes[k].w = contentW;
            sizes[k].h = 0;
        }
    }

    std::vector<std::vector<size_t>> lines;
    std::vector<float> lineW;
    std::vector<size_t> curLine;
    float curW = 0;
    for (size_t k = 0; k < items.size(); ++k) {
        float w = sizes[k].w;
        if (wrap && !curLine.empty() && curW + w > contentW) {
            lines.push_back(curLine); lineW.push_back(curW);
            curLine.clear(); curW = 0;
        }
        curLine.push_back(k);
        curW += w;
    }
    if (!curLine.empty()) { lines.push_back(curLine); lineW.push_back(curW); }

    float lineY = contentY;
    float maxH = 0;

    for (size_t li = 0; li < lines.size(); ++li) {
        auto& line = lines[li];
        float totalW = lineW[li];
        float startX = contentX;

        if (jc == "center") startX = contentX + (contentW - totalW) / 2;
        else if (jc == "flex-end" || jc == "end") startX = contentX + (contentW - totalW);

        float gap = 0;
        if (jc == "space-between" && line.size() > 1) gap = (contentW - totalW) / (line.size() - 1);
        else if (jc == "space-around" && line.size() > 0) gap = (contentW - totalW) / line.size();

        float cx = startX;
        float lineMaxH = 0;

        for (size_t k : line) {
            RenderNode* c = items[k];
            float cw = sizes[k].w;
            float ch = 0;

            if (c->isText) {
                int cfs = fontSizePx(c->style.fontSize); if (cfs <= 0) cfs = 16;
                ch = computeLineHeight(c, cfs);
                c->layout = {cx, lineY, cw, ch};
            } else if (c->tag == "img") {
                ch = sizes[k].h;
                c->layout = {cx, lineY, cw, ch};
            } else {
                int cfs = fontSizePx(c->style.fontSize); if (cfs <= 0) cfs = 16;
                float padL2 = lenToPx(c->style.paddingLeft, cfs, cw);
                float padR2 = lenToPx(c->style.paddingRight, cfs, cw);
                float bW    = lenToPx(c->style.borderWidth, cfs, cw);
                float innerW = cw - padL2 - padR2 - bW * 2;
                if (innerW < 0) innerW = 0;
                float innerX = cx + padL2 + bW;
                float innerY = lineY + lenToPx(c->style.paddingTop, cfs, 0) + bW;
                float innerH = layoutBlockChildren(c, innerX, innerY, innerW);
                c->layout.x = cx;
                c->layout.y = lineY;
                c->layout.w = cw;
                float totalH = innerH + lenToPx(c->style.paddingTop, cfs, 0)
                                       + lenToPx(c->style.paddingBottom, cfs, 0) + bW * 2;
                if (c->style.hasHeight) {
                    float eh = lenToPx(c->style.height, cfs, 0);
                    if (eh > 0) totalH = eh;
                }
                c->layout.h = totalH;
                ch = totalH;
            }
            if (ch > lineMaxH) lineMaxH = ch;
            cx += cw + gap;
        }

        for (size_t k : line) {
            RenderNode* c = items[k];
            if (ai == "center") c->layout.y += (lineMaxH - c->layout.h) / 2;
            else if (ai == "flex-end" || ai == "end") c->layout.y += (lineMaxH - c->layout.h);
        }

        lineY += lineMaxH;
        if (lineY - contentY > maxH) maxH = lineY - contentY;
    }

    maxH += padT + padB;
    return maxH;
}

// ============================================================
// grid
// ============================================================
struct GridCol {
    float px;
    float fr;
    bool isFr;
};

static std::vector<GridCol> parseGridTemplateColumns(const std::string& s,
                                                     float contentW,
                                                     float colGap,
                                                     int& frCount) {
    std::vector<GridCol> out;
    frCount = 0;

    auto rep = s.find("repeat(");
    if (rep != std::string::npos) {
        auto close = s.find(')', rep);
        if (close != std::string::npos) {
            std::string inner = s.substr(rep + 7, close - rep - 7);
            auto comma = inner.find(',');
            int n = 1;
            std::string part;
            if (comma != std::string::npos) {
                try { n = std::stoi(inner.substr(0, comma)); } catch (...) { n = 1; }
                part = inner.substr(comma + 1);
            } else {
                part = inner;
            }
            auto t = part.find_first_not_of(" \t");
            if (t != std::string::npos) part = part.substr(t);
            while (!part.empty() && (part.back() == ' ' || part.back() == '\t')) part.pop_back();

            for (int k = 0; k < n; ++k) {
                GridCol c;
                if (part.size() > 2 && part.substr(part.size() - 2) == "fr") {
                    try { c.fr = std::stof(part.substr(0, part.size() - 2)); } catch (...) { c.fr = 1; }
                    c.isFr = true;
                    frCount++;
                } else {
                    Length l = parseLength(part);
                    c.px = l.value;
                    c.isFr = false;
                }
                out.push_back(c);
            }
            return out;
        }
    }

    std::istringstream ss(s);
    std::string tok;
    while (ss >> tok) {
        GridCol c;
        if (tok.size() > 2 && tok.substr(tok.size() - 2) == "fr") {
            try { c.fr = std::stof(tok.substr(0, tok.size() - 2)); } catch (...) { c.fr = 1; }
            c.isFr = true;
            frCount++;
        } else {
            Length l = parseLength(tok);
            c.px = l.value;
            c.isFr = false;
        }
        out.push_back(c);
    }
    return out;
}

float layoutGridChildren(RenderNode* parent, float x, float y, float width) {
    int fs = fontSizePx(parent->style.fontSize); if (fs <= 0) fs = 16;
    float padL = lenToPx(parent->style.paddingLeft, fs, 0);
    float padR = lenToPx(parent->style.paddingRight, fs, 0);
    float padT = lenToPx(parent->style.paddingTop, fs, 0);
    float padB = lenToPx(parent->style.paddingBottom, fs, 0);
    float contentW = width - padL - padR;
    if (contentW < 0) contentW = 0;

    float colGap = 0, rowGap = 0;
    if (parent->style.hasGridColumnGap) colGap = lenToPx(parent->style.gridColumnGap, fs, 0);
    else if (parent->style.hasGridGap)   colGap = lenToPx(parent->style.gridGap, fs, 0);
    if (parent->style.hasGridRowGap)    rowGap = lenToPx(parent->style.gridRowGap, fs, 0);
    else if (parent->style.hasGridGap)  rowGap = lenToPx(parent->style.gridGap, fs, 0);

    std::vector<GridCol> cols;
    int frCount = 0;
    if (!parent->style.gridTemplateColumns.empty()) {
        cols = parseGridTemplateColumns(parent->style.gridTemplateColumns, contentW, colGap, frCount);
    }

    int numCols = (int)cols.size();
    if (numCols <= 0) numCols = 3;

    std::vector<float> colW(numCols, 0);
    float totalGap = colGap * (numCols - 1);
    float avail = contentW - totalGap;
    if (avail < 0) avail = 0;

    float fixedTotal = 0;
    for (int k = 0; k < numCols && k < (int)cols.size(); ++k) {
        if (!cols[k].isFr) fixedTotal += cols[k].px;
    }
    float frUnit = 0;
    if (frCount > 0) frUnit = std::max(0.0f, (avail - fixedTotal) / frCount);

    for (int k = 0; k < numCols; ++k) {
        if (k < (int)cols.size()) {
            colW[k] = cols[k].isFr ? frUnit * cols[k].fr : cols[k].px;
        } else {
            colW[k] = avail / numCols;
        }
    }

    float contentX = x + padL;
    float contentY = y + padT;

    std::vector<RenderNode*> items;
    for (auto& cp : parent->children) {
        RenderNode* c = cp.get();
        if (isHidden(c)) continue;
        items.push_back(c);
    }

    float rowY = contentY;
    float maxH = 0;
    for (size_t k = 0; k < items.size(); ++k) {
        int col = (int)k % numCols;
        if (col == 0 && k > 0) { rowY += maxH + rowGap; maxH = 0; }
        float cx = contentX;
        for (int j = 0; j < col; ++j) cx += colW[j] + colGap;
        float cw = colW[col];
        RenderNode* c = items[k];

        int cfs = fontSizePx(c->style.fontSize); if (cfs <= 0) cfs = 16;
        float padL2 = lenToPx(c->style.paddingLeft, cfs, cw);
        float padR2 = lenToPx(c->style.paddingRight, cfs, cw);
        float bW    = lenToPx(c->style.borderWidth, cfs, cw);
        float innerW = cw - padL2 - padR2 - bW * 2;
        if (innerW < 0) innerW = 0;
        float innerX = cx + padL2 + bW;
        float innerY = rowY + lenToPx(c->style.paddingTop, cfs, 0) + bW;
        float innerH = layoutBlockChildren(c, innerX, innerY, innerW);

        float totalH = innerH + lenToPx(c->style.paddingTop, cfs, 0)
                              + lenToPx(c->style.paddingBottom, cfs, 0) + bW * 2;
        if (c->style.hasHeight) {
            float eh = lenToPx(c->style.height, cfs, 0);
            if (eh > 0) totalH = eh;
        }

        c->layout.x = cx;
        c->layout.y = rowY;
        c->layout.w = cw;
        c->layout.h = totalH;
        if (totalH > maxH) maxH = totalH;
    }
    rowY += maxH;
    return rowY - y + padB;
}

// ============================================================
// block
// ============================================================
float layoutBlockChildren(RenderNode* parent, float x, float y, float width) {
    // iter3: table 分支
    if (parent->tag == "table") return layoutTableChildren(parent, x, y, width);

    if (isFlex(parent)) return layoutFlexChildren(parent, x, y, width);
    if (isGrid(parent)) return layoutGridChildren(parent, x, y, width);

    FloatRegion savedFloat = g_floatRegion;

    float curY = y;
    float curX = x;
    float lineMaxH = 0;
    Length pendingMarginBottom{};

    for (auto& childPtr : parent->children) {
        RenderNode* c = childPtr.get();
        if (isHidden(c)) { c->layout = {0, 0, 0, 0}; continue; }

        int fs = fontSizePx(c->style.fontSize); if (fs <= 0) fs = 16;

        if (c->style.position == "absolute" || c->style.position == "fixed") {
            RenderNode* anc = findPositionedAncestor(c);
            float baseX = anc ? anc->layout.x + lenToPx(anc->style.borderWidth, fs, 0) : 0;
            float baseY = anc ? anc->layout.y + lenToPx(anc->style.paddingTop, fs, 0) : 0;
            float absX = baseX + (c->style.hasLeft ? lenToPx(c->style.left, fs, 0) : 0);
            float absY = baseY + (c->style.hasTop ? lenToPx(c->style.top, fs, 0) : 0);
            float iw = intrinsicWidth(c);
            if (iw < 1) iw = 100;
            float innerH = layoutBlockChildren(c, absX, absY, iw);
            c->layout.x = absX;
            c->layout.y = absY;
            c->layout.w = iw;
            c->layout.h = innerH + lenToPx(c->style.paddingTop, fs, 0)
                                 + lenToPx(c->style.paddingBottom, fs, 0);
            continue;
        }

        if (c->isText) { layoutText(c, curX, curY, x, width, lineMaxH); continue; }

        if (isInline(c)) {
            float h = fs * 1.5f;
            if (c->tag == "img") {
                float w = c->intrinsicW > 0 ? c->intrinsicW : 50.0f;
                h = c->intrinsicH > 0 ? c->intrinsicH : 50.0f;
                float availX, availW;
                availableLine(x, width, availX, availW);
                if (curX + w > availX + availW && curX > availX) {
                    curY += lineMaxH; curX = availX; lineMaxH = 0;
                }
                if (curX < availX) curX = availX;
                c->layout = {curX, curY, w, h};
                curX += w;
                if (h > lineMaxH) lineMaxH = h;
            } else if (!c->children.empty()) {
                float padL = lenToPx(c->style.paddingLeft, fs, 0);
                float padR = lenToPx(c->style.paddingRight, fs, 0);
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
        } else if (c->style.floatDir == "left" || c->style.floatDir == "right") {
            float fw;
            if (c->style.hasWidth) fw = lenToPx(c->style.width, fs, width);
            else fw = intrinsicWidth(c);
            if (fw < 1) fw = 100;

            float fx;
            if (c->style.floatDir == "left") {
                fx = x;
                if (g_floatRegion.active && g_floatRegion.leftX > fx)
                    fx = g_floatRegion.leftX;
            } else {
                fx = x + width - fw;
                if (g_floatRegion.active && g_floatRegion.rightX > 0 &&
                    g_floatRegion.rightX < x + width)
                    fx = g_floatRegion.rightX - fw;
            }
            float fy = curY;

            FloatRegion savedFR = g_floatRegion;
            g_floatRegion = FloatRegion{};
            float fh = layoutBlockChildren(c, fx, fy, fw);
            g_floatRegion = savedFR;

            float totalH = fh
                + lenToPx(c->style.paddingTop, fs, 0)
                + lenToPx(c->style.paddingBottom, fs, 0)
                + lenToPx(c->style.borderWidth, fs, 0) * 2;

            if (c->style.hasHeight) {
                float eh = lenToPx(c->style.height, fs, 0);
                if (eh > 0) totalH = eh;
            }

            c->layout.x = fx;
            c->layout.y = fy;
            c->layout.w = fw;
            c->layout.h = totalH;

            g_floatRegion.active = true;
            if (c->style.floatDir == "left") {
                if (fx + fw > g_floatRegion.leftX)
                    g_floatRegion.leftX = fx + fw;
            } else {
                if (g_floatRegion.rightX == 0 || fx < g_floatRegion.rightX)
                    g_floatRegion.rightX = fx;
            }

            if (c->layout.h > lineMaxH) lineMaxH = c->layout.h;
        } else {
            if (lineMaxH > 0) { curY += lineMaxH; curX = x; lineMaxH = 0; }

            if (!c->style.clear.empty() && c->style.clear != "none") {
                if (g_floatRegion.active) g_floatRegion = FloatRegion{};
            }

            float mt = lenToPx(c->style.marginTop, fs, width);
            float mbVal = lenToPx(pendingMarginBottom, fs, width);

            float collapsed;
            if (mt < 0 || mbVal < 0) collapsed = mt + mbVal;
            else collapsed = std::max(mt, mbVal);
            curY += collapsed;
            pendingMarginBottom = Length{};

            float ml = lenToPx(c->style.marginLeft, fs, width);
            float mr = lenToPx(c->style.marginRight, fs, width);
            float childX = x + ml;
            float childW = width - ml - mr;
            if (childW < 0) childW = 0;

            if (g_floatRegion.active) {
                float fl = g_floatRegion.leftX;
                float fr = g_floatRegion.rightX > 0 ? g_floatRegion.rightX : (x + width);
                if (fl > childX) {
                    childW -= (fl - childX);
                    childX = fl;
                }
                if (fr < x + width) {
                    childW -= (x + width - fr);
                }
                if (childW < 0) childW = 0;
            }

            if (c->style.hasWidth) {
                float ew = lenToPx(c->style.width, fs, width);
                if (ew > 0) childW = ew;
            }
            if (c->style.hasMinWidth) {
                float mn = lenToPx(c->style.minWidth, fs, width);
                if (childW < mn) childW = mn;
            }
            if (c->style.hasMaxWidth) {
                float mx = lenToPx(c->style.maxWidth, fs, width);
                if (mx > 0 && childW > mx) childW = mx;
            }

            float markerW = 0;
            if (isListItem(c)) {
                markerW = 20.0f;
                childX += markerW;
                childW -= markerW;
                if (childW < 0) childW = 0;
            }

            float padL = lenToPx(c->style.paddingLeft, fs, childW);
            float padR = lenToPx(c->style.paddingRight, fs, childW);
            float bW   = lenToPx(c->style.borderWidth, fs, childW);

            float contentW = childW - padL - padR - bW * 2;
            if (contentW < 0) contentW = 0;

            float relX = 0, relY = 0;
            if (c->style.position == "relative") {
                if (c->style.hasLeft)        relX += lenToPx(c->style.left,   fs, 0);
                else if (c->style.hasRight)  relX -= lenToPx(c->style.right,  fs, 0);
                if (c->style.hasTop)         relY += lenToPx(c->style.top,    fs, 0);
                else if (c->style.hasBottom) relY -= lenToPx(c->style.bottom, fs, 0);
            }

            c->layout.x = childX + relX;
            c->layout.y = curY   + relY;
            c->layout.w = childW + markerW;

            float innerX = c->layout.x + padL + bW;
            float innerY = c->layout.y + lenToPx(c->style.paddingTop, fs, 0) + bW;
            float innerH = layoutBlockChildren(c, innerX, innerY, contentW);

            float totalH;
            if (c->style.isBorderBox() && c->style.hasHeight) {
                totalH = lenToPx(c->style.height, fs, 0);
            } else {
                totalH = innerH + lenToPx(c->style.paddingTop, fs, 0)
                                 + lenToPx(c->style.paddingBottom, fs, 0) + bW * 2;
                if (c->style.hasHeight) {
                    float eh = lenToPx(c->style.height, fs, 0);
                    if (eh > 0) totalH = eh;
                }
            }
            if (c->style.hasMinHeight) {
                float mn = lenToPx(c->style.minHeight, fs, 0);
                if (totalH < mn) totalH = mn;
            }
            if (c->style.hasMaxHeight) {
                float mx = lenToPx(c->style.maxHeight, fs, 0);
                if (mx > 0 && totalH > mx) totalH = mx;
            }

            c->layout.h = totalH;
            curY += totalH;
            pendingMarginBottom = c->style.marginBottom;
        }
    }

    if (lineMaxH > 0) curY += lineMaxH;
    float pbm = lenToPx(pendingMarginBottom, 16, width);
    curY += pbm;

    g_floatRegion = savedFloat;
    return curY - y;
}

} // namespace

void layoutTree(RenderNode* root, int viewportWidth, int viewportHeight) {
    if (!root) return;
    g_viewportW = viewportWidth;
    g_viewportH = viewportHeight;
    g_floatRegion = FloatRegion{};

    root->layout = {0, 0, (float)viewportWidth, (float)viewportHeight};

    float y = 0;
    for (auto& c : root->children) {
        if (isHidden(c.get())) continue;
        g_floatRegion = FloatRegion{};
        float h = layoutBlockChildren(c.get(), 0, y, (float)viewportWidth);
        c->layout.x = 0;
        c->layout.y = y;
        c->layout.w = (float)viewportWidth;
        c->layout.h = h;
        y += h;
    }
}

} // namespace dm::render