#include "render/html_parser.h"
#include "render/css_parser.h"
#include "render/style_resolver.h"
#include "render/layout_engine.h"
#include "render/snapshot_dumper.h"
#include "render/render_window.h"
#include "render/text_measure.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <windows.h>

using namespace dm::render;

static std::string shortStyle(const ComputedStyle& s) {
    std::ostringstream o;
    bool first = true;
    auto add = [&](const std::string& k, const std::string& v) {
        if (v.empty()) return;
        if (!first) o << " ";
        first = false;
        o << k << ":" << v;
    };
    add("display", s.display);
    add("color", s.color);
    add("font-size", s.fontSize);
    return o.str();
}

static void printTree(const RenderNode* node, int indent,
                      bool showStyle, bool showLayout) {
    for (int i = 0; i < indent; ++i) std::cout << "  ";

    if (node->isText) {
        std::string t = node->text;
        if (t.size() > 30) t = t.substr(0, 30) + "...";
        std::cout << "\"" << t << "\"";
        if (showLayout) {
            char buf[80];
            std::snprintf(buf, sizeof(buf), "  (%d,%d %dx%d)",
                          (int)node->layout.x, (int)node->layout.y,
                          (int)node->layout.w, (int)node->layout.h);
            std::cout << buf;
        }
        std::cout << "\n";
        return;
    }

    std::cout << node->tag;
    if (!node->id.empty()) std::cout << "#" << node->id;
    if (!node->className.empty()) std::cout << "." << node->className;
    if (!node->text.empty()) {
        std::string t = node->text;
        if (t.size() > 25) t = t.substr(0, 25) + "...";
        std::cout << "  \"" << t << "\"";
    }
    if (showLayout) {
        char buf[80];
        std::snprintf(buf, sizeof(buf), "  (%d,%d %dx%d)",
                      (int)node->layout.x, (int)node->layout.y,
                      (int)node->layout.w, (int)node->layout.h);
        std::cout << buf;
    }
    if (showStyle) {
        std::string st = shortStyle(node->style);
        if (!st.empty()) std::cout << "  [" << st << "]";
    }
    std::cout << "\n";

    for (auto& c : node->children)
        printTree(c.get(), indent + 1, showStyle, showLayout);
}

static int countNodes(const RenderNode* node) {
    int n = node->isText ? 0 : 1;
    for (auto& c : node->children) n += countNodes(c.get());
    return n;
}

static int maxDepth(const RenderNode* node) {
    int d = node->isText ? 0 : node->depth;
    for (auto& c : node->children) {
        int cd = maxDepth(c.get());
        if (cd > d) d = cd;
    }
    return d;
}

int main(int argc, char** argv) {
    SetConsoleOutputCP(CP_UTF8);

    if (argc < 2) {
        std::cerr << "用法: dm_render_test <html_file> [viewport_w] [viewport_h] "
                     "[--dump|--window|--gdi]\n";
        return 1;
    }

    bool dumpMode = false;
    bool windowMode = false;
    bool useGdi = false;            // 默认关闭 GDI 度量
    std::string htmlPath;
    int vw = 1024, vh = 768;
    int posArg = 0;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--dump")   { dumpMode = true; continue; }
        if (a == "--window") { windowMode = true; continue; }
        if (a == "--gdi")    { useGdi = true; continue; }
        if (posArg == 0) { htmlPath = a; posArg = 1; }
        else if (posArg == 1) { vw = std::atoi(a.c_str()); posArg = 2; }
        else if (posArg == 2) { vh = std::atoi(a.c_str()); posArg = 3; }
    }
    if (htmlPath.empty()) {
        std::cerr << "缺少 html 文件参数\n";
        return 1;
    }

    // 只有显式 --gdi 时才启用 GDI 度量
    // 因为 GDI 的字体渲染跟 Chromium 的 DirectWrite 有偏差
    if (useGdi) {
        initTextMeasurer();
        std::cout << "[text] 使用 GDI 度量\n";
    } else {
        std::cout << "[text] 使用估算（跟 Chromium 对齐）\n";
    }

    std::ifstream f(htmlPath, std::ios::binary);
    if (!f) {
        std::cerr << "无法打开: " << htmlPath << "\n";
        return 1;
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    std::string html = ss.str();

    auto styleBlocks = extractStyleBlocks(html);
    std::string allCss;
    for (auto& b : styleBlocks) allCss += b + "\n";

    std::string cssPath = htmlPath;
    auto dot = cssPath.find_last_of('.');
    if (dot != std::string::npos) {
        cssPath = cssPath.substr(0, dot) + ".css";
        std::ifstream cf(cssPath);
        if (cf) {
            std::ostringstream cssSs;
            cssSs << cf.rdbuf();
            allCss += cssSs.str();
        }
    }

    auto rules = parseCss(allCss);
    auto root = parseHtml(html);
    if (!root) { std::cerr << "解析失败\n"; return 1; }

    resolveStyles(root.get(), rules);
    layoutTree(root.get(), vw, vh);

    if (dumpMode) {
        std::string json = dumpSnapshotJson(root.get(),
                                             "file://" + htmlPath,
                                             htmlPath,
                                             vw, vh);
        std::string outPath = htmlPath + ".snapshot.json";
        std::ofstream out(outPath, std::ios::binary);
        out << json;
        std::cout << "已生成 " << outPath
                  << " (" << json.size() << " 字节)\n";
        return 0;
    }

    if (windowMode) {
        RenderWindow win;
        if (!win.create(L"DM Render Test", vw, vh)) {
            std::cerr << "窗口创建失败\n";
            return 1;
        }
        win.setRoot(root.get());
        return win.run();
    }

    std::cout << "解析 " << htmlPath << " (" << html.size() << " 字节) "
              << "视口 " << vw << "x" << vh << "...\n\n";
    std::cout << "样式规则: " << rules.size() << " 条\n\n";

    printTree(root.get(), 0, true, true);

    std::cout << "\n节点总数: " << countNodes(root.get()) - 1 << "\n";
    std::cout << "最大深度: " << maxDepth(root.get()) << "\n";

    return 0;
}