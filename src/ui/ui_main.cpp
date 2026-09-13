#include "ui/BrowserWindow.h"
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    dm::ui::BrowserWindow win;
    if (!win.create(L"大明DM浏览器", 1024, 768)) {
        std::cerr << "窗口创建失败\n";
        return 1;
    }

    std::cout << "[UI] 多标签窗口已启动\n";
    return win.run();
}