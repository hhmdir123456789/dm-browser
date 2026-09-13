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

// 解析单个选择器段："div.foo#bar" → CssSelector
CssSelector parseSelectorPart(const std::string& part) {
    CssSelector sel;
    std::string s = trim(part);
    if (s.empty()) return sel;

    size_t i = 0;
    // 可选 tag 前缀
    if (s[0] != '.' && s[0] != '#' && s[0] != ':') {
        while (i < s.size() && s[i] != '.' && s[i] != '#' && s[i] != ':') i++;
        std::string t = s.substr(0, i);
        for (char& c : t) c = (char)std::tolower((unsigned char)c);
        sel.tag = t;
    }
    // 循环读 .class / #id，跳过 :pseudo
    while (i < s.size()) {
        char c = s[i++];
        if (c == ':') {
            // 跳过伪类，直到下一个 . # : 或末尾
            while (i < s.size() && s[i] != '.' && s[i] != '#' && s[i] != ':') i++;
            continue;
        }
        size_t start = i;
        while (i < s.size() && s[i] != '.' && s[i] != '#' && s[i] != ':') i++;
        std::string name = s.substr(start, i - start);
        if (c == '.') sel.classes.push_back(name);
        else if (c == '#') sel.id = name;
    }
    return sel;
}

// 解析后代选择器："div#main .foo .bar" → [{tag:"div", id:"main"}, {classes:["foo"]}, {classes:["bar"]}]
std::vector<CssSelector> parseSelectorChain(const std::string& s) {
    std::vector<CssSelector> parts;
    std::stringstream ss(s);
    std::string item;
    while (ss >> item) {
        parts.push_back(parseSelectorPart(item));
    }
    return parts;
}

// 解析 "color:red; font-size:14px" → 声明列表
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

// 单个选择器段是否匹配一个节点
bool matchSingle(const CssSelector& sel, const RenderNode* node) {
    if (!node) return false;
    if (!sel.tag.empty() && sel.tag != node->tag) return false;
    if (!sel.id.empty() && sel.id != node->id) return false;
    if (!sel.classes.empty()) {
        std::vector<std::string> tokens;
        std::stringstream ss(node->className);
        std::string t;
        while (ss >> t) tokens.push_back(t);
        for (const auto& want : sel.classes) {
            bool found = false;
            for (const auto& have : tokens) {
                if (have == want) { found = true; break; }
            }
            if (!found) return false;
        }
    }
    return true;
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
        auto decls = parseDecls(declBody);
        if (decls.empty()) {
            i = closeBrace + 1;
            continue;
        }

        // 逗号分隔的选择器组："div, .foo, #bar"
        std::stringstream selStream(selectorGroup);
        std::string singleSel;
        while (std::getline(selStream, singleSel, ',')) {
            CssRule rule;
            rule.parts = parseSelectorChain(singleSel);
            // 过滤无效 part
            bool valid = !rule.parts.empty();
            for (auto& p : rule.parts) {
                if (p.tag.empty() && p.id.empty() && p.classes.empty()) {
                    valid = false;
                    break;
                }
            }
            if (!valid) continue;
            rule.decls = decls;
            rules.push_back(rule);
        }

        i = closeBrace + 1;
    }

    return rules;
}

bool matchesSelector(const RenderNode* node, const CssRule& rule) {
    if (!node || rule.parts.empty()) return false;

    // 最右的 part 必须匹配 node 本身
    int i = (int)rule.parts.size() - 1;
    if (!matchSingle(rule.parts[i], node)) return false;
    i--;

    if (i < 0) return true;  // 只有一个 part

    // 从 parent 往上找左侧的 parts（可以跨节点）
    const RenderNode* cur = node->parent;
    while (i >= 0 && cur) {
        if (matchSingle(rule.parts[i], cur)) {
            i--;
        }
        cur = cur->parent;
    }
    return i < 0;
}

} // namespace dm::render