#pragma once
#include <string>
#include <vector>
#include <map>

namespace dm::render {

struct CssDecl {
    std::string property;
    std::string value;
};

struct CssRule {
    std::string tag;
    std::string id;
    std::vector<std::string> classes;
    std::vector<CssDecl> decls;

    int specificity() const {
        int s = 0;
        if (!id.empty()) s += 100;
        s += (int)classes.size() * 10;
        if (!tag.empty()) s += 1;
        return s;
    }
};

std::vector<CssRule> parseCss(const std::string& css);

bool matchesSelector(const std::string& tag,
                     const std::string& id,
                     const std::string& className,
                     const CssRule& rule);

} // namespace dm::render