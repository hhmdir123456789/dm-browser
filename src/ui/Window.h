#pragma once
#include <string>
#include <vector>
#include <functional>
#include <windows.h>

namespace dm::ui {

struct TabView {
    int id;
    std::string title;
    std::string origin;
    bool active;
};

class Window {
public:
    Window();
    ~Window();

    bool create(const std::string& title, int width, int height);
    void setTabs(const std::vector<TabView>& tabs);

    using NewTabFn    = std::function<void()>;
    using CloseTabFn  = std::function<void(int)>;
    using SwitchTabFn = std::function<void(int)>;
    using NavigateFn  = std::function<void(const std::string&)>;

    void onNewTab(NewTabFn fn)      { newTabFn_ = std::move(fn); }
    void onCloseTab(CloseTabFn fn)  { closeTabFn_ = std::move(fn); }
    void onSwitchTab(SwitchTabFn fn){ switchTabFn_ = std::move(fn); }
    void onNavigate(NavigateFn fn)  { navigateFn_ = std::move(fn); }

    int run();

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    void paint(HDC hdc);
    void layout();
    int hitTestTab(int x, int y);

    HWND hwnd_{nullptr};
    std::vector<TabView> tabs_;
    std::string address_;
    int width_{1024}, height_{768};

    static constexpr int kTabHeight = 32;
    static constexpr int kTabWidth  = 180;
    static constexpr int kAddrHeight = 36;
    static constexpr int kStatusHeight = 24;

    NewTabFn newTabFn_;
    CloseTabFn closeTabFn_;
    SwitchTabFn switchTabFn_;
    NavigateFn navigateFn_;
};

} // namespace dm::ui
