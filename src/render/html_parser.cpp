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

void parseStartTag(const std::string& raw,
                   std::string& tag,
                   std::string& id,
                   std::string& cls) {
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

        if (name == "id") id = value;
        else if (name == "class") cls = value;
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
        std::string t = trim(textBuf);
        textBuf.clear();
        if (t.empty()) return;
        if (!cur->text.empty()) cur->text += " ";
        cur->text += t;
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
                size_t end = html.find('>', i);
                if (end == std::string::npos) break;
                i = end + 1;
                continue;
            }

            size_t end = html.find('>', i);
            if (end == std::string::npos) break;

            std::string inner = html.substr(i + 1, end - i - 1);
            i = end + 1;

            if (!inner.empty() && inner[0] == '/') {
                flushText();
                std::string closing = lower(trim(inner.substr(1)));
                if (cur->tag == closing && cur->parent) {
                    cur = cur->parent;
                }
                continue;
            }

            flushText();
            std::string tag, id, cls;
            parseStartTag(inner, tag, id, cls);
            if (tag.empty()) continue;

            if (isSkipTag(tag)) {
                std::string closeTag = "</" + tag;
                size_t skipEnd = html.find(closeTag, i);
                if (skipEnd == std::string::npos) {
                    i = html.size();
                } else {
                    size_t gt = html.find('>', skipEnd);
                    i = (gt == std::string::npos) ? html.size() : gt + 1;
                }
                continue;
            }

            auto node = std::make_unique<RenderNode>();
            node->tag = tag;
            node->id = id;
            node->className = cls;

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