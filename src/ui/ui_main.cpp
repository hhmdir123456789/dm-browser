#include "ui/Window.h"
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    dm::ui::Window win;
    if (!win.create("大明DM浏览器", 1024, 768)) {
        std::cerr << "窗口创建失败\n";
        return 1;
    }

    std::vector<dm::ui::TabView> tabs = {
        {1, "起始页", "https://start.dm", true},
        {2, "书签", "https://bookmarks.dm", false},
    };
    win.setTabs(tabs);

    win.onNewTab([&]() { std::cout << "[UI] 新标签\n"; });
    win.onCloseTab([&](int id) { std::cout << "[UI] 关闭标签 " << id << "\n"; });
    win.onSwitchTab([&](int id) { std::cout << "[UI] 切换标签 " << id << "\n"; });
    win.onNavigate([&](const std::string& url) {
        std::cout << "[UI] 导航到 " << url << "\n";
    });

    return win.run();
}