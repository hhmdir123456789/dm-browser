#include <windows.h>
#include "learn/learn_store.h"
#include "learn/snapshot_parser.h"
#include "learn/multi_compare.h"
#include "db/Database.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <set>
#include <map>
#include <algorithm>
#include <ctime>
#include <cstdio>

using namespace dm::learn;

static std::string readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static void printUsage() {
    std::cout << "dm_learn - 学习库工具\n\n";
    std::cout << "用法:\n";
    std::cout << "  dm_learn                                 显示统计\n";
    std::cout << "  dm_learn list                            列出样本\n";
    std::cout << "  dm_learn show <id>                       查看样本详情\n";
    std::cout << "  dm_learn add <url> <html_file> <css_file>  添加样本\n";
    std::cout << "  dm_learn status <id> <status>            更新状态\n";
    std::cout << "  dm_learn features                        特性统计\n";
    std::cout << "  dm_learn diffs                           最近差异\n";
    std::cout << "  dm_learn seed                            插入演示数据\n";
    std::cout << "  dm_learn dump                            生成示例快照\n";
    std::cout << "  dm_learn compare <ref.json> <dm.json>    多维对比\n";
    std::cout << "  dm_learn infer <ref.json> <dm.json>      推断缺失特性\n";
    std::cout << "  dm_learn refs                            列出参考快照\n";
    std::cout << "  dm_learn inspect <url>                   查看 URL 的参考详情\n";
    std::cout << "  dm_learn export <url> <file.json>        导出参考快照\n";
    std::cout << "  dm_learn report <ref.json> <dm.json> <out.md>  生成对比报告\n";
    std::cout << "\n状态值: pending / analyzed / fixed / ignored\n";
}

static void printStats(LearnStore& store) {
    std::cout << "=== 学习库统计 ===\n";
    std::cout << "样本总数: " << store.sampleCount() << "\n";
    std::cout << "差异记录: " << store.diffCount() << "\n";
    std::cout << "已修复:   " << store.fixedCount() << "\n\n";

    std::cout << "最近样本：\n";
    auto samples = store.listSamples(10);
    if (samples.empty()) {
        std::cout << "  (空，先运行 dm_learn seed)\n";
    } else {
        for (auto& s : samples) {
            std::cout << "  #" << s.id << " " << s.url
                      << " (" << s.htmlSize << " 字节, "
                      << "差异 " << (int)(s.diffScore * 100) << "%, "
                      << s.status << ")\n";
        }
    }
}

static void seedDemo(LearnStore& store) {
    std::cout << "插入演示数据...\n";

    struct Demo {
        const char* url;
        const char* title;
        const char* html;
        const char* css;
        double diff;
        const char* status;
    };
    Demo demos[] = {
        {"https://example.com/", "Example Domain",
         "<h1>Example Domain</h1><p>This domain is for use in examples.</p>",
         "h1 { color: #333; }", 0.02, "fixed"},
        {"https://www.qq.com/", "腾讯网",
         "<div class='nav'><a>新闻</a><a>视频</a></div><h1>腾讯</h1>",
         ".nav { display: flex; }", 0.35, "analyzed"},
        {"https://www.sohu.com/", "搜狐",
         "<nav style='display:flex'><a>首页</a><a>新闻</a></nav>",
         "nav { display: flex; gap: 20px; }", 0.42, "pending"},
        {"https://github.com/", "GitHub",
         "<header><nav></nav></header><main><div class='repo'></div></main>",
         "header { position: fixed; } .repo { border-radius: 8px; }",
         0.55, "analyzed"},
    };

    for (auto& d : demos) {
        SiteSample s;
        s.url = d.url;
        s.title = d.title;
        s.html = d.html;
        s.css = d.css;
        s.htmlSize = (int)strlen(d.html);
        int64_t id = store.addSample(s);
        store.updateSampleScreenshots(id, "", "", d.diff);
        store.updateSampleStatus(id, d.status);

        if (std::string(d.status) == "analyzed") {
            DiffRecord dr;
            dr.sampleId = id;
            dr.category = "layout";
            dr.severity = 4;
            dr.feature = "display:flex";
            dr.description = "flex 布局未实现，子元素垂直堆叠";
            dr.aiAnalysis = "在 layoutBlock 里支持 display:flex";
            store.addDiff(dr);

            store.recordFeature("display:flex", false);
            store.recordFeature("display:flex", false);
            store.recordFeature("display:block", true);
        }
        std::cout << "  已添加: " << d.url << " (id=" << id << ")\n";
    }
    store.recalcPriorities();
    std::cout << "\n完成。\n";
}

static void dumpDemoSnapshot() {
    PageSnapshot s;
    s.url = "https://demo.local/";
    s.title = "Demo";
    s.viewportWidth = 800;
    s.viewportHeight = 600;

    NodeSnapshot n1;
    n1.path = "html>body>div.0";
    n1.tag = "div";
    n1.className = "container";
    n1.style.display = "flex";
    n1.style.color = "0,0,0";
    n1.style.fontSize = "16px";
    n1.style.backgroundColor = "255,255,255";
    n1.layout = {0, 0, 800, 100};
    s.nodes.push_back(n1);

    NodeSnapshot n2;
    n2.path = "html>body>div.0>p.0";
    n2.tag = "p";
    n2.textPreview = "Hello";
    n2.style.display = "block";
    n2.style.color = "0,0,0";
    n2.style.fontSize = "16px";
    n2.layout = {10, 10, 100, 20};
    s.nodes.push_back(n2);

    std::ofstream out("snapshot_ref.json");
    out << serializeSnapshot(s);
    std::cout << "已生成 snapshot_ref.json ("
              << s.nodes.size() << " 个节点)\n";
}

// ============================================================
// 批 C：refs / inspect
// ============================================================
static std::string formatTime(int64_t ms) {
    if (ms == 0) return "(未知)";
    ULONGLONG now = GetTickCount64();
    if (now < (ULONGLONG)ms) return "(刚刚)";
    ULONGLONG diffSec = (now - (ULONGLONG)ms) / 1000;
    if (diffSec < 60) return "刚刚";
    if (diffSec < 3600)
        return std::to_string(diffSec / 60) + " 分钟前";
    if (diffSec < 86400)
        return std::to_string(diffSec / 3600) + " 小时前";
    return std::to_string(diffSec / 86400) + " 天前";
}

static void cmdRefs(LearnStore& store) {
    auto refs = store.listRefs(100);
    if (refs.empty()) {
        std::cout << "（无参考快照）\n";
        std::cout << "先在 dm_ui.exe 打开一个网页，侧边栏「分析」点「分析本页」\n";
        return;
    }
    std::cout << "参考快照列表（共 " << refs.size() << " 条）：\n\n";
    for (auto& r : refs) {
        auto snap = parseSnapshotJson(r.snapshotJson);
        std::cout << "  #" << r.id << "  " << r.url << "\n";
        std::cout << "        " << snap.nodes.size() << " 节点"
                  << "  · 深度 " << snap.maxDepth
                  << "  · 保存于 " << formatTime(r.capturedAt) << "\n";
    }
}

static void cmdInspect(LearnStore& store, const std::string& url) {
    auto ref = store.getRef(url);
    if (ref.id == 0) {
        std::cout << "URL 无参考快照: " << url << "\n";
        std::cout << "提示：在 dm_ui.exe 打开该页面，点侧边栏「分析本页」\n";
        return;
    }

    auto snap = parseSnapshotJson(ref.snapshotJson);
    std::cout << "URL:      " << ref.url << "\n";
    std::cout << "标题:     " << ref.title << "\n";
    std::cout << "保存于:   " << formatTime(ref.capturedAt) << "\n";
    std::cout << "节点数:   " << snap.nodes.size() << "\n";
    std::cout << "最大深度: " << snap.maxDepth << "\n";
    std::cout << "视口:     " << snap.viewportWidth
              << " x " << snap.viewportHeight << "\n";
    std::cout << "图片:     " << snap.imageCount << "\n";
    std::cout << "脚本:     " << snap.scriptCount << "\n";
    std::cout << "样式表:   " << snap.styleSheetCount << "\n\n";

    if (snap.nodes.empty()) {
        std::cout << "（快照无节点）\n";
        return;
    }

    std::cout << "前 10 个节点：\n";
    int n = 0;
    for (auto& node : snap.nodes) {
        if (n++ >= 10) break;
        std::cout << "  " << node.path << "  <" << node.tag << ">";
        if (!node.className.empty()) std::cout << " ." << node.className;
        std::cout << "  (" << (int)node.layout.w << "x"
                  << (int)node.layout.h << ")";
        if (node.style.display != "block" && !node.style.display.empty())
            std::cout << "  display:" << node.style.display;
        std::cout << "\n";
    }
    if (snap.nodes.size() > 10) {
        std::cout << "  ... 还有 " << (snap.nodes.size() - 10) << " 个\n";
    }
}

// ============================================================
// 批 F-1：export
// ============================================================
static void cmdExport(LearnStore& store,
                      const std::string& url,
                      const std::string& outPath) {
    auto ref = store.getRef(url);
    if (ref.id == 0) {
        std::cout << "URL 无参考快照: " << url << "\n";
        std::cout << "提示：先在 dm_ui.exe 打开该页面，点「分析本页」\n";
        return;
    }

    std::ofstream out(outPath, std::ios::binary);
    if (!out) {
        std::cerr << "无法写入: " << outPath << "\n";
        return;
    }
    out << ref.snapshotJson;
    out.close();

    auto snap = parseSnapshotJson(ref.snapshotJson);
    std::cout << "已导出 " << ref.snapshotJson.size() << " 字节"
              << "（" << snap.nodes.size() << " 节点）到 "
              << outPath << "\n";
}

// ============================================================
// 批 F-4：report
// ============================================================
static std::string escMd(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '|') out += "\\|";
        else if (c == '\n') out += ' ';
        else if (c == '\r') continue;
        else if (c == '`') out += '\'';
        else out += c;
    }
    return out;
}

struct PropAgg {
    int count = 0;
    int sevSum = 0;
};

static void cmdReport(const std::string& refPath,
                      const std::string& dmPath,
                      const std::string& outPath) {
    auto ref = parseSnapshotFile(refPath);
    auto dm  = parseSnapshotFile(dmPath);
    if (ref.nodes.empty() || dm.nodes.empty()) {
        std::cerr << "无法读取快照文件\n";
        return;
    }

    auto result = MultiCompare::compare(ref, dm);

    std::map<std::string, PropAgg> byProp;
    std::map<std::string, int> byPath;
    for (auto& d : result.diffs) {
        auto& p = byProp[d.property];
        p.count++;
        p.sevSum += d.severity;
        byPath[d.path]++;
    }

    std::vector<std::pair<std::string, PropAgg>> propList(
        byProp.begin(), byProp.end());
    std::sort(propList.begin(), propList.end(),
        [](const auto& a, const auto& b) {
            if (a.second.count != b.second.count)
                return a.second.count > b.second.count;
            return a.first < b.first;
        });

    std::vector<std::pair<std::string, int>> pathList(
        byPath.begin(), byPath.end());
    std::sort(pathList.begin(), pathList.end(),
        [](const auto& a, const auto& b) {
            if (a.second != b.second) return a.second > b.second;
            return a.first < b.first;
        });

    std::ofstream out(outPath, std::ios::binary);
    if (!out) {
        std::cerr << "无法写入: " << outPath << "\n";
        return;
    }

    // UTF-8 BOM，让 Windows 记事本 / PowerShell 正确识别编码
    out << "\xEF\xBB\xBF";

    auto pct = [](double v) {
        int p = (int)(v * 100 + 0.5);
        if (p < 0) p = 0;
        if (p > 100) p = 100;
        return p;
    };

    out << "# 跨引擎对比报告\n\n";
    out << "- 参考：" << escMd(refPath) << "\n";
    out << "- 当前：" << escMd(dmPath) << "\n";
    out << "- 参考节点数：" << ref.nodes.size() << "\n";
    out << "- 当前节点数：" << dm.nodes.size() << "\n";
    out << "- 参考视口：" << ref.viewportWidth << " x " << ref.viewportHeight << "\n";
    out << "- 当前视口：" << dm.viewportWidth << " x " << dm.viewportHeight << "\n";
    out << "- URL：" << escMd(ref.url) << "\n\n";

    out << "## 相似度\n\n";
    out << "| 维度 | 相似度 |\n";
    out << "|------|--------|\n";
    out << "| 结构 | " << pct(result.structuralScore) << "% |\n";
    out << "| 样式 | " << pct(result.styleScore) << "% |\n";
    out << "| 布局 | " << pct(result.layoutScore) << "% |\n\n";

    out << "## 差异统计\n\n";
    out << "- 总计：" << result.diffs.size() << " 条\n";
    out << "- 结构：" << result.structuralDiffCount << " 条\n";
    out << "- 样式：" << result.styleDiffCount << " 条\n";
    out << "- 布局：" << result.layoutDiffCount << " 条\n\n";

    out << "## 按属性聚合（Top 20）\n\n";
    out << "| 属性 | 次数 | 平均严重度 |\n";
    out << "|------|------|------------|\n";
    int n = 0;
    for (auto& p : propList) {
        if (n++ >= 20) break;
        double avg = p.second.count > 0
            ? (double)p.second.sevSum / p.second.count : 0;
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.1f", avg);
        out << "| " << escMd(p.first) << " | " << p.second.count
            << " | " << buf << " |\n";
    }
    out << "\n";

    out << "## 按 DOM 路径聚合（Top 20）\n\n";
    out << "| 路径 | 差异数 |\n";
    out << "|------|--------|\n";
    n = 0;
    for (auto& p : pathList) {
        if (n++ >= 20) break;
        out << "| `" << escMd(p.first) << "` | " << p.second << " |\n";
    }
    out << "\n";

    out << "## 详细差异（前 50 条，按严重度降序）\n\n";
    out << "| 维度 | 属性 | 路径 | 参考值 | 当前值 | 严重度 |\n";
    out << "|------|------|------|--------|--------|--------|\n";
    auto sorted = result.diffs;
    std::sort(sorted.begin(), sorted.end(),
        [](const DimDiff& a, const DimDiff& b) {
            return a.severity > b.severity;
        });
    n = 0;
    for (auto& d : sorted) {
        if (n++ >= 50) break;
        out << "| " << escMd(d.dimension) << " | " << escMd(d.property)
            << " | `" << escMd(d.path) << "`"
            << " | " << escMd(d.refValue)
            << " | " << escMd(d.dmValue)
            << " | " << d.severity << " |\n";
    }
    out << "\n";

    out << "## 建议\n\n";
    out << "按差异次数排序，优先补这些能力：\n\n";
    n = 0;
    for (auto& p : propList) {
        if (n++ >= 10) break;
        out << n << ". **" << escMd(p.first) << "** —— "
            << p.second.count << " 条差异\n";
    }
    out << "\n";

    out.close();

    std::cout << "报告已写入: " << outPath << "\n";
    std::cout << "  差异 " << result.diffs.size() << " 条"
              << "（结构 " << result.structuralDiffCount
              << " / 样式 " << result.styleDiffCount
              << " / 布局 " << result.layoutDiffCount << "）\n";
}

int main(int argc, char** argv) {
    SetConsoleOutputCP(CP_UTF8);

    Database db;
    auto r = db.open("dm_browser.db");
    if (!r.isOk()) {
        std::cerr << "打开学习库失败: " << r.error().msg << "\n";
        return 1;
    }

    LearnStore store(db);
    if (!store.init()) {
        std::cerr << "初始化学习库失败\n";
        return 1;
    }

    std::string cmd = argc > 1 ? argv[1] : "";

    if (cmd.empty() || cmd == "stats") {
        printStats(store);
    }
    else if (cmd == "list") {
        auto samples = store.listSamples(100);
        if (samples.empty()) {
            std::cout << "(空)\n";
        } else {
            for (auto& s : samples) {
                std::cout << "#" << s.id
                          << "  " << s.status
                          << "  差异 " << (int)(s.diffScore * 100) << "%"
                          << "  " << s.url << "\n";
            }
        }
    }
    else if (cmd == "show" && argc > 2) {
        int64_t id = std::stoll(argv[2]);
        auto s = store.getSample(id);
        if (s.id == 0) {
            std::cout << "样本 #" << id << " 不存在\n";
            return 1;
        }
        std::cout << "ID: " << s.id << "\n";
        std::cout << "URL: " << s.url << "\n";
        std::cout << "标题: " << s.title << "\n";
        std::cout << "HTML 大小: " << s.htmlSize << " 字节\n";
        std::cout << "差异率: " << (int)(s.diffScore * 100) << "%\n";
        std::cout << "状态: " << s.status << "\n";

        auto diffs = store.getDiffs(id);
        if (!diffs.empty()) {
            std::cout << "\n差异记录 (" << diffs.size() << " 条):\n";
            for (auto& d : diffs) {
                std::cout << "  [" << d.severity << "] "
                          << d.category << " / " << d.feature
                          << " — " << d.description << "\n";
            }
        }
    }
    else if (cmd == "add" && argc > 4) {
        SiteSample s;
        s.url = argv[2];
        s.html = readFile(argv[3]);
        s.css = readFile(argv[4]);
        s.htmlSize = (int)s.html.size();
        s.title = s.url;
        int64_t id = store.addSample(s);
        std::cout << "已添加样本 #" << id << " (" << s.htmlSize << " 字节)\n";
    }
    else if (cmd == "status" && argc > 3) {
        int64_t id = std::stoll(argv[2]);
        if (store.updateSampleStatus(id, argv[3])) {
            std::cout << "样本 #" << id << " 状态改为: " << argv[3] << "\n";
        } else {
            std::cout << "更新失败\n";
        }
    }
    else if (cmd == "features") {
        store.recalcPriorities();
        auto feats = store.listFeaturesByPriority();
        if (feats.empty()) {
            std::cout << "(暂无特性统计)\n";
        } else {
            std::cout << "特性优先级（高到低）：\n";
            for (auto& f : feats) {
                std::cout << "  " << f.feature
                          << "  测试 " << f.tested
                          << "  通过 " << f.passed
                          << "  失败 " << f.failed
                          << "  优先级 " << f.priority << "\n";
            }
        }
    }
    else if (cmd == "diffs") {
        auto diffs = store.listRecentDiffs(20);
        if (diffs.empty()) {
            std::cout << "(暂无差异记录)\n";
        } else {
            for (auto& d : diffs) {
                std::cout << "#" << d.id
                          << "  样本#" << d.sampleId
                          << "  [" << d.severity << "] "
                          << d.category << " / " << d.feature
                          << " — " << d.description << "\n";
            }
        }
    }
    else if (cmd == "seed") {
        seedDemo(store);
    }
    else if (cmd == "dump") {
        dumpDemoSnapshot();
    }
    else if (cmd == "compare" && argc > 3) {
        auto ref = parseSnapshotFile(argv[2]);
        auto dm  = parseSnapshotFile(argv[3]);
        if (ref.nodes.empty() || dm.nodes.empty()) {
            std::cout << "无法读取快照文件\n";
            return 1;
        }
        auto result = MultiCompare::compare(ref, dm);

        std::cout << "=== 多维对比 ===\n";
        std::cout << "URL: " << result.url << "\n";
        std::cout << "结构相似度: " << (int)(result.structuralScore * 100) << "%\n";
        std::cout << "样式相似度: " << (int)(result.styleScore * 100) << "%\n";
        std::cout << "布局相似度: " << (int)(result.layoutScore * 100) << "%\n";
        std::cout << "差异总数: " << result.diffs.size() << "\n";
        std::cout << "  - 结构: " << result.structuralDiffCount << "\n";
        std::cout << "  - 样式: " << result.styleDiffCount << "\n";
        std::cout << "  - 布局: " << result.layoutDiffCount << "\n\n";

        std::cout << "差异明细（按严重程度）：\n";
        auto sorted = result.diffs;
        std::sort(sorted.begin(), sorted.end(),
            [](const DimDiff& a, const DimDiff& b) {
                return a.severity > b.severity;
            });
        int n = 0;
        for (auto& d : sorted) {
            if (n++ >= 20) break;
            std::cout << "  [" << d.severity << "] "
                      << d.dimension << " / " << d.category << "\n";
            std::cout << "      " << d.path << "\n";
            std::cout << "      " << d.property << ": "
                      << d.refValue << " -> " << d.dmValue << "\n";
        }
    }
    else if (cmd == "infer" && argc > 3) {
        auto ref = parseSnapshotFile(argv[2]);
        auto dm  = parseSnapshotFile(argv[3]);
        auto result = MultiCompare::compare(ref, dm);

        std::set<std::string> feats;
        for (auto& d : result.diffs) {
            if (d.dimension == "style") {
                if (d.property == "display") {
                    if (d.refValue.find("flex") != std::string::npos)
                        feats.insert("display:flex");
                    else if (d.refValue.find("grid") != std::string::npos)
                        feats.insert("display:grid");
                    else if (d.refValue.find("inline-block") != std::string::npos)
                        feats.insert("display:inline-block");
                }
                if (d.property == "position") {
                    if (d.refValue == "absolute")  feats.insert("position:absolute");
                    if (d.refValue == "fixed")     feats.insert("position:fixed");
                    if (d.refValue == "relative")  feats.insert("position:relative");
                }
                if (d.property == "fontSize")  feats.insert("font-size");
                if (d.property == "color")     feats.insert("color");
                if (d.property == "backgroundColor")
                    feats.insert("background-color");
            }
            if (d.dimension == "layout") {
                if (d.property == "width")  feats.insert("width:auto");
                if (d.property == "height") feats.insert("height:auto");
            }
            if (d.dimension == "structural" &&
                d.category == "missing_node") {
                feats.insert("node:" + d.refValue);
            }
        }

        std::cout << "=== 推断缺失特性 ===\n";
        if (feats.empty()) {
            std::cout << "  (无)\n";
        } else {
            for (auto& f : feats) {
                std::cout << "  - " << f << "\n";
                store.recordFeature(f, false);
            }
            store.recalcPriorities();
        }
    }
    else if (cmd == "refs") {
        cmdRefs(store);
    }
    else if (cmd == "inspect" && argc > 2) {
        cmdInspect(store, argv[2]);
    }
    else if (cmd == "export" && argc > 3) {
        cmdExport(store, argv[2], argv[3]);
    }
    else if (cmd == "report" && argc > 4) {
        cmdReport(argv[2], argv[3], argv[4]);
    }
    else {
        printUsage();
    }

    return 0;
}