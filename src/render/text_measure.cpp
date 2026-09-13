#include "render/text_measure.h"
#include <cctype>

#ifdef _WIN32
#include <windows.h>
#endif

namespace dm::render {

static MeasureFn g_measurer = nullptr;

void setTextMeasurer(MeasureFn fn) {
    g_measurer = fn;
}

// 后备：按字符估算（非 Windows 平台或 GDI 失败时）
float estimateTextWidth(const std::string& text, int fontSizePx) {
    float fs = (float)fontSizePx;
    if (fs <= 0) fs = 16;
    float w = 0;
    size_t i = 0;
    while (i < text.size()) {
        unsigned char c = (unsigned char)text[i];
        if (c < 0x80) { w += fs * 0.50f; i++; }
        else if ((c & 0xE0) == 0xC0) { w += fs * 0.50f; i += 2; }
        else if ((c & 0xF0) == 0xE0) { w += fs * 1.0f; i += 3; }
        else if ((c & 0xF8) == 0xF0) { w += fs * 1.0f; i += 4; }
        else { w += fs * 0.50f; i++; }
    }
    return w;
}

#ifdef _WIN32
static float gdiMeasureText(const std::string& text, int fs, const std::string& family) {
    if (text.empty()) return 0;
    if (fs <= 0) fs = 16;

    HDC hdc = GetDC(nullptr);
    if (!hdc) return estimateTextWidth(text, fs);

    const char* fam = family.empty() ? "Microsoft YaHei" : family.c_str();
    HFONT font = CreateFontA(fs, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE, fam);
    HGDIOBJ oldFont = SelectObject(hdc, font);

    SIZE sz{};
    GetTextExtentPoint32A(hdc, text.c_str(), (int)text.size(), &sz);

    SelectObject(hdc, oldFont);
    DeleteObject(font);
    ReleaseDC(nullptr, hdc);
    return (float)sz.cx;
}
#endif

void initTextMeasurer() {
#ifdef _WIN32
    g_measurer = gdiMeasureText;
#endif
}

float measureText(const std::string& text, int fontSizePx, const std::string& fontFamily) {
    if (g_measurer) return g_measurer(text, fontSizePx, fontFamily);
    return estimateTextWidth(text, fontSizePx);
}

} // namespace dm::render