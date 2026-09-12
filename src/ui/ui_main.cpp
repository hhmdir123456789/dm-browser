#include "ui/RenderWindow.h"
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    dm::ui::RenderWindow win;
    if (!win.create(L"大明DM浏览器", 1024, 768)) {
        std::cerr << "窗口创建失败\n";
        return 1;
    }

    win.onReady([&win]() {
        std::cout << "[UI] 导航到 bing.com\n";
        win.navigate(L"https://www.bing.com");
    });

    win.onNewWindow([](const std::wstring& url) {
        std::wcout << L"[UI] 新窗口请求: " << url << L"\n";
    });

    return win.run();
}