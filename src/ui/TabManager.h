#pragma once
#include <string>
#include <vector>
#include <wrl.h>
#include <WebView2.h>

namespace dm::ui {

struct Tab {
    int id{0};
    std::wstring title{L"新标签页"};
    std::wstring url{L"about:blank"};
    bool loading{false};
    Microsoft::WRL::ComPtr<ICoreWebView2Controller> controller;
    Microsoft::WRL::ComPtr<ICoreWebView2> webview;
};

class TabManager {
public:
    int create();                       // 新建标签，返回 id
    bool close(int id);                 // 关闭标签
    bool activate(int id);              // 激活标签
    Tab* get(int id);                   // 获取标签
    Tab* active();                      // 当前激活标签
    const std::vector<Tab>& all() const { return tabs_; }
    size_t count() const { return tabs_.size(); }
    int activeId() const { return activeId_; }

private:
    std::vector<Tab> tabs_;
    int nextId_{1};
    int activeId_{0};
};

} // namespace dm::ui