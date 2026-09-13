#pragma once
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>
#include <windows.h>
#include <wrl.h>
#include <WebView2.h>
#include <WebView2EnvironmentOptions.h>
#include "ui/TabManager.h"
#include "ui/AiClient.h"
#include "ui/SettingsStore.h"
#include "learn/snapshot.h"
#include "learn/multi_compare.h"
#include "learn/learn_store.h"
#include "db/Database.h"
#include "Storage.h"

namespace dm::ui {

#define WM_AI_CHUNK  (WM_APP + 1)
#define WM_AI_ERROR  (WM_APP + 2)
#define WM_AI_DONE   (WM_APP + 3)
#define WM_OLLAMA_RESULT  (WM_APP + 4)

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
    void initSidebar();

    void createTab(const std::wstring& url = L"");
    void closeTab(int64_t id);
    void activateTab(int64_t id);
    void navigateActive(const std::wstring& url);

    std::wstring loadUIHtml();
    std::wstring loadStartPage();
    std::wstring loadSidebarHtml();
    std::wstring readFileAsWide(const std::string& path);

    void syncTabsToUI(bool force = false);
    void syncAddressToUI(const std::wstring& url);
    void syncNavStateToUI(bool canBack, bool canForward);
    void syncStarStateToUI(bool starred);
    void sendBookmarksToContent();

    void handleUIMessage(const std::wstring& json);
    void handleSidebarMessage(const std::wstring& json);
    void onContentNavCompleted(int64_t tabId);

    void applyLayout();
    void layout();
    void updateLockIcon(const std::wstring& url);

    // 侧边栏 / AI
    void onToggleSidebar(bool open);
    void onAiChat(const std::wstring& json);
    void onCheckOllama();
    void onLoadAudit();
    void onLoadHistory();

    // 学习库分析（批 4A）
    void onAnalyzePage();
    void onLoadFeatures();
    void sendAnalyzeResult(const dm::learn::MultiDimResult& result,
                           const std::vector<std::string>& inferred);
    void sendAnalyzeError(const std::string& error);
    void onHighlightDiff(const std::wstring& path);

    HWND hwnd_{nullptr};

    Microsoft::WRL::ComPtr<ICoreWebView2Controller> uiController_;
    Microsoft::WRL::ComPtr<ICoreWebView2> uiWebView_;

    Microsoft::WRL::ComPtr<ICoreWebView2Controller> sidebarController_;
    Microsoft::WRL::ComPtr<ICoreWebView2> sidebarWebView_;
    bool sidebarOpen_{false};
    int sidebarWidth_{380};

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
    std::atomic<ULONGLONG> lastSyncMs_{0};

    std::wstring cachedUIHtml_;
    std::wstring cachedStartPage_;
    std::wstring cachedSidebarHtml_;

    Database db_;
    std::unique_ptr<BookmarkStore> bookmarkStore_;
    std::unique_ptr<SettingsStore> settings_;
    AiClient aiClient_;
    std::unique_ptr<dm::learn::LearnStore> learnStore_;
};

} // namespace dm::ui