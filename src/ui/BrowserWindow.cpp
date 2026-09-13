#include "ui/BrowserWindow.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <regex>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

namespace dm::ui {

static const wchar_t* kClassName = L"DMBrowserWindow";

BrowserWindow::BrowserWindow() = default;
BrowserWindow::~BrowserWindow() {
    if (hwnd_) DestroyWindow(hwnd_);
}

bool BrowserWindow::create(const std::wstring& title, int w, int h) {
    width_ = w; height_ = h;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = kClassName;
    RegisterClassExW(&wc);

    hwnd_ = CreateWindowExW(0, kClassName, title.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, w, h,
        nullptr, nullptr, GetModuleHandle(nullptr), this);
    if (!hwnd_) return false;

    SetWindowLongPtrW(hwnd_, GWLP_USERDATA, (LONG_PTR)this);
    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);

    initWebView2Env();
    return true;
}

void BrowserWindow::initWebView2Env() {
    auto options = Microsoft::WRL::Make<CoreWebView2EnvironmentOptions>();
    CreateCoreWebView2EnvironmentWithOptions(
        nullptr, nullptr, options.Get(),
        Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                if (FAILED(result) || !env) {
                    std::cerr << "[UI] WebView2 环境创建失败\n";
                    return S_OK;
                }
                env_ = env;
                envReady_ = true;
                onEnvReady();
                return S_OK;
            }).Get());
}

void BrowserWindow::onEnvReady() {
    std::cout << "[UI] WebView2 环境就绪\n";

    // 创建 UI 层（A 层）
    env_->CreateCoreWebView2Controller(hwnd_,
        Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
            [this](HRESULT result, ICoreWebView2Controller* ctrl) -> HRESULT {
                if (FAILED(result) || !ctrl) return S_OK;
                uiController_ = ctrl;
                ctrl->get_CoreWebView2(&uiWebView_);

                RECT uiBounds{0, 0, width_, uiHeight_};
                ctrl->put_Bounds(uiBounds);
                ctrl->put_IsVisible(TRUE);

                // 注册消息接收
                uiWebView_->add_WebMessageReceived(
                    Microsoft::WRL::Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                        [this](ICoreWebView2*,
                               ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                            LPWSTR json = nullptr;
                            args->TryGetWebMessageAsString(&json);
                            if (json) {
                                handleUIMessage(json);
                                CoTaskMemFree(json);
                            }
                            return S_OK;
                        }).Get(), nullptr);

                // 加载 UI HTML
                std::wstring html = loadUIHtml();
                if (!html.empty()) {
                    uiWebView_->NavigateToString(html.c_str());
                } else {
                    std::cerr << "[UI] ui.html 读取失败\n";
                }

                // 创建第一个内容标签
                createTab(L"");
                return S_OK;
            }).Get());
}

std::wstring BrowserWindow::loadUIHtml() {
    auto readFrom = [](const std::string& path) -> std::string {
        std::ifstream f(path, std::ios::binary);
        if (!f) return "";
        std::ostringstream ss;
        ss << f.rdbuf();
        return ss.str();
    };
    std::string html = readFrom("ui.html");
    if (html.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, html.c_str(),
                                   (int)html.size(), nullptr, 0);
    std::wstring out(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, html.c_str(),
                        (int)html.size(), &out[0], size);
    return out;
}

std::wstring BrowserWindow::loadStartPage() {
    auto readFrom = [](const std::string& path) -> std::string {
        std::ifstream f(path, std::ios::binary);
        if (!f) return "";
        std::ostringstream ss;
        ss << f.rdbuf();
        return ss.str();
    };
    std::string html = readFrom("start_page.html");
    if (html.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, html.c_str(),
                                   (int)html.size(), nullptr, 0);
    std::wstring out(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, html.c_str(),
                        (int)html.size(), &out[0], size);
    return out;
}

void BrowserWindow::createTab(const std::wstring& url) {
    int id = tabs_.create();
    tabs_.activate(id);

    if (!envReady_) {
        pendingTabs_.push_back(id);
        return;
    }

    env_->CreateCoreWebView2Controller(hwnd_,
        Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
            [this, id, url](HRESULT result, ICoreWebView2Controller* ctrl) -> HRESULT {
                if (FAILED(result) || !ctrl) return S_OK;
                Tab* t = tabs_.get(id);
                if (!t) { ctrl->Close(); return S_OK; }

                t->controller = ctrl;
                ctrl->get_CoreWebView2(&t->webview);
                ctrl->put_ZoomFactor(1.0);

                RECT bounds{0, uiHeight_, width_, height_};
                ctrl->put_Bounds(bounds);
                ctrl->put_IsVisible(tabs_.activeId() == id ? TRUE : FALSE);

                // 加载内容
                if (url.empty()) {
                    std::wstring html = loadStartPage();
                    if (!html.empty()) {
                        t->webview->NavigateToString(html.c_str());
                        t->url = L"dm://start";
                        t->title = L"新标签页";
                    } else {
                        t->webview->Navigate(L"https://www.deepseek.com/");
                        t->url = L"https://www.deepseek.com/";
                        t->title = L"DeepSeek";
                    }
                } else {
                    t->webview->Navigate(url.c_str());
                    t->url = url;
                    t->title = url;
                }

                onContentNavCompleted(id);
                syncTabsToUI();
                syncAddressToUI(t->url);
                return S_OK;
            }).Get());

    syncTabsToUI();
    layout();
}

void BrowserWindow::closeTab(int id) {
    Tab* t = tabs_.get(id);
    if (t && t->controller) t->controller->Close();
    tabs_.close(id);
    if (tabs_.count() > 0) {
        activateTab(tabs_.activeId());
    } else {
        createTab(L"");
    }
    syncTabsToUI();
}

void BrowserWindow::activateTab(int id) {
    if (!tabs_.activate(id)) return;
    for (auto& t : const_cast<std::vector<Tab>&>(tabs_.all())) {
        if (t.controller) {
            t.controller->put_IsVisible(t.id == id ? TRUE : FALSE);
            if (t.id == id) {
                RECT bounds{0, uiHeight_, width_, height_};
                t.controller->put_Bounds(bounds);
            }
        }
    }
    Tab* t = tabs_.get(id);
    if (t) syncAddressToUI(t->url);
    syncTabsToUI();
}

void BrowserWindow::navigateActive(const std::wstring& url) {
    Tab* t = tabs_.active();
    if (!t || !t->webview) return;

    std::wstring finalUrl = url;
    if (finalUrl.find(L"://") == std::wstring::npos &&
        finalUrl.find(L"about:") != 0 &&
        finalUrl.find(L"dm:") != 0) {
        finalUrl = L"https://" + finalUrl;
    }
    t->url = finalUrl;
    t->webview->Navigate(finalUrl.c_str());
    syncAddressToUI(finalUrl);
}

void BrowserWindow::onContentNavCompleted(int tabId) {
    Tab* t = tabs_.get(tabId);
    if (!t || !t->webview) return;

    t->webview->add_NavigationCompleted(
        Microsoft::WRL::Callback<ICoreWebView2NavigationCompletedEventHandler>(
            [this, tabId](ICoreWebView2* sender,
                          ICoreWebView2NavigationCompletedEventArgs*) -> HRESULT {
                Tab* tt = tabs_.get(tabId);
                if (!tt) return S_OK;

                LPWSTR uri = nullptr;
                sender->get_Source(&uri);
                if (uri) {
                    tt->url = uri;
                    if (tabs_.activeId() == tabId) {
                        syncAddressToUI(uri);
                    }
                    CoTaskMemFree(uri);
                }

                // 标题
                LPWSTR title = nullptr;
                sender->get_DocumentTitle(&title);
                if (title && title[0]) {
                    tt->title = title;
                    CoTaskMemFree(title);
                }

                syncTabsToUI();

                // 前进后退状态
                BOOL canBack = FALSE, canForward = FALSE;
                sender->get_CanGoBack(&canBack);
                sender->get_CanGoForward(&canForward);
                if (tabs_.activeId() == tabId) {
                    syncNavStateToUI(canBack, canForward);
                }

                return S_OK;
            }).Get(), nullptr);
}

void BrowserWindow::syncTabsToUI() {
    if (!uiWebView_) return;

    std::wostringstream json;
    json << L"{\"type\":\"updateTabs\",\"activeId\":" << tabs_.activeId()
         << L",\"tabs\":[";
    bool first = true;
    for (const auto& t : tabs_.all()) {
        if (!first) json << L",";
        first = false;
        json << L"{\"id\":" << t.id
             << L",\"title\":\"" << t.title << L"\"}";
    }
    json << L"]}";

    uiWebView_->PostWebMessageAsString(json.str().c_str());
}

void BrowserWindow::syncAddressToUI(const std::wstring& url) {
    if (!uiWebView_) return;
    std::wostringstream json;
    json << L"{\"type\":\"updateAddress\",\"url\":\"" << url << L"\"}";
    uiWebView_->PostWebMessageAsString(json.str().c_str());
}

void BrowserWindow::syncNavStateToUI(bool canBack, bool canForward) {
    if (!uiWebView_) return;
    std::wostringstream json;
    json << L"{\"type\":\"updateNavState\",\"canBack\":"
         << (canBack ? L"true" : L"false")
         << L",\"canForward\":" << (canForward ? L"true" : L"false") << L"}";
    uiWebView_->PostWebMessageAsString(json.str().c_str());
}

void BrowserWindow::handleUIMessage(const std::wstring& json) {
    // 简单解析：找 "type":"xxx"
    auto findType = [&json](const std::wstring& key) -> std::wstring {
        auto pos = json.find(L"\"" + key + L"\"");
        if (pos == std::wstring::npos) return L"";
        pos = json.find(L":", pos);
        if (pos == std::wstring::npos) return L"";
        pos = json.find(L"\"", pos);
        if (pos == std::wstring::npos) return L"";
        auto end = json.find(L"\"", pos + 1);
        if (end == std::wstring::npos) return L"";
        return json.substr(pos + 1, end - pos - 1);
    };
    auto findInt = [&json](const std::wstring& key) -> int {
        auto pos = json.find(L"\"" + key + L"\"");
        if (pos == std::wstring::npos) return 0;
        pos = json.find(L":", pos);
        if (pos == std::wstring::npos) return 0;
        return std::stoi(json.substr(pos + 1));
    };

    std::wstring type = findType(L"type");

    if (type == L"newTab") {
        createTab(L"");
    } else if (type == L"closeTab") {
        closeTab(findInt(L"id"));
    } else if (type == L"activateTab") {
        activateTab(findInt(L"id"));
    } else if (type == L"navigate") {
        navigateActive(findType(L"url"));
    } else if (type == L"back") {
        Tab* t = tabs_.active();
        if (t && t->webview) t->webview->GoBack();
    } else if (type == L"forward") {
        Tab* t = tabs_.active();
        if (t && t->webview) t->webview->GoForward();
    } else if (type == L"reload") {
        Tab* t = tabs_.active();
        if (t && t->webview) t->webview->Reload();
    } else if (type == L"home") {
        navigateActive(L"dm://start");
        Tab* t = tabs_.active();
        if (t && t->webview) {
            std::wstring html = loadStartPage();
            if (!html.empty()) t->webview->NavigateToString(html.c_str());
        }
    }
}

void BrowserWindow::layout() {
    if (uiController_) {
        RECT b{0, 0, width_, uiHeight_};
        uiController_->put_Bounds(b);
    }
    RECT bounds{0, uiHeight_, width_, height_};
    for (auto& t : const_cast<std::vector<Tab>&>(tabs_.all())) {
        if (t.controller) t.controller->put_Bounds(bounds);
    }
}

LRESULT CALLBACK BrowserWindow::WndProc(HWND hwnd, UINT msg,
                                         WPARAM wp, LPARAM lp) {
    BrowserWindow* self =
        (BrowserWindow*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    switch (msg) {
        case WM_CREATE: {
            auto* cs = (CREATESTRUCT*)lp;
            self = (BrowserWindow*)cs->lpCreateParams;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)self);
            self->hwnd_ = hwnd;
            return 0;
        }
        case WM_SIZE: {
            if (self) {
                self->width_ = LOWORD(lp);
                self->height_ = HIWORD(lp);
                self->layout();
            }
            return 0;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int BrowserWindow::run() {
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}

} // namespace dm::ui