#pragma once
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <cstdint>
#include <wrl.h>
#include <WebView2.h>

namespace dm::ui {

struct Tab {
    int64_t id{0};
    std::wstring title{L"新标签页"};
    std::wstring url{L""};
    std::wstring faviconUrl{L""};
    bool loading{false};
    bool navHandlerRegistered{false};
    Microsoft::WRL::ComPtr<ICoreWebView2Controller> controller;
    Microsoft::WRL::ComPtr<ICoreWebView2> webview;
};

class TabManager {
public:
    int64_t create();
    bool close(int64_t id);
    bool activate(int64_t id);
    std::shared_ptr<Tab> get(int64_t id);
    std::shared_ptr<Tab> active();
    std::vector<std::shared_ptr<Tab>> all() const;
    std::vector<std::shared_ptr<Tab>> allMutable();
    size_t count() const;
    int64_t activeId() const;

private:
    void fixActiveId();
    mutable std::mutex mu_;
    std::vector<std::shared_ptr<Tab>> tabs_;
    int64_t nextId_{1};
    int64_t activeId_{0};
};

} // namespace dm::ui