#include "render/filter.h"
#include <cctype>
#include <cmath>
#include <algorithm>

namespace dm::render {

namespace {

std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    size_t b = s.find_last_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    return s.substr(a, b - a + 1);
}

float parseValue(const std::string& s) {
    std::string t = trim(s);
    if (t.empty()) return 0;
    bool isPercent = (!t.empty() && t.back() == '%');
    if (isPercent) t.pop_back();
    // 去掉 px 后缀
    if (t.size() >= 2 && t.substr(t.size() - 2) == "px") t = t.substr(0, t.size() - 2);
    float v = 0;
    try { v = std::stof(t); } catch (...) { return 0; }
    if (isPercent) v /= 100.0f;
    return v;
}

void applyGrayscale(std::vector<unsigned char>& b, int w, int h, float amount) {
    if (amount <= 0) return;
    if (amount > 1) amount = 1;
    size_t n = (size_t)w * h;
    for (size_t i = 0; i < n; ++i) {
        unsigned char* p = &b[i * 4];
        int lum = (int)(0.299f * p[2] + 0.587f * p[1] + 0.114f * p[0]);
        p[0] = (unsigned char)(p[0] + (lum - p[0]) * amount);
        p[1] = (unsigned char)(p[1] + (lum - p[1]) * amount);
        p[2] = (unsigned char)(p[2] + (lum - p[2]) * amount);
    }
}

void applyBrightness(std::vector<unsigned char>& b, int w, int h, float factor) {
    if (factor <= 0) factor = 1;
    size_t n = (size_t)w * h;
    for (size_t i = 0; i < n; ++i) {
        unsigned char* p = &b[i * 4];
        for (int k = 0; k < 3; ++k) {
            int v = (int)(p[k] * factor);
            if (v > 255) v = 255;
            p[k] = (unsigned char)v;
        }
    }
}

void applyContrast(std::vector<unsigned char>& b, int w, int h, float factor) {
    if (factor <= 0) factor = 1;
    float c = factor;
    size_t n = (size_t)w * h;
    for (size_t i = 0; i < n; ++i) {
        unsigned char* p = &b[i * 4];
        for (int k = 0; k < 3; ++k) {
            int v = (int)((p[k] - 128) * c + 128);
            if (v < 0) v = 0;
            if (v > 255) v = 255;
            p[k] = (unsigned char)v;
        }
    }
}

void applyInvert(std::vector<unsigned char>& b, int w, int h, float amount) {
    if (amount <= 0) return;
    if (amount > 1) amount = 1;
    size_t n = (size_t)w * h;
    for (size_t i = 0; i < n; ++i) {
        unsigned char* p = &b[i * 4];
        for (int k = 0; k < 3; ++k) {
            int inv = 255 - p[k];
            p[k] = (unsigned char)(p[k] + (inv - p[k]) * amount);
        }
    }
}

// 简单 box blur
void applyBlur(std::vector<unsigned char>& b, int w, int h, int radius) {
    if (radius <= 0 || w <= 0 || h <= 0) return;
    if (radius > 20) radius = 20;

    std::vector<unsigned char> tmp(b.size());
    int win = radius * 2 + 1;

    // 水平方向
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int sr = 0, sg = 0, sb = 0, sa = 0, cnt = 0;
            for (int dx = -radius; dx <= radius; ++dx) {
                int nx = x + dx;
                if (nx < 0) nx = 0;
                if (nx >= w) nx = w - 1;
                const unsigned char* p = &b[((size_t)y * w + nx) * 4];
                sb += p[0]; sg += p[1]; sr += p[2]; sa += p[3];
                cnt++;
            }
            unsigned char* q = &tmp[((size_t)y * w + x) * 4];
            q[0] = (unsigned char)(sb / cnt);
            q[1] = (unsigned char)(sg / cnt);
            q[2] = (unsigned char)(sr / cnt);
            q[3] = (unsigned char)(sa / cnt);
        }
    }

    // 垂直方向
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int sr = 0, sg = 0, sb = 0, sa = 0, cnt = 0;
            for (int dy = -radius; dy <= radius; ++dy) {
                int ny = y + dy;
                if (ny < 0) ny = 0;
                if (ny >= h) ny = h - 1;
                const unsigned char* p = &tmp[((size_t)ny * w + x) * 4];
                sb += p[0]; sg += p[1]; sr += p[2]; sa += p[3];
                cnt++;
            }
            unsigned char* q = &b[((size_t)y * w + x) * 4];
            q[0] = (unsigned char)(sb / cnt);
            q[1] = (unsigned char)(sg / cnt);
            q[2] = (unsigned char)(sr / cnt);
            q[3] = (unsigned char)(sa / cnt);
        }
    }
    (void)win;
}

} // namespace

std::vector<FilterOp> parseFilterList(const std::string& s) {
    std::vector<FilterOp> out;
    if (s.empty() || s == "none") return out;

    size_t i = 0;
    while (i < s.size()) {
        // 找下一个函数名
        while (i < s.size() && !std::isalpha((unsigned char)s[i])) i++;
        if (i >= s.size()) break;
        size_t nameStart = i;
        while (i < s.size() && std::isalpha((unsigned char)s[i])) i++;
        std::string name = s.substr(nameStart, i - nameStart);
        for (char& c : name) c = (char)std::tolower((unsigned char)c);

        while (i < s.size() && s[i] != '(') i++;
        if (i >= s.size()) break;
        i++; // 跳过 (
        size_t argStart = i;
        while (i < s.size() && s[i] != ')') i++;
        std::string arg = trim(s.substr(argStart, i - argStart));
        if (i < s.size()) i++; // 跳过 )

        FilterOp op;
        op.name = name;
        op.value = parseValue(arg);
        out.push_back(op);
    }
    return out;
}

void applyFilters(std::vector<unsigned char>& bgra, int w, int h,
                  const std::vector<FilterOp>& filters) {
    if (w <= 0 || h <= 0 || bgra.empty()) return;
    for (const auto& f : filters) {
        if (f.name == "grayscale")      applyGrayscale(bgra, w, h, f.value);
        else if (f.name == "brightness") applyBrightness(bgra, w, h, f.value);
        else if (f.name == "contrast")   applyContrast(bgra, w, h, f.value);
        else if (f.name == "invert")     applyInvert(bgra, w, h, f.value);
        else if (f.name == "blur")       applyBlur(bgra, w, h, (int)f.value);
    }
}

} // namespace dm::render