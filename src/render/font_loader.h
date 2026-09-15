#pragma once
#include <string>
#include <vector>

namespace dm::render {

// iter3: @font-face 注册的私有字体
struct FontFaceEntry {
    std::string family;
    std::string srcPath;
    bool loaded = false;
};

void clearFontFaces();
void addFontFace(const std::string& family, const std::string& src);
void loadAllFontFaces();
std::string lookupFontFace(const std::string& family);

} // namespace dm::render