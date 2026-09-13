#include <windows.h>
#include "knowledge/knowledge_base.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

using namespace dm::knowledge;

static std::string getExeDir() {
    char path[MAX_PATH];
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    std::string dir = path;
    auto pos = dir.find_last_of("\\/");
    if (pos != std::string::npos) dir = dir.substr(0, pos + 1);
    return dir;
}

static void printUsage() {
    std::cout << "dm_kb - 知识库工具\n\n";
    std::cout << "用法:\n";
    std::cout << "  dm_kb                          显示报告\n";
    std::cout << "  dm_kb list                     列出所有条目\n";
    std::cout << "  dm_kb list <category>          按分类列出 (html/css/js)\n";
    std::cout << "  dm_kb find <id>                按 id 查找\n";
    std::cout << "  dm_kb search <keyword>         搜索\n";
    std::cout << "  dm_kb missing                  列出未实现的\n";
    std::cout << "  dm_kb prompt <id>              生成 AI 修复 Prompt\n";
    std::cout << "  dm_kb prompt <id> <code_file>  带源码片段\n";
}

int main(int argc, char** argv) {
    SetConsoleOutputCP(CP_UTF8);

    // 加载知识库
    KnowledgeBase kb;
    std::string exeDir = getExeDir();
    bool ok = kb.load(exeDir + "knowledge");
    if (!ok) {
        ok = kb.load(exeDir + "../../knowledge");
    }
    if (!ok) {
        ok = kb.load("knowledge");
    }
    if (!ok) {
        std::cerr << "无法加载知识库。\n";
        std::cerr << "请确认 knowledge/ 目录下有 html.json / css.json / js.json\n";
        return 1;
    }

    std::string cmd = argc > 1 ? argv[1] : "";

    if (cmd.empty() || cmd == "report") {
        std::cout << kb.generatePriorityReport() << "\n";

        std::cout << "\n按分类统计：\n";
        const char* cats[] = { "html", "css", "js" };
        for (auto* cat : cats) {
            auto list = kb.byCategory(cat);
            int impl = 0;
            for (auto* e : list) if (e->implemented) impl++;
            std::cout << "  " << cat << ": " << impl
                      << " / " << list.size() << " 已实现\n";
        }
    }
    else if (cmd == "list") {
        if (argc > 2) {
            auto list = kb.byCategory(argv[2]);
            for (auto* e : list) {
                std::cout << (e->implemented ? "[v] " : "[ ] ")
                          << e->id << "  " << e->name << "\n";
            }
        } else {
            std::cout << "总计 " << kb.total() << " 条：\n";
            const char* cats[] = { "html", "css", "js" };
            for (auto* cat : cats) {
                auto list = kb.byCategory(cat);
                std::cout << "\n[" << cat << "]\n";
                for (auto* e : list) {
                    std::cout << "  " << (e->implemented ? "v" : " ")
                              << " " << e->id << "\n";
                }
            }
        }
    }
    else if (cmd == "find" && argc > 2) {
        auto* e = kb.findById(argv[2]);
        if (!e) {
            std::cout << "未找到: " << argv[2] << "\n";
            return 1;
        }
        std::cout << "ID: " << e->id << "\n";
        std::cout << "名称: " << e->name << "\n";
        std::cout << "分类: " << e->category << "\n";
        std::cout << "摘要: " << e->summary << "\n";
        std::cout << "规范: " << e->specUrl << "\n";
        std::cout << "实现: " << (e->implemented ? "是" : "否") << "\n";
        std::cout << "难度: " << e->difficulty << "\n";
        if (!e->syntax.empty()) std::cout << "语法: " << e->syntax << "\n";
        if (!e->relatedCode.empty()) {
            std::cout << "相关代码:\n";
            for (auto& c : e->relatedCode) std::cout << "  - " << c << "\n";
        }
    }
    else if (cmd == "search" && argc > 2) {
        auto list = kb.search(argv[2]);
        std::cout << "找到 " << list.size() << " 条：\n";
        for (auto* e : list) {
            std::cout << "  " << (e->implemented ? "v" : " ")
                      << " " << e->id << "  " << e->name << "\n";
        }
    }
    else if (cmd == "missing") {
        auto list = kb.notImplemented();
        std::cout << "未实现 " << list.size() << " 条：\n";
        for (auto* e : list) {
            std::cout << "  " << e->id
                      << "  难度 " << e->difficulty
                      << "  失败 " << e->failCount << "\n";
        }
    }
    else if (cmd == "prompt" && argc > 2) {
        std::string code;
        if (argc > 3) {
            std::ifstream f(argv[3]);
            if (f) {
                std::stringstream ss;
                ss << f.rdbuf();
                code = ss.str();
                if (code.size() > 3000) code = code.substr(0, 3000) + "\n...(截断)";
            }
        }
        std::string prompt = kb.buildFixPrompt(argv[2], code, "");
        if (prompt.empty()) {
            std::cout << "未找到: " << argv[2] << "\n";
            return 1;
        }
        std::cout << prompt;
    }
    else {
        printUsage();
    }

    return 0;
}