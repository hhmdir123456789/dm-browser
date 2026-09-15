#pragma once
#include "render/render_node.h"
#include <string>
#include <vector>

namespace dm::render {

struct CssDecl {
    std::string property;
    std::string value;
};

struct CssSelector {
    std::string tag;
    std::string id;
    std::vector<std::string> classes;
    std::string attrName;
    std::string attrValue;
    bool hasAttr = false;

    bool hasNot = false;
    bool notInvalid = false;
    std::string notTag;
    std::string notId;
    std::string notClass;

    std::vector<std::string> pseudos;
};

enum class Combinator {
    Descendant,
    Child,
    Adjacent,
    Sibling,
};

struct CssRule {
    std::vector<CssSelector> parts;
    std::vector<Combinator>  combinators;
    std::vector<CssDecl>     decls;

    // iter4: 所属 @media 条件，空表示无条件
    std::string mediaCondition;

    int specificity() const {
        int s = 0;
        for (const auto& p : parts) {
            if (!p.id.empty()) s += 100;
            s += (int)p.classes.size() * 10;
            if (p.hasAttr) s += 10;
            if (p.hasNot)  s += 10;
            s += (int)p.pseudos.size() * 10;
            if (!p.tag.empty()) s += 1;
        }
        return s;
    }
};

// iter3: @font-face 提取结果
struct FontFaceRule {
    std::string family;
    std::string src;
};

std::vector<CssRule> parseCss(const std::string& css,
                              std::vector<FontFaceRule>* outFontFaces = nullptr);

bool matchesSelector(const RenderNode* node, const CssRule& rule);

} // namespace dm::render