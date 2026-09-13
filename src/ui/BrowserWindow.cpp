#include "ui/BrowserWindow.h"
#include <iostream>
#include <sstream>
#include <fstream>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

namespace dm::ui {

static const wchar_t* kClassName = L"DMBrowserWindow";

static std::string wideToUtf8(const std::wstring& w) {
    if (w.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, w.c_str(),
                                   (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string out(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(),
                        (int)w.size(), &out[0], size, nullptr, nullptr);
    return out;
}

static std::wstring escapeJson(const std::wstring& s) {
    std::wstring out;
    out.reserve(s.size() + 8);
    for (wchar_t c : s) {
        if (c == L'"') out += L"\\\"";
        else if (c == L'\\') out += L"\\\\";
        else if (c == L'\n') out += L"\\n";
        else if (c == L'\r') out += L"\\r";
        else if (c == L'\t') out += L"\\t";
        else out += c;
    }
    return out;
}

static std::wstring urlEncode(const std::wstring& s) {
    std::wstring out;
    out.reserve(s.size() * 3);
    for (wchar_t c : s) {
        if ((c >= L'A' && c <= L'Z') ||
            (c >= L'a' && c <= L'z') ||
            (c >= L'0' && c <= L'9') ||
            c == L'-' || c == L'_' || c == L'.' || c == L'~' ||
            c == L':' || c == L'/' || c == L'?' || c == L'=' ||
            c == L'&' || c == L'%' || c == L'#') {
            out += c;
        } else {
            char buf[8];
            sprintf_s(buf, "%%%04X", (unsigned)c);
            out += std::wstring(buf, buf + 3);
        }
    }
    return out;
}

BrowserWindow::BrowserWindow() = default;

BrowserWindow::~BrowserWindow() {
    if (uiController_) uiController_->Close();
    for (auto& t : tabs_.allMutable()) {
        if (t->controller) t->controller->Close();
    }
    Sleep(100);
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

    env_->CreateCoreWebView2Controller(hwnd_,
        Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
            [this](HRESULT result, ICoreWebView2Controller* ctrl) -> HRESULT {
                if (FAILED(result) || !ctrl) {
                    std::cerr << "[UI] UI 层 controller 创建失败\n";
                    return S_OK;
                }
                uiController_ = ctrl;
                ctrl->get_CoreWebView2(&uiWebView_);

                RECT uiBounds{0, 0, width_, uiHeight_};
                ctrl->put_Bounds(uiBounds);
                ctrl->put_IsVisible(TRUE);

                // 注册消息接收
                HRESULT hr = uiWebView_->add_WebMessageReceived(
                    Microsoft::WRL::Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                        [this](ICoreWebView2*,
                               ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                            LPWSTR json = nullptr;
                            HRESULT r = args->TryGetWebMessageAsString(&json);
                            if (SUCCEEDED(r) && json) {
                                std::cout << "[C++] 收到 UI 消息: "
                                          << wideToUtf8(json) << "\n";
                                handleUIMessage(json);
                                CoTaskMemFree(json);
                            } else {
                                LPWSTR raw = nullptr;
                                if (SUCCEEDED(args->get_WebMessageAsJson(&raw)) && raw) {
                                    std::cout << "[C++] 收到 UI JSON: "
                                              << wideToUtf8(raw) << "\n";
                                    handleUIMessage(raw);
                                    CoTaskMemFree(raw);
                                } else {
                                    std::cout << "[C++] 消息读取失败\n";
                                }
                            }
                            return S_OK;
                        }).Get(), nullptr);

                if (FAILED(hr)) {
                    std::cerr << "[UI] add_WebMessageReceived 失败: "
                              << std::hex << hr << "\n";
                } else {
                    std::cout << "[UI] WebMessageReceived 注册成功\n";
                }

                std::wstring html = loadUIHtml();
                if (!html.empty()) {
                    std::cout << "[UI] 加载 ui.html (" << html.size() << " 字节)\n";
                    uiWebView_->NavigateToString(html.c_str());
                } else {
                    std::cerr << "[UI] ui.html 读取失败\n";
                }

                {
                    std::lock_guard lock(pendingMutex_);
                    for (auto& u : pendingUrls_) createTab(u);
                    pendingUrls_.clear();
                    for (auto id : pendingTabs_) {
                        (void)id;
                        createTab(L"");
                    }
                    pendingTabs_.clear();
                }

                if (tabs_.count() == 0) createTab(L"");
                return S_OK;
            }).Get());
}

std::wstring BrowserWindow::readFileAsWide(const std::string& path) {
    std::lock_guard lock(fileMutex_);
    std::ifstream f(path, std::ios::binary);
    if (!f) return L"";
    std::ostringstream ss;
    ss << f.rdbuf();
    std::string s = ss.str();
    if (s.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, s.c_str(),
                                   (int)s.size(), nullptr, 0);
    std::wstring out(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(),
                        (int)s.size(), &out[0], size);
    return out;
}

std::wstring BrowserWindow::loadUIHtml() {
    if (!cachedUIHtml_.empty()) return cachedUIHtml_;
    cachedUIHtml_ = readFileAsWide("ui.html");
    if (cachedUIHtml_.empty()) {
        char exePath[MAX_PATH];
        GetModuleFileNameA(nullptr, exePath, MAX_PATH);
        std::string dir = exePath;
        auto pos = dir.find_last_of("\\/");
        if (pos != std::string::npos) dir = dir.substr(0, pos + 1);
        cachedUIHtml_ = readFileAsWide(dir + "ui.html");
    }
    return cachedUIHtml_;
}

std::wstring BrowserWindow::loadStartPage() {
    if (!cachedStartPage_.empty()) return cachedStartPage_;
    cachedStartPage_ = readFileAsWide("start_page.html");
    if (cachedStartPage_.empty()) {
        char exePath[MAX_PATH];
        GetModuleFileNameA(nullptr, exePath, MAX_PATH);
        std::string dir = exePath;
        auto pos = dir.find_last_of("\\/");
        if (pos != std::string::npos) dir = dir.substr(0, pos + 1);
        cachedStartPage_ = readFileAsWide(dir + "start_page.html");
    }
    return cachedStartPage_;
}

void BrowserWindow::createTab(const std::wstring& url) {
    {
        std::lock_guard lock(createMutex_);
        if (!envReady_) {
            std::lock_guard plock(pendingMutex_);
            pendingUrls_.push_back(url);
            return;
        }
    }

    int64_t id = tabs_.create();
    tabs_.activate(id);
    syncTabsToUI();

    std::wstring target = url;
    bool useStartPage = (url.empty() || url == L"about:blank");

    env_->CreateCoreWebView2Controller(hwnd_,
        Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
            [this, id, target, useStartPage](HRESULT result,
                    ICoreWebView2Controller* ctrl) -> HRESULT {
                if (FAILED(result) || !ctrl) {
                    tabs_.close(id);
                    syncTabsToUI();
                    return S_OK;
                }
                auto t = tabs_.get(id);
                if (!t) { ctrl->Close(); return S_OK; }

                t->controller = ctrl;
                ctrl->get_CoreWebView2(&t->webview);
                ctrl->put_ZoomFactor(1.0);

                RECT bounds{0, uiHeight_, width_, height_};
                ctrl->put_Bounds(bounds);
                ctrl->put_IsVisible(tabs_.activeId() == id ? TRUE : FALSE);

                onContentNavCompleted(id);

                if (useStartPage) {
                    std::wstring html = loadStartPage();
                    if (!html.empty()) {
                        t->webview->NavigateToString(html.c_str());
                        t->url = L"dm://start";
                        t->title = L"新标签页";
                        syncAddressToUI(L"dm://start");
                    } else {
                        t->webview->Navigate(L"https://www.deepseek.com/");
                        t->url = L"https://www.deepseek.com/";
                        t->title = L"DeepSeek";
                        syncAddressToUI(L"https://www.deepseek.com/");
                    }
                } else {
                    std::wstring enc = urlEncode(target);
                    t->webview->Navigate(enc.c_str());
                    t->url = target;
                    t->title = target;
                    syncAddressToUI(target);
                }

                syncTabsToUI();
                layout();
                return S_OK;
            }).Get());
}

void BrowserWindow::closeTab(int64_t id) {
    auto t = tabs_.get(id);
    Microsoft::WRL::ComPtr<ICoreWebView2Controller> ctrl;
    if (t) ctrl = t->controller;

    tabs_.close(id);

    if (ctrl) ctrl->Close();

    if (tabs_.count() > 0) {
        activateTab(tabs_.activeId());
    } else {
        createTab(L"");
    }
    syncTabsToUI();
}

void BrowserWindow::activateTab(int64_t id) {
    if (!tabs_.activate(id)) return;

    for (auto& t : tabs_.allMutable()) {
        if (t->controller) {
            t->controller->put_IsVisible(t->id == id ? TRUE : FALSE);
            if (t->id == id) {
                RECT bounds{0, uiHeight_, width_, height_};
                t->controller->put_Bounds(bounds);
            }
        }
    }

    auto t = tabs_.get(id);
    if (t) {
        syncAddressToUI(t->url);
        updateLockIcon(t->url);
    }
    syncTabsToUI();
}

void BrowserWindow::navigateActive(const std::wstring& url) {
    auto t = tabs_.active();
    if (!t || !t->webview) return;

    std::wstring finalUrl = url;
    if (finalUrl.empty()) return;

    if (finalUrl == L"dm://start" || finalUrl == L"dm://newtab") {
        std::wstring html = loadStartPage();
        if (!html.empty()) {
            t->webview->NavigateToString(html.c_str());
            t->url = L"dm://start";
            t->title = L"新标签页";
            syncAddressToUI(L"dm://start");
            updateLockIcon(L"dm://start");
            syncTabsToUI();
        }
        return;
    }

    if (finalUrl.find(L"://") == std::wstring::npos &&
        finalUrl.find(L"about:") != 0) {
        finalUrl = L"https://" + finalUrl;
    }

    t->url = finalUrl;
    std::wstring enc = urlEncode(finalUrl);
    t->webview->Navigate(enc.c_str());
    syncAddressToUI(finalUrl);
    updateLockIcon(finalUrl);
    syncTabsToUI();
}

void BrowserWindow::onContentNavCompleted(int64_t tabId) {
    auto t = tabs_.get(tabId);
    if (!t || !t->webview) return;
    if (t->navHandlerRegistered) return;
    t->navHandlerRegistered = true;

    t->webview->add_NavigationCompleted(
        Microsoft::WRL::Callback<ICoreWebView2NavigationCompletedEventHandler>(
            [this, tabId](ICoreWebView2* sender,
                          ICoreWebView2NavigationCompletedEventArgs*) -> HRESULT {
                auto tt = tabs_.get(tabId);
                if (!tt) return S_OK;

                tt->loading = false;

                LPWSTR uri = nullptr;
                sender->get_Source(&uri);
                if (uri) {
                    tt->url = uri;
                    if (tabs_.activeId() == tabId) {
                        syncAddressToUI(uri);
                        updateLockIcon(uri);
                    }
                    CoTaskMemFree(uri);
                }

                LPWSTR title = nullptr;
                sender->get_DocumentTitle(&title);
                if (title && title[0]) {
                    tt->title = title;
                    CoTaskMemFree(title);
                }

                BOOL canBack = FALSE, canForward = FALSE;
                sender->get_CanGoBack(&canBack);
                sender->get_CanGoForward(&canForward);
                if (tabs_.activeId() == tabId) {
                    syncNavStateToUI(canBack, canForward);
                }

                syncTabsToUI();
                return S_OK;
            }).Get(), nullptr);
}

void BrowserWindow::syncTabsToUI() {
    if (!uiWebView_) return;

    ULONGLONG now = GetTickCount64();
    ULONGLONG last = lastSyncMs_.load();
    if (last != 0 && now - last < 50) return;
    lastSyncMs_.store(now);

    std::lock_guard lock(syncMutex_);

    int64_t aid = tabs_.activeId();
    std::wostringstream json;
    json << L"{\"type\":\"updateTabs\",\"activeId\":" << aid
         << L",\"tabs\":[";
    bool first = true;
    for (const auto& t : tabs_.all()) {
        if (!first) json << L",";
        first = false;
        json << L"{\"id\":" << t->id
             << L",\"title\":\"" << escapeJson(t->title) << L"\""
             << L",\"url\":\"" << escapeJson(t->url) << L"\""
             << L",\"loading\":" << (t->loading ? L"true" : L"false")
             << L"}";
    }
    json << L"]}";

    uiWebView_->PostWebMessageAsString(json.str().c_str());
}

void BrowserWindow::syncAddressToUI(const std::wstring& url) {
    if (!uiWebView_) return;
    std::lock_guard lock(syncMutex_);
    std::wostringstream json;
    json << L"{\"type\":\"updateAddress\",\"url\":\"" << escapeJson(url) << L"\"}";
    uiWebView_->PostWebMessageAsString(json.str().c_str());
}

void BrowserWindow::syncNavStateToUI(bool canBack, bool canForward) {
    if (!uiWebView_) return;
    std::lock_guard lock(syncMutex_);
    std::wostringstream json;
    json << L"{\"type\":\"updateNavState\",\"canBack\":"
         << (canBack ? L"true" : L"false")
         << L",\"canForward\":" << (canForward ? L"true" : L"false") << L"}";
    uiWebView_->PostWebMessageAsString(json.str().c_str());
}

void BrowserWindow::updateLockIcon(const std::wstring& url) {
    if (!uiWebView_) return;
    std::wstring icon = L"📄";
    if (url.rfind(L"https://", 0) == 0) icon = L"🔒";
    else if (url.rfind(L"http://", 0) == 0) icon = L"⚠";
    else if (url.rfind(L"dm://", 0) == 0) icon = L"📄";
    std::wostringstream json;
    json << L"{\"type\":\"updateLock\",\"icon\":\"" << icon << L"\"}";
    uiWebView_->PostWebMessageAsString(json.str().c_str());
}

void BrowserWindow::handleUIMessage(const std::wstring& json) {
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
    auto findInt = [&json](const std::wstring& key) -> int64_t {
        auto pos = json.find(L"\"" + key + L"\"");
        if (pos == std::wstring::npos) return 0;
        pos = json.find(L":", pos);
        if (pos == std::wstring::npos) return 0;
        try {
            return std::stoll(json.substr(pos + 1));
        } catch (...) {
            return 0;
        }
    };

    std::wstring type = findType(L"type");
    std::cout << "[C++] type = " << wideToUtf8(type) << "\n";

    if (type == L"newTab") {
        createTab(L"");
    } else if (type == L"closeTab") {
        closeTab(findInt(L"id"));
    } else if (type == L"activateTab") {
        activateTab(findInt(L"id"));
    } else if (type == L"navigate") {
        navigateActive(findType(L"url"));
    } else if (type == L"back") {
        auto t = tabs_.active();
        if (t && t->webview) t->webview->GoBack();
    } else if (type == L"forward") {
        auto t = tabs_.active();
        if (t && t->webview) t->webview->GoForward();
    } else if (type == L"reload") {
        auto t = tabs_.active();
        if (t && t->webview) t->webview->Reload();
    } else if (type == L"home") {
        navigateActive(L"dm://start");
    } else if (type == L"uiReady") {
        SetTimer(hwnd_, 1, 50, nullptr);
    }
}

void BrowserWindow::layout() {
    if (uiController_) {
        RECT b{0, 0, width_, uiHeight_};
        uiController_->put_Bounds(b);
    }
    RECT bounds{0, uiHeight_, width_, height_};
    for (auto& t : tabs_.allMutable()) {
        if (t->controller) t->controller->put_Bounds(bounds);
    }
}

std::wstring BrowserWindow::getActiveUrl() const {
    auto t = const_cast<BrowserWindow*>(this)->tabs_.active();
    return t ? t->url : L"";
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
        case WM_SETFOCUS: {
            if (self && self->uiController_) {
                self->uiController_->MoveFocus(
                    COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
            }
            return 0;
        }
        case WM_TIMER: {
            if (self && wp == 1) {
                KillTimer(hwnd, 1);
                self->syncTabsToUI();
                auto t = self->tabs_.active();
                if (t) self->syncAddressToUI(t->url);
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