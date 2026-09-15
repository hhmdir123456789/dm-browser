#include "render/html_parser.h"
#include <cctype>

namespace dm::render {

namespace {

bool isVoidTag(const std::string& tag) {
    static const char* kVoid[] = {
        "area","base","br","col","embed","hr","img","input",
        "link","meta","param","source","track","wbr", nullptr
    };
    for (int i = 0; kVoid[i]; ++i) {
        if (tag == kVoid[i]) return true;
    }
    return false;
}

bool isSkipTag(const std::string& tag) {
    return tag == "script" || tag == "style";
}

bool isSpace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && isSpace(s[a])) a++;
    while (b > a && isSpace(s[b - 1])) b--;
    return s.substr(a, b - a);
}

std::string lower(const std::string& s) {
    std::string out = s;
    for (char& c : out) c = (char)std::tolower((unsigned char)c);
    return out;
}

std::string decodeEntities(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    size_t i = 0;
    while (i < s.size()) {
        if (s[i] == '&') {
            size_t semi = s.find(';', i);
            if (semi != std::string::npos && semi - i <= 10) {
                std::string ent = s.substr(i + 1, semi - i - 1);
                if (ent == "nbsp") out += ' ';
                else if (ent == "amp") out += '&';
                else if (ent == "lt") out += '<';
                else if (ent == "gt") out += '>';
                else if (ent == "quot") out += '"';
                else if (ent == "apos") out += '\'';
                else if (ent == "mdash" || ent == "ndash") out += '-';
                else if (ent == "hellip") out += '.';
                else if (ent == "copy" || ent == "reg") out += 'C';
                else if (!ent.empty() && ent[0] == '#') {
                    int code = 0;
                    if (ent.size() > 1 && (ent[1] == 'x' || ent[1] == 'X')) {
                        try { code = std::stoi(ent.substr(2), nullptr, 16); } catch (...) {}
                    } else {
                        try { code = std::stoi(ent.substr(1)); } catch (...) {}
                    }
                    if (code > 0 && code < 0x80) {
                        out += (char)code;
                    } else if (code >= 0x80 && code <= 0x7FF) {
                        out += (char)(0xC0 | (code >> 6));
                        out += (char)(0x80 | (code & 0x3F));
                    } else if (code >= 0x800 && code <= 0xFFFF) {
                        out += (char)(0xE0 | (code >> 12));
                        out += (char)(0x80 | ((code >> 6) & 0x3F));
                        out += (char)(0x80 | (code & 0x3F));
                    }
                } else {
                    out += s.substr(i, semi - i + 1);
                }
                i = semi + 1;
                continue;
            }
        }
        out += s[i++];
    }
    return out;
}

size_t findTagEnd(const std::string& s, size_t start) {
    char quote = 0;
    size_t i = start;
    while (i < s.size()) {
        char c = s[i];
        if (quote) {
            if (c == quote) quote = 0;
        } else if (c == '"' || c == '\'') {
            quote = c;
        } else if (c == '>') {
            return i;
        }
        i++;
    }
    return std::string::npos;
}

// iter3: 解析所有属性
void parseStartTag(const std::string& raw,
                   std::string& tag,
                   std::map<std::string, std::string>& attrs) {
    size_t i = 0;
    while (i < raw.size() && !isSpace(raw[i]) && raw[i] != '/') i++;
    tag = lower(raw.substr(0, i));

    while (i < raw.size()) {
        while (i < raw.size() && isSpace(raw[i])) i++;
        if (i >= raw.size() || raw[i] == '/') break;

        size_t nameStart = i;
        while (i < raw.size() && raw[i] != '=' && !isSpace(raw[i])) i++;
        std::string name = lower(raw.substr(nameStart, i - nameStart));

        std::string value;
        if (i < raw.size() && raw[i] == '=') {
            i++;
            char quote = 0;
            if (i < raw.size() && (raw[i] == '"' || raw[i] == '\'')) {
                quote = raw[i++];
            }
            size_t valStart = i;
            if (quote) {
                while (i < raw.size() && raw[i] != quote) i++;
            } else {
                while (i < raw.size() && !isSpace(raw[i])) i++;
            }
            value = raw.substr(valStart, i - valStart);
            if (quote && i < raw.size()) i++;
        }

        if (!name.empty()) attrs[name] = value;
    }
}

} // namespace

std::unique_ptr<RenderNode> parseHtml(const std::string& html) {
    auto root = std::make_unique<RenderNode>();
    root->tag = "#document";
    root->depth = 0;

    RenderNode* cur = root.get();
    size_t i = 0;
    std::string textBuf;

    auto flushText = [&]() {
        std::string t = textBuf;
        textBuf.clear();
        if (t.empty()) return;

        t = decodeEntities(t);

        std::string collapsed;
        bool lastWasSpace = false;
        bool allSpace = true;
        for (char c : t) {
            if (isSpace(c)) {
                if (!lastWasSpace) collapsed += ' ';
                lastWasSpace = true;
            } else {
                collapsed += c;
                lastWasSpace = false;
                allSpace = false;
            }
        }
        if (allSpace) return;

        auto tn = std::make_unique<RenderNode>();
        tn->tag = "#text";
        tn->isText = true;
        tn->text = collapsed;
        cur->appendChild(std::move(tn));
    };

    while (i < html.size()) {
        char c = html[i];

        if (c == '<') {
            if (html.compare(i, 4, "<!--") == 0) {
                size_t end = html.find("-->", i + 4);
                if (end == std::string::npos) break;
                i = end + 3;
                continue;
            }
            if (html.compare(i, 2, "<!") == 0) {
                size_t end = findTagEnd(html, i + 2);
                if (end == std::string::npos) break;
                i = end + 1;
                continue;
            }

            size_t end = findTagEnd(html, i + 1);
            if (end == std::string::npos) break;

            std::string inner = html.substr(i + 1, end - i - 1);
            i = end + 1;

            if (!inner.empty() && inner[0] == '/') {
                flushText();
                std::string closing = lower(trim(inner.substr(1)));
                RenderNode* p = cur;
                while (p && p->tag != closing && p->parent) {
                    p = p->parent;
                }
                if (p && p->tag == closing && p->parent) {
                    cur = p->parent;
                }
                continue;
            }

            flushText();
            std::string tag;
            std::map<std::string, std::string> attrs;
            parseStartTag(inner, tag, attrs);
            if (tag.empty()) continue;

            if (isSkipTag(tag)) {
                std::string closeTag = "</" + tag;
                size_t skipEnd = html.find(closeTag, i);
                if (skipEnd == std::string::npos) {
                    i = html.size();
                } else {
                    size_t gt = findTagEnd(html, skipEnd + closeTag.size());
                    i = (gt == std::string::npos) ? html.size() : gt + 1;
                }
                continue;
            }

            auto node = std::make_unique<RenderNode>();
            node->tag = tag;
            node->attrs = attrs;
            auto idIt = attrs.find("id");
            if (idIt != attrs.end()) node->id = idIt->second;
            auto clsIt = attrs.find("class");
            if (clsIt != attrs.end()) node->className = clsIt->second;

            bool selfClosing = (!inner.empty() && inner.back() == '/') ||
                                isVoidTag(tag);

            RenderNode* added = cur->appendChild(std::move(node));
            if (!selfClosing) {
                cur = added;
            }
        } else {
            textBuf += c;
            i++;
        }
    }

    flushText();
    return root;
}

} // namespace dm::render