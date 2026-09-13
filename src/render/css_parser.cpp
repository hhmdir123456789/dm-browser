#include "render/css_parser.h"
#include <cctype>
#include <sstream>

namespace dm::render {

namespace {

bool isSpace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && isSpace(s[a])) a++;
    while (b > a && isSpace(s[b - 1])) b--;
    return s.substr(a, b - a);
}

std::string stripComments(const std::string& s) {
    std::string out;
    size_t i = 0;
    while (i < s.size()) {
        if (i + 1 < s.size() && s[i] == '/' && s[i + 1] == '*') {
            size_t end = s.find("*/", i + 2);
            if (end == std::string::npos) break;
            i = end + 2;
        } else {
            out += s[i++];
        }
    }
    return out;
}

void parseSelectorPart(const std::string& part, CssRule& rule) {
    std::string s = trim(part);
    if (s.empty()) return;

    size_t i = 0;
    if (!s.empty() && s[0] != '.' && s[0] != '#') {
        while (i < s.size() && s[i] != '.' && s[i] != '#') i++;
        std::string t = s.substr(0, i);
        for (char& c : t) c = (char)std::tolower((unsigned char)c);
        rule.tag = t;
    }
    while (i < s.size()) {
        char c = s[i++];
        size_t start = i;
        while (i < s.size() && s[i] != '.' && s[i] != '#') i++;
        std::string name = s.substr(start, i - start);
        if (c == '.') rule.classes.push_back(name);
        else if (c == '#') rule.id = name;
    }
}

std::vector<CssDecl> parseDecls(const std::string& body) {
    std::vector<CssDecl> out;
    std::stringstream ss(body);
    std::string item;
    while (std::getline(ss, item, ';')) {
        size_t colon = item.find(':');
        if (colon == std::string::npos) continue;
        CssDecl d;
        d.property = trim(item.substr(0, colon));
        d.value = trim(item.substr(colon + 1));
        if (!d.property.empty() && !d.value.empty()) {
            out.push_back(d);
        }
    }
    return out;
}

} // namespace

std::vector<CssRule> parseCss(const std::string& css) {
    std::vector<CssRule> rules;
    std::string s = stripComments(css);
    size_t i = 0;

    while (i < s.size()) {
        size_t brace = s.find('{', i);
        if (brace == std::string::npos) break;

        std::string selectorGroup = trim(s.substr(i, brace - i));
        size_t closeBrace = s.find('}', brace);
        if (closeBrace == std::string::npos) break;

        std::string declBody = s.substr(brace + 1, closeBrace - brace - 1);

        std::stringstream selStream(selectorGroup);
        std::string singleSel;
        while (std::getline(selStream, singleSel, ',')) {
            CssRule rule;
            parseSelectorPart(singleSel, rule);
            if (rule.tag.empty() && rule.id.empty() && rule.classes.empty())
                continue;
            rule.decls = parseDecls(declBody);
            if (!rule.decls.empty()) rules.push_back(rule);
        }

        i = closeBrace + 1;
    }

    return rules;
}

bool matchesSelector(const std::string& tag,
                     const std::string& id,
                     const std::string& className,
                     const CssRule& rule) {
    if (!rule.tag.empty() && rule.tag != tag) return false;
    if (!rule.id.empty() && rule.id != id) return false;
    if (!rule.classes.empty()) {
        std::vector<std::string> tokens;
        std::stringstream ss(className);
        std::string t;
        while (ss >> t) tokens.push_back(t);
        for (const auto& want : rule.classes) {
            bool found = false;
            for (const auto& have : tokens) {
                if (have == want) { found = true; break; }
            }
            if (!found) return false;
        }
    }
    return true;
}

} // namespace dm::render