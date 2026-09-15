#pragma once
#include <string>
#include <vector>

namespace dm::render {

struct FilterOp {
    std::string name;   // grayscale / brightness / blur / contrast / invert
    float value = 0;    // grayscale/invert: 0..1；brightness/contrast: 倍数；blur: 像素
};

// 解析 "grayscale(50%) brightness(1.2) blur(3px)"
std::vector<FilterOp> parseFilterList(const std::string& s);

// 对 BGRA 缓冲逐像素应用 filter
void applyFilters(std::vector<unsigned char>& bgra, int w, int h,
                  const std::vector<FilterOp>& filters);

} // namespace dm::render