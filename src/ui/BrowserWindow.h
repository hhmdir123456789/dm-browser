#pragma once
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
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

    std::wstring getActiveUrl() const;

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

    void initWebView2Env();
    void onEnvReady();

    void createTab(const std::wstring& url = L"");
    void closeTab(int64_t id);
    void activateTab(int64_t id);
    void navigateActive(const std::wstring& url);

    std::wstring loadUIHtml();
    std::wstring loadStartPage();
    std::wstring readFileAsWide(const std::string& path);

    void syncTabsToUI();
    void syncAddressToUI(const std::wstring& url);
    void syncNavStateToUI(bool canBack, bool canForward);

    void handleUIMessage(const std::wstring& json);
    void onContentNavCompleted(int64_t tabId);

    void layout();
    void updateLockIcon(const std::wstring& url);

    HWND hwnd_{nullptr};

    Microsoft::WRL::ComPtr<ICoreWebView2Controller> uiController_;
    Microsoft::WRL::ComPtr<ICoreWebView2> uiWebView_;

    TabManager tabs_;
    Microsoft::WRL::ComPtr<ICoreWebView2Environment> env_;
    bool envReady_{false};

    int width_{1024}, height_{768};
    int uiHeight_{88};

    std::vector<std::wstring> pendingUrls_;
    std::vector<int64_t> pendingTabs_;

    std::mutex createMutex_;
    std::mutex syncMutex_;
    std::mutex pendingMutex_;
    std::mutex fileMutex_;

    std::wstring cachedUIHtml_;
    std::wstring cachedStartPage_;
};

} // namespace dm::ui