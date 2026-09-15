#pragma once
#include <string>
#include <vector>

namespace dm::render {

using MeasureFn = float(*)(const std::string&, int, const std::string&, int);

void setTextMeasurer(MeasureFn fn);

float estimateTextWidth(const std::string& text, int fontSizePx);

float measureText(const std::string& text, int fontSizePx,
                  const std::string& fontFamily, int fontWeight = 400);

// iter2: 按字体列表回退
float measureTextWithList(const std::string& text, int fontSizePx,
                          const std::vector<std::string>& familyList,
                          int fontWeight = 400);

void initTextMeasurer();

} // namespace dm::render