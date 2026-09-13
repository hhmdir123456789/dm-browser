#pragma once
#include <string>

namespace dm::render {

using MeasureFn = float(*)(const std::string&, int, const std::string&);

void setTextMeasurer(MeasureFn fn);

float estimateTextWidth(const std::string& text, int fontSizePx);

float measureText(const std::string& text, int fontSizePx, const std::string& fontFamily);

// 初始化：Windows 下自动启用 GDI 度量
void initTextMeasurer();

} // namespace dm::render