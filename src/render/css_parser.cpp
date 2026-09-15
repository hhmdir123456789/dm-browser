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

CssSelector parseSelectorPart(const std::string& part) {
    CssSelector sel;
    std::string s = trim(part);
    if (s.empty()) return sel;

    if (s == ":root") { sel.tag = "html"; return sel; }
    if (s == "*") { sel.attrName = "*"; sel.hasAttr = true; return sel; }

    size_t i = 0;
    if (s[0] != '.' && s[0] != '#' && s[0] != '[' && s[0] != ':') {
        while (i < s.size() && s[i] != '.' && s[i] != '#' && s[i] != '[' && s[i] != ':') i++;
        std::string t = s.substr(0, i);
        for (char& c : t) c = (char)std::tolower((unsigned char)c);
        sel.tag = t;
    }

    while (i < s.size()) {
        char c = s[i];
        if (c == ':') {
            i++;
            size_t start = i;
            while (i < s.size() && s[i] != '.' && s[i] != '#' && s[i] != '[' && s[i] != ':'
                   && s[i] != '(' && s[i] != ' ') i++;
            std::string pseudo = s.substr(start, i - start);
            for (char& ch : pseudo) ch = (char)std::tolower((unsigned char)ch);

            if (pseudo == "root") { sel.tag = "html"; continue; }

            if (i < s.size() && s[i] == '(') {
                int depth = 1;
                size_t argStart = i + 1;
                i++;
                while (i < s.size() && depth > 0) {
                    if (s[i] == '(') depth++;
                    else if (s[i] == ')') {
                        depth--;
                        if (depth == 0) break;
                    }
                    i++;
                }
                std::string args = s.substr(argStart, i - argStart);
                if (i < s.size()) i++;

                if (pseudo == "not") {
                    std::string a = trim(args);
                    if (!a.empty()) {
                        CssSelector notSel = parseSelectorPart(a);
                        bool tagOnly   = !notSel.tag.empty() &&
                                         notSel.id.empty() &&
                                         notSel.classes.empty() &&
                                         !notSel.hasAttr &&
                                         notSel.pseudos.empty();
                        bool idOnly    = notSel.tag.empty() &&
                                         !notSel.id.empty() &&
                                         notSel.classes.empty() &&
                                         !notSel.hasAttr &&
                                         notSel.pseudos.empty();
                        bool classOnly = notSel.tag.empty() &&
                                         notSel.id.empty() &&
                                         notSel.classes.size() == 1 &&
                                         !notSel.hasAttr &&
                                         notSel.pseudos.empty();
                        sel.hasNot = true;
                        if (tagOnly)        sel.notTag   = notSel.tag;
                        else if (idOnly)    sel.notId    = notSel.id;
                        else if (classOnly) sel.notClass = notSel.classes[0];
                        else                sel.notInvalid = true;
                    }
                    continue;
                }

                sel.hasAttr = true;
                sel.attrName = "__pseudo__" + pseudo;
                return sel;
            }

            if (pseudo == "first-child"  || pseudo == "last-child" ||
                pseudo == "only-child"   || pseudo == "first-of-type" ||
                pseudo == "last-of-type" || pseudo == "only-of-type") {
                sel.pseudos.push_back(pseudo);
                continue;
            }

            sel.hasAttr = true;
            sel.attrName = "__pseudo__" + pseudo;
            return sel;
        }
        if (c == '[') {
            size_t close = s.find(']', i);
            if (close == std::string::npos) break;
            std::string inner = s.substr(i + 1, close - i - 1);
            i = close + 1;
            auto eq = inner.find('=');
            if (eq == std::string::npos) {
                sel.attrName = trim(inner);
            } else {
                sel.attrName = trim(inner.substr(0, eq));
                std::string v = trim(inner.substr(eq + 1));
                if (v.size() >= 2 &&
                    ((v.front() == '"' && v.back() == '"') ||
                     (v.front() == '\'' && v.back() == '\''))) {
                    v = v.substr(1, v.size() - 2);
                }
                sel.attrValue = v;
            }
            sel.hasAttr = !sel.attrName.empty();
            continue;
        }
        if (c == '.' || c == '#') {
            i++;
            size_t start = i;
            while (i < s.size() && s[i] != '.' && s[i] != '#' && s[i] != '[' && s[i] != ':') i++;
            std::string name = s.substr(start, i - start);
            if (c == '.') sel.classes.push_back(name);
            else sel.id = name;
            continue;
        }
        i++;
    }
    return sel;
}

void parseSelectorChain(const std::string& s,
                        std::vector<CssSelector>& parts,
                        std::vector<Combinator>& combinators) {
    std::string cur;
    size_t i = 0;
    while (i < s.size()) {
        char c = s[i];
        if (c == '>' || c == '+' || c == '~') {
            std::string t = trim(cur);
            if (!t.empty()) parts.push_back(parseSelectorPart(t));
            cur.clear();
            if (!parts.empty()) {
                if (c == '>') combinators.push_back(Combinator::Child);
                else if (c == '+') combinators.push_back(Combinator::Adjacent);
                else combinators.push_back(Combinator::Sibling);
            }
            i++;
            continue;
        }
        if (isSpace(c)) {
            std::string t = trim(cur);
            if (!t.empty()) {
                parts.push_back(parseSelectorPart(t));
                cur.clear();
                size_t j = i;
                while (j < s.size() && isSpace(s[j])) j++;
                if (j < s.size() && (s[j] == '>' || s[j] == '+' || s[j] == '~')) {
                } else if (j < s.size()) {
                    combinators.push_back(Combinator::Descendant);
                }
                i = j;
                continue;
            }
            i++;
            continue;
        }
        cur += c;
        i++;
    }
    std::string t = trim(cur);
    if (!t.empty()) parts.push_back(parseSelectorPart(t));
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
        if (!d.property.empty() && !d.value.empty()) out.push_back(d);
    }
    return out;
}

bool matchSingle(const CssSelector& sel, const RenderNode* node) {
    if (!node || node->isText) return false;

    if (sel.hasAttr && sel.attrName.rfind("__pseudo__", 0) == 0) return false;
    if (sel.hasAttr && sel.attrName == "*") return true;

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

    if (sel.hasAttr && sel.attrName != "*") {
        if (sel.attrName == "id") {
            if (node->id != sel.attrValue) return false;
        } else if (sel.attrName == "class") {
            if (node->className != sel.attrValue) return false;
        } else {
            return false;
        }
    }

    if (sel.hasNot) {
        if (sel.notInvalid) return false;
        if (!sel.notTag.empty() && node->tag == sel.notTag) return false;
        if (!sel.notId.empty()  && node->id  == sel.notId)  return false;
        if (!sel.notClass.empty()) {
            std::stringstream ss(node->className);
            std::string t;
            while (ss >> t) if (t == sel.notClass) return false;
        }
    }

    for (const auto& ps : sel.pseudos) {
        const RenderNode* p = node->parent;
        if (!p) return false;
        const auto& sibs = p->children;

        if (ps == "first-child" || ps == "only-child" ||
            ps == "first-of-type" || ps == "only-of-type") {
            const RenderNode* first = nullptr;
            for (auto& s : sibs) {
                if (s->isText) continue;
                if (ps == "first-of-type" || ps == "only-of-type") {
                    if (s->tag != node->tag) continue;
                }
                first = s.get();
                break;
            }
            if (first != node) return false;
        }

        if (ps == "last-child" || ps == "only-child" ||
            ps == "last-of-type" || ps == "only-of-type") {
            const RenderNode* last = nullptr;
            for (auto it = sibs.rbegin(); it != sibs.rend(); ++it) {
                if ((*it)->isText) continue;
                if (ps == "last-of-type" || ps == "only-of-type") {
                    if ((*it)->tag != node->tag) continue;
                }
                last = (*it).get();
                break;
            }
            if (last != node) return false;
        }

        if (ps == "only-child" || ps == "only-of-type") {
            int count = 0;
            for (auto& s : sibs) {
                if (s->isText) continue;
                if (ps == "only-of-type" && s->tag != node->tag) continue;
                count++;
                if (count > 1) return false;
            }
            if (count != 1) return false;
        }
    }

    return true;
}

void parseCssInto(const std::string& css,
                  std::vector<CssRule>& rules,
                  std::vector<FontFaceRule>* outFontFaces) {
    std::string s = css;
    size_t i = 0;

    while (i < s.size()) {
        size_t brace = s.find('{', i);
        if (brace == std::string::npos) break;

        std::string selectorGroup = trim(s.substr(i, brace - i));
        size_t closeBrace = s.find('}', brace);
        if (closeBrace == std::string::npos) break;

        if (!selectorGroup.empty() && selectorGroup[0] == '@') {
            int depth = 1;
            size_t j = brace + 1;
            size_t innerStart = j;
            while (j < s.size() && depth > 0) {
                if (s[j] == '{') depth++;
                else if (s[j] == '}') {
                    depth--;
                    if (depth == 0) break;
                }
                j++;
            }
            if (j >= s.size()) break;

            if (selectorGroup.rfind("@media", 0) == 0 ||
                selectorGroup.rfind("@supports", 0) == 0) {
                std::string inner = s.substr(innerStart, j - innerStart);
                parseCssInto(inner, rules, outFontFaces);
            }
            // iter3: @font-face 提取
            else if (selectorGroup.rfind("@font-face", 0) == 0 && outFontFaces) {
                std::string inner = s.substr(innerStart, j - innerStart);
                auto decls = parseDecls(inner);
                FontFaceRule ff;
                for (auto& d : decls) {
                    if (d.property == "font-family") ff.family = d.value;
                    else if (d.property == "src") ff.src = d.value;
                }
                if (!ff.family.empty() && !ff.src.empty()) {
                    outFontFaces->push_back(ff);
                }
            }

            i = j + 1;
            continue;
        }

        std::string declBody = s.substr(brace + 1, closeBrace - brace - 1);
        auto decls = parseDecls(declBody);
        if (decls.empty()) {
            i = closeBrace + 1;
            continue;
        }

        std::stringstream selStream(selectorGroup);
        std::string singleSel;
        while (std::getline(selStream, singleSel, ',')) {
            CssRule rule;
            parseSelectorChain(singleSel, rule.parts, rule.combinators);

            bool valid = !rule.parts.empty();
            for (auto& p : rule.parts) {
                if (p.tag.empty() && p.id.empty() && p.classes.empty() &&
                    !p.hasAttr && !p.hasNot && p.pseudos.empty()) {
                    valid = false;
                    break;
                }
            }
            if (!valid) continue;

            while (rule.combinators.size() + 1 < rule.parts.size()) {
                rule.combinators.push_back(Combinator::Descendant);
            }
            if (rule.combinators.size() + 1 > rule.parts.size()) {
                rule.combinators.resize(rule.parts.size() - 1);
            }

            rule.decls = decls;
            rules.push_back(rule);
        }

        i = closeBrace + 1;
    }
}

} // namespace

std::vector<CssRule> parseCss(const std::string& css,
                              std::vector<FontFaceRule>* outFontFaces) {
    std::vector<CssRule> rules;
    std::string s = stripComments(css);
    parseCssInto(s, rules, outFontFaces);
    return rules;
}

bool matchesSelector(const RenderNode* node, const CssRule& rule) {
    if (!node || rule.parts.empty()) return false;
    if (node->isText) return false;

    int i = (int)rule.parts.size() - 1;
    if (!matchSingle(rule.parts[i], node)) return false;

    const RenderNode* cur = node;
    while (i > 0) {
        Combinator comb = rule.combinators[i - 1];
        const RenderNode* prev = cur->parent;

        if (comb == Combinator::Child) {
            if (!prev || !matchSingle(rule.parts[i - 1], prev)) return false;
            cur = prev; i--;
        } else if (comb == Combinator::Descendant) {
            while (prev && !matchSingle(rule.parts[i - 1], prev)) prev = prev->parent;
            if (!prev) return false;
            cur = prev; i--;
        } else if (comb == Combinator::Adjacent) {
            if (!prev) return false;
            const RenderNode* found = nullptr;
            for (size_t k = 0; k < prev->children.size(); ++k) {
                if (prev->children[k].get() == cur) {
                    for (int m = (int)k - 1; m >= 0; --m) {
                        if (!prev->children[m]->isText) { found = prev->children[m].get(); break; }
                    }
                    break;
                }
            }
            if (!found || !matchSingle(rule.parts[i - 1], found)) return false;
            cur = found; i--;
        } else if (comb == Combinator::Sibling) {
            if (!prev) return false;
            const RenderNode* found = nullptr;
            for (size_t k = 0; k < prev->children.size(); ++k) {
                if (prev->children[k].get() == cur) {
                    for (int m = (int)k - 1; m >= 0; --m) {
                        if (!prev->children[m]->isText &&
                            matchSingle(rule.parts[i - 1], prev->children[m].get())) {
                            found = prev->children[m].get(); break;
                        }
                    }
                    break;
                }
            }
            if (!found) return false;
            cur = found; i--;
        } else {
            return false;
        }
    }
    return true;
}

} // namespace dm::render