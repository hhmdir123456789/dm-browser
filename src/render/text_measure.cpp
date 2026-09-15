#include "render/text_measure.h"
#include <cctype>
#include <cstring>
#include <vector>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace dm::render {

static MeasureFn g_measurer = nullptr;

void setTextMeasurer(MeasureFn fn) { g_measurer = fn; }

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

static float gdiMeasureTextImpl(const std::string& text, int fs,
                                 const std::string& family, int weight) {
    if (text.empty()) return 0;
    if (fs <= 0) fs = 16;
    if (weight <= 0) weight = 400;

    HDC hdc = GetDC(nullptr);
    if (!hdc) return estimateTextWidth(text, fs);

    const char* fam = family.empty() ? "Microsoft YaHei" : family.c_str();
    HFONT font = CreateFontA(fs, 0, 0, 0, weight, FALSE, FALSE, FALSE,
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

static int CALLBACK enumFontProc(const LOGFONTA*, const TEXTMETRICA*,
                                  DWORD, LPARAM) {
    return 0;
}

// 检测系统是否安装该字体
static bool fontExists(const std::string& name) {
    if (name.empty()) return false;
    HDC hdc = GetDC(nullptr);
    if (!hdc) return false;

    LOGFONTA lf{};
    lf.lfCharSet = DEFAULT_CHARSET;
    std::strncpy(lf.lfFaceName, name.c_str(), LF_FACESIZE - 1);
    lf.lfFaceName[LF_FACESIZE - 1] = 0;

    int found = 0;
    EnumFontFamiliesExA(hdc, &lf,
        [](const LOGFONTA*, const TEXTMETRICA*, DWORD, LPARAM lp) -> int {
            *(int*)lp = 1;
            return 0;
        }, (LPARAM)&found, 0);

    ReleaseDC(nullptr, hdc);
    return found != 0;
}

#endif

void initTextMeasurer() {
#ifdef _WIN32
    g_measurer = gdiMeasureTextImpl;
#endif
}

static bool isGenericFamily(const std::string& f) {
    return f == "serif" || f == "sans-serif" || f == "monospace" ||
           f == "cursive" || f == "fantasy" ||
           f == "system-ui" || f == "ui-serif" ||
           f == "ui-sans-serif" || f == "ui-monospace";
}

static std::string mapGenericFamily(const std::string& f) {
    if (f == "serif")      return "Times New Roman";
    if (f == "sans-serif") return "Microsoft YaHei";
    if (f == "monospace")  return "Consolas";
    if (f == "cursive")    return "Comic Sans MS";
    if (f == "fantasy")    return "Impact";
    if (f == "system-ui" || f == "ui-sans-serif") return "Microsoft YaHei";
    if (f == "ui-serif")   return "Times New Roman";
    if (f == "ui-monospace") return "Consolas";
    return f;
}

// iter2: 按列表回退
float measureTextWithList(const std::string& text, int fontSizePx,
                          const std::vector<std::string>& familyList,
                          int fontWeight) {
#ifdef _WIN32
    if (g_measurer) {
        for (const auto& f : familyList) {
            if (f.empty()) continue;
            std::string actual = isGenericFamily(f) ? mapGenericFamily(f) : f;
            if (fontExists(actual)) {
                return g_measurer(text, fontSizePx, actual, fontWeight);
            }
        }
        return g_measurer(text, fontSizePx, "", fontWeight);
    }
#endif
    return estimateTextWidth(text, fontSizePx);
}

float measureText(const std::string& text, int fontSizePx,
                  const std::string& fontFamily, int fontWeight) {
    if (g_measurer) return g_measurer(text, fontSizePx, fontFamily, fontWeight);
    return estimateTextWidth(text, fontSizePx);
}

} // namespace dm::render