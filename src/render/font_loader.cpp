#include "render/font_loader.h"
#include <windows.h>
#include <shlwapi.h>
#include <vector>
#include <string>

#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shlwapi.lib")

namespace dm::render {

namespace {

std::vector<FontFaceEntry> g_fontFaces;

std::wstring utf8ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    if (len <= 0) return L"";
    std::wstring out(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], len);
    return out;
}

std::string stripQuotes(const std::string& s) {
    if (s.size() >= 2 &&
        ((s.front() == '"' && s.back() == '"') ||
         (s.front() == '\'' && s.back() == '\''))) {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    size_t b = s.find_last_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    return s.substr(a, b - a + 1);
}

} // namespace

void clearFontFaces() {
    for (auto& f : g_fontFaces) {
        if (f.loaded && !f.srcPath.empty()) {
            std::wstring w = utf8ToWide(f.srcPath);
            RemoveFontResourceExW(w.c_str(), FR_PRIVATE, 0);
        }
    }
    g_fontFaces.clear();
}

void addFontFace(const std::string& family, const std::string& src) {
    FontFaceEntry e;
    e.family = stripQuotes(trim(family));
    std::string s = trim(src);

    // 解析 url('xxx') / url("xxx") / url(xxx)
    auto up = s.find("url(");
    if (up != std::string::npos) {
        size_t start = up + 4;
        while (start < s.size() && (s[start] == ' ' || s[start] == '"' || s[start] == '\'')) start++;
        size_t end = start;
        while (end < s.size() && s[end] != ')' && s[end] != '"' && s[end] != '\'') end++;
        e.srcPath = s.substr(start, end - start);
    } else {
        e.srcPath = stripQuotes(s);
    }

    if (e.family.empty() || e.srcPath.empty()) return;
    g_fontFaces.push_back(e);
}

void loadAllFontFaces() {
    for (auto& f : g_fontFaces) {
        if (f.loaded) continue;
        std::wstring w = utf8ToWide(f.srcPath);
        if (AddFontResourceExW(w.c_str(), FR_PRIVATE, 0) > 0) {
            f.loaded = true;
        }
    }
}

std::string lookupFontFace(const std::string& family) {
    for (auto& f : g_fontFaces) {
        if (f.loaded && f.family == family) return f.family;
    }
    return "";
}

} // namespace dm::render