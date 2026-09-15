#include "learn/snapshot_parser.h"
#include <fstream>
#include <sstream>
#include <cctype>
#include <cstdio>
#include <cmath>

namespace dm::learn {

// ============================================================
// 极简 JSON 读写 —— 只处理本项目需要的结构，不引第三方库
// 支持：对象、数组、字符串、数字、bool、null
// ============================================================

namespace {

struct JParser {
    const std::string& s;
    size_t i = 0;

    explicit JParser(const std::string& str) : s(str) {}

    void skipWs() {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t' ||
                                s[i] == '\n' || s[i] == '\r'))
            i++;
    }

    bool eof() { skipWs(); return i >= s.size(); }

    char peek() { skipWs(); return i < s.size() ? s[i] : '\0'; }

    bool expect(char c) {
        skipWs();
        if (i < s.size() && s[i] == c) { i++; return true; }
        return false;
    }

    std::string parseString() {
        skipWs();
        std::string out;
        if (i >= s.size() || s[i] != '"') return out;
        i++; // 跳过开头 "
        while (i < s.size() && s[i] != '"') {
            char c = s[i++];
            if (c == '\\' && i < s.size()) {
                char e = s[i++];
                switch (e) {
                    case 'n': out += '\n'; break;
                    case 't': out += '\t'; break;
                    case 'r': out += '\r'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case '"': out += '"';  break;
                    case '\\': out += '\\'; break;
                    case '/': out += '/';  break;
                    case 'u': {
                        if (i + 4 <= s.size()) {
                            unsigned int cp = 0;
                            for (int k = 0; k < 4; k++) {
                                char h = s[i + k];
                                cp <<= 4;
                                if (h >= '0' && h <= '9') cp |= (h - '0');
                                else if (h >= 'a' && h <= 'f') cp |= (h - 'a' + 10);
                                else if (h >= 'A' && h <= 'F') cp |= (h - 'A' + 10);
                            }
                            i += 4;
                            if (cp < 0x80) {
                                out += (char)cp;
                            } else if (cp < 0x800) {
                                out += (char)(0xC0 | (cp >> 6));
                                out += (char)(0x80 | (cp & 0x3F));
                            } else {
                                out += (char)(0xE0 | (cp >> 12));
                                out += (char)(0x80 | ((cp >> 6) & 0x3F));
                                out += (char)(0x80 | (cp & 0x3F));
                            }
                        }
                        break;
                    }
                    default: out += e; break;
                }
            } else {
                out += c;
            }
        }
        if (i < s.size() && s[i] == '"') i++;
        return out;
    }

    double parseNumber() {
        skipWs();
        size_t start = i;
        if (i < s.size() && (s[i] == '-' || s[i] == '+')) i++;
        while (i < s.size() &&
               (std::isdigit((unsigned char)s[i]) || s[i] == '.' ||
                s[i] == 'e' || s[i] == 'E' || s[i] == '-' || s[i] == '+'))
            i++;
        if (start == i) return 0;
        try { return std::stod(s.substr(start, i - start)); }
        catch (...) { return 0; }
    }

    bool parseBool() {
        skipWs();
        if (s.compare(i, 4, "true") == 0)  { i += 4; return true; }
        if (s.compare(i, 5, "false") == 0) { i += 5; return false; }
        return false;
    }

    void skipNull() {
        skipWs();
        if (s.compare(i, 4, "null") == 0) i += 4;
    }

    void skipValue() {
        skipWs();
        if (i >= s.size()) return;
        char c = s[i];
        if (c == '"') { parseString(); }
        else if (c == '{') {
            i++;
            while (i < s.size() && s[i] != '}') {
                if (s[i] == '"') { parseString(); }
                else if (s[i] == ':') i++;
                else if (s[i] == ',') i++;
                else skipValue();
            }
            if (i < s.size() && s[i] == '}') i++;
        }
        else if (c == '[') {
            i++;
            while (i < s.size() && s[i] != ']') {
                if (s[i] == ',') i++;
                else skipValue();
            }
            if (i < s.size() && s[i] == ']') i++;
        }
        else if (c == 't' || c == 'f') { parseBool(); }
        else if (c == 'n') { skipNull(); }
        else { parseNumber(); }
    }
};

std::string jsonEscape(const std::string& in) {
    std::string out;
    out.reserve(in.size() + 8);
    for (char c : in) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            default:
                if ((unsigned char)c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x",
                                  (unsigned char)c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

std::string getStr(JParser& p) { return p.parseString(); }

} // namespace

// ============================================================
// serializeSnapshot — 对象 -> JSON 字符串
// ============================================================
std::string serializeSnapshot(const PageSnapshot& s) {
    std::ostringstream o;
    o << "{\n";
    o << "  \"url\": \"" << jsonEscape(s.url) << "\",\n";
    o << "  \"title\": \"" << jsonEscape(s.title) << "\",\n";
    o << "  \"viewportWidth\": " << s.viewportWidth << ",\n";
    o << "  \"viewportHeight\": " << s.viewportHeight << ",\n";
    o << "  \"totalNodes\": " << s.totalNodes << ",\n";
    o << "  \"maxDepth\": " << s.maxDepth << ",\n";
    o << "  \"imageCount\": " << s.imageCount << ",\n";
    o << "  \"scriptCount\": " << s.scriptCount << ",\n";
    o << "  \"styleSheetCount\": " << s.styleSheetCount << ",\n";
    o << "  \"nodes\": [\n";

    for (size_t k = 0; k < s.nodes.size(); k++) {
        const auto& n = s.nodes[k];
        o << "    {\n";
        o << "      \"path\": \"" << jsonEscape(n.path) << "\",\n";
        o << "      \"tag\": \"" << jsonEscape(n.tag) << "\",\n";
        o << "      \"id\": \"" << jsonEscape(n.id) << "\",\n";
        o << "      \"className\": \"" << jsonEscape(n.className) << "\",\n";
        o << "      \"depth\": " << n.depth << ",\n";
        o << "      \"childCount\": " << n.childCount << ",\n";
        o << "      \"textPreview\": \"" << jsonEscape(n.textPreview) << "\",\n";
        o << "      \"alt\": \"" << jsonEscape(n.alt) << "\",\n";
        o << "      \"ariaLabel\": \"" << jsonEscape(n.ariaLabel) << "\",\n";
        o << "      \"role\": \"" << jsonEscape(n.role) << "\",\n";

        o << "      \"style\": {\n";
        o << "        \"color\": \"" << jsonEscape(n.style.color) << "\",\n";
        o << "        \"backgroundColor\": \"" << jsonEscape(n.style.backgroundColor) << "\",\n";
        o << "        \"fontSize\": \"" << jsonEscape(n.style.fontSize) << "\",\n";
        o << "        \"fontWeight\": \"" << jsonEscape(n.style.fontWeight) << "\",\n";
        o << "        \"display\": \"" << jsonEscape(n.style.display) << "\",\n";
        o << "        \"position\": \"" << jsonEscape(n.style.position) << "\",\n";
        o << "        \"textAlign\": \"" << jsonEscape(n.style.textAlign) << "\",\n";
        o << "        \"marginTop\": " << n.style.marginTop << ",\n";
        o << "        \"marginBottom\": " << n.style.marginBottom << ",\n";
        o << "        \"marginLeft\": " << n.style.marginLeft << ",\n";
        o << "        \"marginRight\": " << n.style.marginRight << ",\n";
        o << "        \"paddingTop\": " << n.style.paddingTop << ",\n";
        o << "        \"paddingBottom\": " << n.style.paddingBottom << ",\n";
        o << "        \"paddingLeft\": " << n.style.paddingLeft << ",\n";
        o << "        \"paddingRight\": " << n.style.paddingRight << ",\n";
        o << "        \"borderWidth\": " << n.style.borderWidth << ",\n";
        o << "        \"borderStyle\": \"" << jsonEscape(n.style.borderStyle) << "\",\n";
        o << "        \"opacity\": \"" << jsonEscape(n.style.opacity) << "\",\n";
        o << "        \"fontStyle\": \"" << jsonEscape(n.style.fontStyle) << "\",\n";
        o << "        \"listStyleType\": \"" << jsonEscape(n.style.listStyleType) << "\",\n";
        o << "        \"backgroundImage\": \"" << jsonEscape(n.style.backgroundImage) << "\",\n";
        o << "        \"intrinsicW\": " << n.style.intrinsicW << ",\n";
        o << "        \"intrinsicH\": " << n.style.intrinsicH << ",\n";
        o << "        \"filter\": \"" << jsonEscape(n.style.filter) << "\"\n";
        o << "      },\n";

        o << "      \"layout\": {\n";
        o << "        \"x\": " << n.layout.x << ",\n";
        o << "        \"y\": " << n.layout.y << ",\n";
        o << "        \"w\": " << n.layout.w << ",\n";
        o << "        \"h\": " << n.layout.h << "\n";
        o << "      }\n";
        o << "    }";
        if (k + 1 < s.nodes.size()) o << ",";
        o << "\n";
    }
    o << "  ]\n";
    o << "}\n";
    return o.str();
}

// ============================================================
// parseSnapshotJson — JSON 字符串 -> 对象
// 已处理 UTF-8 BOM
// ============================================================
PageSnapshot parseSnapshotJson(const std::string& json) {
    PageSnapshot s;

    // 跳过 UTF-8 BOM（PowerShell Set-Content -Encoding UTF8 会写入）
    std::string cleaned = json;
    if (cleaned.size() >= 3 &&
        (unsigned char)cleaned[0] == 0xEF &&
        (unsigned char)cleaned[1] == 0xBB &&
        (unsigned char)cleaned[2] == 0xBF) {
        cleaned.erase(0, 3);
    }

    JParser p(cleaned);

    if (!p.expect('{')) return s;

    while (!p.eof() && p.peek() != '}') {
        p.skipWs();
        if (p.peek() == ',') { p.i++; continue; }
        if (p.peek() != '"') break;

        std::string key = p.parseString();
        p.expect(':');

        if (key == "url")              s.url = getStr(p);
        else if (key == "title")       s.title = getStr(p);
        else if (key == "viewportWidth")   s.viewportWidth = (int)p.parseNumber();
        else if (key == "viewportHeight")  s.viewportHeight = (int)p.parseNumber();
        else if (key == "totalNodes")      s.totalNodes = (int)p.parseNumber();
        else if (key == "maxDepth")        s.maxDepth = (int)p.parseNumber();
        else if (key == "imageCount")      s.imageCount = (int)p.parseNumber();
        else if (key == "scriptCount")     s.scriptCount = (int)p.parseNumber();
        else if (key == "styleSheetCount") s.styleSheetCount = (int)p.parseNumber();
        else if (key == "nodes") {
            p.expect('[');
            while (!p.eof() && p.peek() != ']') {
                if (!p.expect('{')) break;
                NodeSnapshot n;
                while (!p.eof() && p.peek() != '}') {
                    p.skipWs();
                    if (p.peek() == ',') { p.i++; continue; }
                    if (p.peek() != '"') break;
                    std::string nk = p.parseString();
                    p.expect(':');

                    if (nk == "path")       n.path = getStr(p);
                    else if (nk == "tag")   n.tag = getStr(p);
                    else if (nk == "id")    n.id = getStr(p);
                    else if (nk == "className") n.className = getStr(p);
                    else if (nk == "depth")      n.depth = (int)p.parseNumber();
                    else if (nk == "childCount") n.childCount = (int)p.parseNumber();
                    else if (nk == "textPreview") n.textPreview = getStr(p);
                    else if (nk == "alt")       n.alt = getStr(p);
                    else if (nk == "ariaLabel") n.ariaLabel = getStr(p);
                    else if (nk == "role")      n.role = getStr(p);
                    else if (nk == "style") {
                        p.expect('{');
                        while (!p.eof() && p.peek() != '}') {
                            p.skipWs();
                            if (p.peek() == ',') { p.i++; continue; }
                            if (p.peek() != '"') break;
                            std::string sk = p.parseString();
                            p.expect(':');
                            if (sk == "color")              n.style.color = getStr(p);
                            else if (sk == "backgroundColor") n.style.backgroundColor = getStr(p);
                            else if (sk == "fontSize")      n.style.fontSize = getStr(p);
                            else if (sk == "fontWeight")    n.style.fontWeight = getStr(p);
                            else if (sk == "display")       n.style.display = getStr(p);
                            else if (sk == "position")      n.style.position = getStr(p);
                            else if (sk == "textAlign")     n.style.textAlign = getStr(p);
                            else if (sk == "marginTop")     n.style.marginTop = (int)p.parseNumber();
                            else if (sk == "marginBottom")  n.style.marginBottom = (int)p.parseNumber();
                            else if (sk == "marginLeft")    n.style.marginLeft = (int)p.parseNumber();
                            else if (sk == "marginRight")   n.style.marginRight = (int)p.parseNumber();
                            else if (sk == "paddingTop")    n.style.paddingTop = (int)p.parseNumber();
                            else if (sk == "paddingBottom") n.style.paddingBottom = (int)p.parseNumber();
                            else if (sk == "paddingLeft")   n.style.paddingLeft = (int)p.parseNumber();
                            else if (sk == "paddingRight")  n.style.paddingRight = (int)p.parseNumber();
                            else if (sk == "borderWidth")   n.style.borderWidth = (int)p.parseNumber();
                            else if (sk == "borderStyle")   n.style.borderStyle = getStr(p);
                            else if (sk == "opacity")       n.style.opacity = getStr(p);
                            else if (sk == "fontStyle")     n.style.fontStyle = getStr(p);
                            else if (sk == "listStyleType") n.style.listStyleType = getStr(p);
                            else if (sk == "backgroundImage") n.style.backgroundImage = getStr(p);
                            else if (sk == "intrinsicW")    n.style.intrinsicW = (int)p.parseNumber();
                            else if (sk == "intrinsicH")    n.style.intrinsicH = (int)p.parseNumber();
                            else if (sk == "filter")        n.style.filter = getStr(p);
                            else p.skipValue();
                        }
                        p.expect('}');
                    }
                    else if (nk == "layout") {
                        p.expect('{');
                        while (!p.eof() && p.peek() != '}') {
                            p.skipWs();
                            if (p.peek() == ',') { p.i++; continue; }
                            if (p.peek() != '"') break;
                            std::string lk = p.parseString();
                            p.expect(':');
                            if (lk == "x")      n.layout.x = (float)p.parseNumber();
                            else if (lk == "y") n.layout.y = (float)p.parseNumber();
                            else if (lk == "w") n.layout.w = (float)p.parseNumber();
                            else if (lk == "h") n.layout.h = (float)p.parseNumber();
                            else p.skipValue();
                        }
                        p.expect('}');
                    }
                    else p.skipValue();
                }
                p.expect('}');
                s.nodes.push_back(n);

                p.skipWs();
                if (p.peek() == ',') { p.i++; continue; }
                if (p.peek() == ']') break;
            }
            p.expect(']');
        }
        else p.skipValue();

        p.skipWs();
        if (p.peek() == ',') { p.i++; continue; }
        if (p.peek() == '}') break;
    }
    return s;
}

// ============================================================
// parseSnapshotFile — 读文件 -> 对象
// ============================================================
PageSnapshot parseSnapshotFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return parseSnapshotJson(ss.str());
}

} // namespace dm::learn