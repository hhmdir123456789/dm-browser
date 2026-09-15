#pragma once
#include <string>

namespace dm::render {

struct Length {
    float value = 0;
    enum Unit { Px, Percent, Em, Rem, Vw, Vh } unit = Px;
    bool isPx() const { return unit == Px; }
    bool isZero() const { return value == 0; }
};

Length parseLength(const std::string& s);

float resolveLength(const Length& len, int fontSizePx, int rootFontSize,
                    int viewportW, int viewportH, float percentBase = 0);

float evalCalc(const std::string& expr, int fontSizePx, int rootFontSize,
               int viewportW, int viewportH, float percentBase);

} // namespace dm::render