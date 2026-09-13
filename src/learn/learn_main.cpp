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
#include <algorithm>

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

int main(int argc, char** argv) {
    SetConsoleOutputCP(CP_UTF8);

    Database db;
    auto r = db.open("dm_learn.db");
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
    else {
        printUsage();
    }

    return 0;
}