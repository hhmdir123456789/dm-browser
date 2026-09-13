#pragma once
#include <string>
#include <functional>
#include <windows.h>
#include <wrl.h>
#include <WebView2.h>
#include <WebView2EnvironmentOptions.h>

namespace dm::ui {

class RenderWindow {
public:
    RenderWindow();
    ~RenderWindow();

    // 创建窗口并初始化 WebView2
    bool create(const std::wstring& title, int width, int height);

    // 导航到指定 URL
    void navigate(const std::wstring& url);

    // 把 HTML 字符串作为页面内容加载
    void navigateToString(const std::wstring& html);

    // 消息循环
    int run();

    // WebView2 就绪回调
    using ReadyFn = std::function<void()>;
    void onReady(ReadyFn fn) { readyFn_ = std::move(fn); }

    // 新窗口请求回调
    using NewWindowFn = std::function<void(const std::wstring&)>;
    void onNewWindow(NewWindowFn fn) { newWindowFn_ = std::move(fn); }

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    void onWebViewReady();

    HWND hwnd_{nullptr};
    Microsoft::WRL::ComPtr<ICoreWebView2Controller> controller_;
    Microsoft::WRL::ComPtr<ICoreWebView2> webview_;
    ReadyFn readyFn_;
    NewWindowFn newWindowFn_;
};

} // namespace dm::ui