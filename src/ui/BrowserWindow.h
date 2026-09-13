#pragma once
#include <string>
#include <vector>
#include <windows.h>
#include <wrl.h>
#include <WebView2.h>
#include <WebView2EnvironmentOptions.h>
#include "ui/TabManager.h"

namespace dm::ui {

class BrowserWindow {
public:
    BrowserWindow();
    ~BrowserWindow();

    bool create(const std::wstring& title, int width, int height);
    int run();

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

    void initWebView2Env();
    void onEnvReady();

    // 创建内容层 WebView2（每个标签一个）
    void createTab(const std::wstring& url = L"");
    void closeTab(int id);
    void activateTab(int id);
    void navigateActive(const std::wstring& url);

    // 同步 UI 层
    void syncTabsToUI();
    void syncAddressToUI(const std::wstring& url);
    void syncNavStateToUI(bool canBack, bool canForward);

    // 处理来自 UI 层的消息
    void handleUIMessage(const std::wstring& json);

    std::wstring loadStartPage();
    std::wstring loadUIHtml();

    void layout();
    void onContentNavCompleted(int tabId);

    HWND hwnd_{nullptr};

    // 两层 WebView2
    Microsoft::WRL::ComPtr<ICoreWebView2Controller> uiController_;
    Microsoft::WRL::ComPtr<ICoreWebView2> uiWebView_;

    // 内容层：每个标签一个 controller
    TabManager tabs_;
    Microsoft::WRL::ComPtr<ICoreWebView2Environment> env_;
    bool envReady_{false};

    int width_{1024}, height_{768};
    int uiHeight_{88};   // UI 层高度 = 标签栏 40 + 工具栏 48

    std::vector<int> pendingTabs_;
};

} // namespace dm::ui