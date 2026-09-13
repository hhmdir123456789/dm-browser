#include "ui/BrowserWindow.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <cstdint>
#include <algorithm>
#include <thread>
#include <vector>
#include <utility>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

namespace dm::ui {

static const wchar_t* kClassName = L"DMBrowserWindow";

// ============================================================
// 工具函数
// ============================================================
static std::string wideToUtf8(const std::wstring& w) {
    if (w.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, w.c_str(),
                                   (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string out(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(),
                        (int)w.size(), &out[0], size, nullptr, nullptr);
    return out;
}

static std::wstring utf8ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, s.c_str(),
                                   (int)s.size(), nullptr, 0);
    std::wstring out(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(),
                        (int)s.size(), &out[0], size);
    return out;
}

static std::string getExeDir() {
    char exePath[MAX_PATH];
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    std::string dir = exePath;
    auto pos = dir.find_last_of("\\/");
    if (pos != std::string::npos) dir = dir.substr(0, pos + 1);
    return dir;
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

// ============================================================
// 构造 / 析构
// ============================================================
BrowserWindow::BrowserWindow() = default;

BrowserWindow::~BrowserWindow() {
    bookmarkStore_.reset();
    settings_.reset();
    db_.close();
    if (sidebarController_) sidebarController_->Close();
    if (uiController_) uiController_->Close();
    for (auto& t : tabs_.allMutable()) {
        if (t->controller) t->controller->Close();
    }
    Sleep(100);
    if (hwnd_) DestroyWindow(hwnd_);
}

// ============================================================
// 创建窗口
// ============================================================
bool BrowserWindow::create(const std::wstring& title, int w, int h) {
    width_ = w; height_ = h;

    std::string dbPath = getExeDir() + "dm_ui.db";
    auto r = db_.open(dbPath);
    if (r.isOk()) {
        db_.exec("CREATE TABLE IF NOT EXISTS bookmarks ("
                 "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                 "title TEXT, url TEXT, created_at INTEGER)");
        bookmarkStore_ = std::make_unique<BookmarkStore>(db_);
        settings_ = std::make_unique<SettingsStore>(db_);
        settings_->init();
    } else {
        std::cerr << "[UI] 打开数据库失败: " << r.error().msg << "\n";
    }

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

// ============================================================
// WebView2 环境
// ============================================================
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

                ctrl->put_ZoomFactor(1.0);

                RECT uiBounds{0, 0, width_, uiHeight_};
                ctrl->put_Bounds(uiBounds);
                ctrl->put_IsVisible(TRUE);

                HRESULT hr = uiWebView_->add_WebMessageReceived(
                    Microsoft::WRL::Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                        [this](ICoreWebView2*,
                               ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                            LPWSTR json = nullptr;
                            HRESULT r = args->TryGetWebMessageAsString(&json);
                            if (SUCCEEDED(r) && json) {
                                handleUIMessage(json);
                                CoTaskMemFree(json);
                            } else {
                                LPWSTR raw = nullptr;
                                if (SUCCEEDED(args->get_WebMessageAsJson(&raw)) && raw) {
                                    handleUIMessage(raw);
                                    CoTaskMemFree(raw);
                                }
                            }
                            return S_OK;
                        }).Get(), nullptr);

                if (FAILED(hr)) {
                    std::cerr << "[UI] add_WebMessageReceived 失败\n";
                } else {
                    std::cout << "[UI] WebMessageReceived 注册成功\n";
                }

                std::wstring html = loadUIHtml();
                if (!html.empty()) {
                    std::cout << "[UI] 加载 ui.html (" << html.size() << " 字节)\n";

                    // 用虚拟主机映射，给页面一个 https origin，这样 localStorage 才能用
                    Microsoft::WRL::ComPtr<ICoreWebView2_3> wv3;
                    if (SUCCEEDED(uiWebView_->QueryInterface(IID_PPV_ARGS(&wv3)))) {
                        std::wstring dir = utf8ToWide(getExeDir());
                        wv3->SetVirtualHostNameToFolderMapping(
                            L"dm.local",
                            dir.c_str(),
                            COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);
                        uiWebView_->Navigate(L"https://dm.local/ui.html");
                    } else {
                        uiWebView_->NavigateToString(html.c_str());
                    }
                }

                if (tabs_.count() == 0) createTab(L"");
                initSidebar();
                return S_OK;
            }).Get());
}

// ============================================================
// 侧边栏 WebView2
// ============================================================
void BrowserWindow::initSidebar() {
    env_->CreateCoreWebView2Controller(hwnd_,
        Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
            [this](HRESULT result, ICoreWebView2Controller* ctrl) -> HRESULT {
                if (FAILED(result) || !ctrl) {
                    std::cerr << "[UI] 侧边栏 controller 创建失败\n";
                    return S_OK;
                }
                sidebarController_ = ctrl;
                ctrl->get_CoreWebView2(&sidebarWebView_);

                ctrl->put_ZoomFactor(1.0);

                RECT b{width_ - sidebarWidth_, uiHeight_, width_, height_};
                ctrl->put_Bounds(b);
                ctrl->put_IsVisible(FALSE);

                sidebarWebView_->add_WebMessageReceived(
                    Microsoft::WRL::Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                        [this](ICoreWebView2*,
                               ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                            LPWSTR json = nullptr;
                            if (SUCCEEDED(args->TryGetWebMessageAsString(&json)) && json) {
                                handleSidebarMessage(json);
                                CoTaskMemFree(json);
                            }
                            return S_OK;
                        }).Get(), nullptr);

                std::wstring html = loadSidebarHtml();
                if (!html.empty()) {
                    std::cout << "[UI] 加载 sidebar.html (" << html.size() << " 字节)\n";

                    Microsoft::WRL::ComPtr<ICoreWebView2_3> wv3;
                    if (SUCCEEDED(sidebarWebView_->QueryInterface(IID_PPV_ARGS(&wv3)))) {
                        std::wstring dir = utf8ToWide(getExeDir());
                        wv3->SetVirtualHostNameToFolderMapping(
                            L"dm.local",
                            dir.c_str(),
                            COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);
                        sidebarWebView_->Navigate(L"https://dm.local/sidebar.html");
                    } else {
                        sidebarWebView_->NavigateToString(html.c_str());
                    }
                }
                return S_OK;
            }).Get());
}

// ============================================================
// 文件读取
// ============================================================
std::wstring BrowserWindow::readFileAsWide(const std::string& path) {
    std::lock_guard lock(fileMutex_);
    std::ifstream f(path, std::ios::binary);
    if (!f) return L"";
    std::ostringstream ss;
    ss << f.rdbuf();
    std::string s = ss.str();
    if (s.empty()) return L"";
    return utf8ToWide(s);
}

std::wstring BrowserWindow::loadUIHtml() {
    if (!cachedUIHtml_.empty()) return cachedUIHtml_;
    cachedUIHtml_ = readFileAsWide("ui.html");
    if (cachedUIHtml_.empty()) {
        cachedUIHtml_ = readFileAsWide(getExeDir() + "ui.html");
    }
    return cachedUIHtml_;
}

std::wstring BrowserWindow::loadStartPage() {
    if (!cachedStartPage_.empty()) return cachedStartPage_;
    cachedStartPage_ = readFileAsWide("start_page.html");
    if (cachedStartPage_.empty()) {
        cachedStartPage_ = readFileAsWide(getExeDir() + "start_page.html");
    }
    return cachedStartPage_;
}

std::wstring BrowserWindow::loadSidebarHtml() {
    if (!cachedSidebarHtml_.empty()) return cachedSidebarHtml_;
    cachedSidebarHtml_ = readFileAsWide("sidebar.html");
    if (cachedSidebarHtml_.empty()) {
        cachedSidebarHtml_ = readFileAsWide(getExeDir() + "sidebar.html");
    }
    return cachedSidebarHtml_;
}

// ============================================================
// 创建标签
// ============================================================
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
    syncTabsToUI(true);

    std::wstring target = url;
    bool useStartPage = (url.empty() || url == L"about:blank");

    env_->CreateCoreWebView2Controller(hwnd_,
        Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
            [this, id, target, useStartPage](HRESULT result,
                    ICoreWebView2Controller* ctrl) -> HRESULT {
                if (FAILED(result) || !ctrl) {
                    tabs_.close(id);
                    syncTabsToUI(true);
                    return S_OK;
                }
                auto t = tabs_.get(id);
                if (!t) { ctrl->Close(); return S_OK; }

                t->controller = ctrl;
                ctrl->get_CoreWebView2(&t->webview);
                ctrl->put_ZoomFactor(1.0);

                RECT bounds{0, uiHeight_,
                            sidebarOpen_ ? width_ - sidebarWidth_ : width_,
                            height_};
                ctrl->put_Bounds(bounds);
                ctrl->put_IsVisible(tabs_.activeId() == id ? TRUE : FALSE);

                t->webview->add_WebMessageReceived(
                    Microsoft::WRL::Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                        [this](ICoreWebView2*,
                               ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                            LPWSTR json = nullptr;
                            if (SUCCEEDED(args->TryGetWebMessageAsString(&json)) && json) {
                                std::wstring msg = json;
                                CoTaskMemFree(json);
                                if (msg.find(L"startPageReady") != std::wstring::npos) {
                                    sendBookmarksToContent();
                                }
                            }
                            return S_OK;
                        }).Get(), nullptr);

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

                syncTabsToUI(true);
                applyLayout();
                activateTab(id);
                InvalidateRect(hwnd_, nullptr, TRUE);
                UpdateWindow(hwnd_);
                return S_OK;
            }).Get());
}

// ============================================================
// 关闭 / 激活 / 导航
// ============================================================
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
    syncTabsToUI(true);
}

void BrowserWindow::activateTab(int64_t id) {
    if (!tabs_.activate(id)) return;
    for (auto& t : tabs_.allMutable()) {
        if (t->controller) {
            if (t->id == id) {
                RECT bounds{0, uiHeight_,
                            sidebarOpen_ ? width_ - sidebarWidth_ : width_,
                            height_};
                t->controller->put_Bounds(bounds);
                t->controller->put_IsVisible(TRUE);
            } else {
                t->controller->put_IsVisible(FALSE);
            }
        }
    }
    auto t = tabs_.get(id);
    if (t) {
        syncAddressToUI(t->url);
        updateLockIcon(t->url);
    }
    syncTabsToUI(true);
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
            syncTabsToUI(true);
            InvalidateRect(hwnd_, nullptr, TRUE);
            UpdateWindow(hwnd_);
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
    syncStarStateToUI(false);
    syncTabsToUI(true);



}

// ============================================================
// 导航完成 + favicon
// ============================================================
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
                tt->discarded = false;

                LPWSTR uri = nullptr;
                sender->get_Source(&uri);
                if (uri) {
                    if (wcscmp(uri, L"about:blank") != 0) {
                        tt->url = uri;
                    }
                    if (tabs_.activeId() == tabId) {
                        syncAddressToUI(tt->url);
                        updateLockIcon(tt->url);
                    }
                    CoTaskMemFree(uri);
                }

                LPWSTR title = nullptr;
                sender->get_DocumentTitle(&title);
                if (title && title[0]) {
                    tt->title = title;
                    CoTaskMemFree(title);
                }

                if (settings_ && !tt->url.empty() &&
                    tt->url.rfind(L"dm://", 0) != 0) {
                    settings_->addHistory(wideToUtf8(tt->url),
                                          wideToUtf8(tt->title));
                }

                BOOL canBack = FALSE, canForward = FALSE;
                sender->get_CanGoBack(&canBack);
                sender->get_CanGoForward(&canForward);
                if (tabs_.activeId() == tabId) {
                    syncNavStateToUI(canBack, canForward);
                }

                tabs_.groupByDomain();
                syncTabsToUI();
                return S_OK;
            }).Get(), nullptr);

    // favicon 监听（ICoreWebView2_15）
    Microsoft::WRL::ComPtr<ICoreWebView2_15> wv2;
    if (SUCCEEDED(t->webview.As(&wv2))) {
        wv2->add_FaviconChanged(
            Microsoft::WRL::Callback<ICoreWebView2FaviconChangedEventHandler>(
                [this, tabId](ICoreWebView2* sender, IUnknown*) -> HRESULT {
                    auto tt = tabs_.get(tabId);
                    if (!tt) return S_OK;

                    Microsoft::WRL::ComPtr<ICoreWebView2_15> s2;
                    if (FAILED(sender->QueryInterface(IID_PPV_ARGS(&s2)))) return S_OK;

                    s2->GetFavicon(
                        COREWEBVIEW2_FAVICON_IMAGE_FORMAT_PNG,
                        Microsoft::WRL::Callback<ICoreWebView2GetFaviconCompletedHandler>(
                            [this, tabId](HRESULT result, IStream* stream) -> HRESULT {
                                if (FAILED(result) || !stream) return S_OK;
                                auto tt = tabs_.get(tabId);
                                if (!tt) return S_OK;

                                Microsoft::WRL::ComPtr<IStream> spStream(stream);
                                LARGE_INTEGER zero{};
                                spStream->Seek(zero, STREAM_SEEK_SET, nullptr);

                                std::vector<BYTE> buf(65536);
                                ULONG read = 0;
                                spStream->Read(buf.data(), (ULONG)buf.size(), &read);
                                if (read == 0) return S_OK;
                                if (read > 8192) read = 8192;
                                buf.resize(read);

                                static const wchar_t* kTable =
                                    L"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                    L"abcdefghijklmnopqrstuvwxyz"
                                    L"0123456789+/";
                                std::wstring b64;
                                b64.reserve(((read + 2) / 3) * 4);
                                for (size_t i = 0; i < read; i += 3) {
                                    uint32_t n = buf[i] << 16;
                                    if (i + 1 < read) n |= buf[i + 1] << 8;
                                    if (i + 2 < read) n |= buf[i + 2];
                                    b64 += kTable[(n >> 18) & 63];
                                    b64 += kTable[(n >> 12) & 63];
                                    b64 += (i + 1 < read) ? kTable[(n >> 6) & 63] : L'=';
                                    b64 += (i + 2 < read) ? kTable[n & 63] : L'=';
                                }

                                tt->faviconUrl = L"data:image/png;base64," + b64;
                                syncTabsToUI();
                                return S_OK;
                            }).Get());
                    return S_OK;
                }).Get(), nullptr);
    }
}

// ============================================================
// UI 同步
// ============================================================
void BrowserWindow::syncTabsToUI(bool force) {
    if (!uiWebView_) return;

    ULONGLONG now = GetTickCount64();
    ULONGLONG last = lastSyncMs_.load();
    if (!force && last != 0 && now - last < 200) return;
    lastSyncMs_.store(now);

    std::lock_guard lock(syncMutex_);

    int64_t aid = tabs_.activeId();
    std::wostringstream json;
    json << L"{\"type\":\"updateTabs\",\"activeId\":" << aid << L",\"tabs\":[";
    bool first = true;
    for (const auto& t : tabs_.all()) {
        if (!first) json << L",";
        first = false;
        std::wstring fav = t->faviconUrl;
        if (fav.size() > 16000) fav = L"";
        json << L"{\"id\":" << t->id
             << L",\"title\":\"" << escapeJson(t->title) << L"\""
             << L",\"url\":\"" << escapeJson(t->url) << L"\""
             << L",\"favicon\":\"" << escapeJson(fav) << L"\""
             << L",\"pinned\":" << (t->pinned ? L"true" : L"false")
             << L",\"discarded\":" << (t->discarded ? L"true" : L"false")
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

void BrowserWindow::syncStarStateToUI(bool starred) {
    if (!uiWebView_) return;
    std::lock_guard lock(syncMutex_);
    std::wostringstream json;
    json << L"{\"type\":\"updateStar\",\"starred\":"
         << (starred ? L"true" : L"false") << L"}";
    uiWebView_->PostWebMessageAsString(json.str().c_str());
}

void BrowserWindow::sendBookmarksToContent() {
    if (!bookmarkStore_) return;

    std::wostringstream json;
    json << L"{\"type\":\"updateBookmarks\",\"items\":[";
    auto items = bookmarkStore_->list();
    bool first = true;
    for (const auto& b : items) {
        if (!first) json << L",";
        first = false;
        json << L"{\"title\":\"" << escapeJson(utf8ToWide(b.title)) << L"\""
             << L",\"url\":\"" << escapeJson(utf8ToWide(b.url)) << L"\"}";
    }
    json << L"]}";

    auto tabs = tabs_.all();
    for (auto& t : tabs) {
        if (t->webview && t->url == L"dm://start") {
            t->webview->PostWebMessageAsString(json.str().c_str());
        }
    }
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

// ============================================================
// UI 消息处理
// ============================================================
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
    // 解析 JSON 布尔值：{"key":true} / {"key":false}
    auto findBool = [&json](const std::wstring& key) -> bool {
        auto pos = json.find(L"\"" + key + L"\"");
        if (pos == std::wstring::npos) return false;
        pos = json.find(L":", pos);
        if (pos == std::wstring::npos) return false;
        pos++;
        while (pos < json.size() &&
               (json[pos] == L' ' || json[pos] == L'\t')) pos++;
        return json.substr(pos, 4) == L"true";
    };
    auto findInt = [&json](const std::wstring& key) -> int64_t {
        auto pos = json.find(L"\"" + key + L"\"");
        if (pos == std::wstring::npos) return 0;
        pos = json.find(L":", pos);
        if (pos == std::wstring::npos) return 0;
        pos++;
        while (pos < json.size() &&
               (json[pos] == L' ' || json[pos] == L'\t')) pos++;
        size_t end = pos;
        while (end < json.size() &&
               (json[end] == L'-' || (json[end] >= L'0' && json[end] <= L'9'))) end++;
        if (end == pos) return 0;
        try {
            return std::stoll(json.substr(pos, end - pos));
        } catch (...) {
            return 0;
        }
    };

    std::wstring type = findType(L"type");
    if (type.empty()) return;

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
    } else if (type == L"star") {
        auto t = tabs_.active();
        if (t && bookmarkStore_) {
            std::string title = wideToUtf8(t->title);
            if (title.empty()) title = wideToUtf8(t->url);
            auto r = bookmarkStore_->add(title, wideToUtf8(t->url));
            if (r.isOk()) {
                syncStarStateToUI(true);
                sendBookmarksToContent();
            }
        }
    } else if (type == L"tabContextMenu") {
        closeTab(findInt(L"id"));
    } else if (type == L"pageContextMenu") {
        auto t = tabs_.active();
        if (t && t->webview) t->webview->Reload();
    } else if (type == L"uiReady") {
        SetTimer(hwnd_, 1, 50, nullptr);
    } else if (type == L"toggleSidebar") {
        onToggleSidebar(findBool(L"open"));
    } else if (type == L"setUIMode") {
        std::wstring mode = findType(L"mode");
        if (settings_) {
            settings_->set("ui_mode", wideToUtf8(mode));
        }
    } else if (type == L"checkOllama") {
        onCheckOllama();
    } else if (type == L"cmdPalette") {
        bool open = findBool(L"open");
        if (uiController_) {
            if (open) {
                RECT b{0, 0, width_, height_};
                uiController_->put_Bounds(b);
                for (auto& t : tabs_.allMutable()) {
                    if (t->controller) t->controller->put_IsVisible(FALSE);
                }
            } else {
                applyLayout();
                InvalidateRect(hwnd_, nullptr, TRUE);
                UpdateWindow(hwnd_);
            }
        }
    }
}

// ============================================================
// 侧边栏消息处理
// ============================================================
void BrowserWindow::handleSidebarMessage(const std::wstring& json) {
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

    std::wstring type = findType(L"type");
    if (type.empty()) return;

    if (type == L"aiChat") onAiChat(json);
    else if (type == L"checkOllama") onCheckOllama();
    else if (type == L"loadAudit") onLoadAudit();
    else if (type == L"loadHistory") onLoadHistory();
    else if (type == L"toggleSidebar") onToggleSidebar(false);
    else if (type == L"navigate") {
        std::wstring url = findType(L"url");
        if (!url.empty()) navigateActive(url);
    }
}

void BrowserWindow::onToggleSidebar(bool open) {
    sidebarOpen_ = open;
    applyLayout();

    if (uiWebView_) {
        std::wostringstream json;
        json << L"{\"type\":\"sidebarState\",\"open\":"
             << (open ? L"true" : L"false") << L"}";
        uiWebView_->PostWebMessageAsString(json.str().c_str());
    }
}

// ============================================================
// AI 对话
// ============================================================
void BrowserWindow::onAiChat(const std::wstring& json) {
    auto findStr = [&json](const std::wstring& key) -> std::wstring {
        auto pos = json.find(L"\"" + key + L"\"");
        if (pos == std::wstring::npos) return L"";
        pos = json.find(L":", pos);
        if (pos == std::wstring::npos) return L"";
        pos = json.find(L"\"", pos);
        if (pos == std::wstring::npos) return L"";
        auto end = pos + 1;
        while (end < json.size()) {
            if (json[end] == L'"' && json[end-1] != L'\\') break;
            end++;
        }
        return json.substr(pos + 1, end - pos - 1);
    };

    std::wstring requestId = findStr(L"requestId");
    std::wstring endpoint  = findStr(L"endpoint");
    std::wstring model     = findStr(L"model");
    std::wstring message   = findStr(L"message");

    std::string ep  = wideToUtf8(endpoint);
    std::string md  = wideToUtf8(model);
    std::string msg = wideToUtf8(message);

    auto self = this;
    std::thread([self, requestId, ep, md, msg]() {
        AiConfig cfg;
        cfg.endpoint = ep;
        cfg.model    = md;
        self->aiClient_.setConfig(cfg);

        std::string full;

        self->aiClient_.chatStream({}, msg,
            [self, requestId, &full](const std::string& chunk) {
                full += chunk;
                std::wstring wc = utf8ToWide(chunk);
                auto* pair = new std::pair<std::wstring, std::wstring>(requestId, wc);
                if (!PostMessageW(self->hwnd_, WM_AI_CHUNK, 0, (LPARAM)pair)) {
                    delete pair;
                }
            },
            [self, requestId](const std::string& err) {
                std::wstring we = utf8ToWide(err);
                auto* pair = new std::pair<std::wstring, std::wstring>(requestId, we);
                if (!PostMessageW(self->hwnd_, WM_AI_ERROR, 0, (LPARAM)pair)) {
                    delete pair;
                }
            });

        std::wstring wfull = utf8ToWide(full);
        auto* pair = new std::pair<std::wstring, std::wstring>(requestId, wfull);
        if (!PostMessageW(self->hwnd_, WM_AI_DONE, 0, (LPARAM)pair)) {
            delete pair;
        }
    }).detach();
}

// ============================================================
// Ollama 检测（异步，避免阻塞 UI）
// ============================================================
void BrowserWindow::onCheckOllama() {
    std::cout << "[C++] onCheckOllama 被调用\n";
    // 立即发"检测中"状态
    if (sidebarWebView_) {
        sidebarWebView_->PostWebMessageAsString(
            L"{\"type\":\"ollamaStatus\",\"ok\":false,\"status\":\"检测中...\"}");
    }

    auto self = this;
    std::thread([self]() {
        auto models = self->aiClient_.listModels();
        auto* payload = new std::vector<std::string>(std::move(models));
        if (!PostMessageW(self->hwnd_, WM_OLLAMA_RESULT, 0, (LPARAM)payload)) {
            delete payload;
        }
    }).detach();
}

void BrowserWindow::onLoadAudit() {
    std::wostringstream out;
    out << L"{\"type\":\"auditList\",\"items\":[]}";
    if (sidebarWebView_)
        sidebarWebView_->PostWebMessageAsString(out.str().c_str());
}

void BrowserWindow::onLoadHistory() {
    if (!settings_) return;
    auto items = settings_->recentHistory(50);
    std::wostringstream out;
    out << L"{\"type\":\"historyList\",\"items\":[";
    bool first = true;
    for (auto& it : items) {
        if (!first) out << L",";
        first = false;
        std::wstring url   = utf8ToWide(it.url);
        std::wstring title = utf8ToWide(it.title);
        out << L"{\"url\":\"" << escapeJson(url)
            << L"\",\"title\":\"" << escapeJson(title) << L"\"}";
    }
    out << L"]}";
    if (sidebarWebView_)
        sidebarWebView_->PostWebMessageAsString(out.str().c_str());
}

// ============================================================
// 布局
// ============================================================
void BrowserWindow::applyLayout() {
    if (uiController_) {
        RECT b{0, 0, width_, uiHeight_};
        uiController_->put_Bounds(b);
    }
    if (sidebarController_) {
        if (sidebarOpen_) {
            RECT b{width_ - sidebarWidth_, uiHeight_, width_, height_};
            sidebarController_->put_Bounds(b);
            sidebarController_->put_IsVisible(TRUE);
        } else {
            sidebarController_->put_IsVisible(FALSE);
        }
    }
    RECT bounds{0, uiHeight_,
                sidebarOpen_ ? width_ - sidebarWidth_ : width_,
                height_};
    for (auto& t : tabs_.allMutable()) {
        if (t->controller) t->controller->put_Bounds(bounds);
    }

}

void BrowserWindow::layout() { applyLayout(); }

std::wstring BrowserWindow::getActiveUrl() const {
    auto t = const_cast<BrowserWindow*>(this)->tabs_.active();
    return t ? t->url : L"";
}

// ============================================================
// 窗口过程
// ============================================================
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
                self->applyLayout();
            }
            return 0;
        }
        case WM_TIMER: {
            if (self && wp == 1) {
                KillTimer(hwnd, 1);
                self->syncTabsToUI(true);
                auto t = self->tabs_.active();
                if (t) self->syncAddressToUI(t->url);
            }
            return 0;
        }
        // ---- AI 消息封送（后台线程 → 主线程）----
        case WM_AI_CHUNK: {
            auto* pair = (std::pair<std::wstring, std::wstring>*)lp;
            if (self && self->sidebarWebView_ && pair) {
                std::wostringstream out;
                out << L"{\"type\":\"aiChunk\",\"requestId\":\""
                    << pair->first << L"\",\"content\":\""
                    << escapeJson(pair->second) << L"\"}";
                self->sidebarWebView_->PostWebMessageAsString(out.str().c_str());
            }
            delete pair;
            return 0;
        }
        case WM_AI_ERROR: {
            auto* pair = (std::pair<std::wstring, std::wstring>*)lp;
            if (self && self->sidebarWebView_ && pair) {
                std::wostringstream out;
                out << L"{\"type\":\"aiError\",\"requestId\":\""
                    << pair->first << L"\",\"error\":\""
                    << escapeJson(pair->second) << L"\"}";
                self->sidebarWebView_->PostWebMessageAsString(out.str().c_str());
            }
            delete pair;
            return 0;
        }
        case WM_AI_DONE: {
            auto* pair = (std::pair<std::wstring, std::wstring>*)lp;
            if (self && self->sidebarWebView_ && pair) {
                std::wostringstream out;
                out << L"{\"type\":\"aiDone\",\"requestId\":\""
                    << pair->first << L"\",\"full\":\""
                    << escapeJson(pair->second) << L"\"}";
                self->sidebarWebView_->PostWebMessageAsString(out.str().c_str());
            }
            delete pair;
            return 0;
        }
        // ---- Ollama 检测结果封送 ----
        case WM_OLLAMA_RESULT: {
            std::cout << "[C++] WM_OLLAMA_RESULT 收到\n";
            auto* models = (std::vector<std::string>*)lp;
            if (self && self->sidebarWebView_ && models) {
                std::wostringstream out;
                out << L"{\"type\":\"ollamaStatus\",\"ok\":"
                    << (models->empty() ? L"false" : L"true")
                    << L",\"status\":\""
                    << (models->empty() ? L"未检测到 (请运行 ollama serve)" : L"就绪")
                    << L"\"}";
                self->sidebarWebView_->PostWebMessageAsString(out.str().c_str());

                std::wostringstream ml;
                ml << L"{\"type\":\"modelList\",\"models\":[";
                bool first = true;
                for (auto& m : *models) {
                    if (!first) ml << L",";
                    first = false;
                    ml << L"\"" << escapeJson(utf8ToWide(m)) << L"\"";
                }
                ml << L"]}";
                self->sidebarWebView_->PostWebMessageAsString(ml.str().c_str());
            }
            delete models;
            return 0;
        }
        case WM_KEYDOWN: {
            if (!self) break;
            bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            bool alt  = (GetKeyState(VK_MENU)    & 0x8000) != 0;
            bool shift= (GetKeyState(VK_SHIFT)   & 0x8000) != 0;

            if (ctrl && !shift && wp == 'T') {
                self->createTab(L"");
                return 0;
            }
            if (ctrl && !shift && wp == 'W') {
                self->closeTab(self->tabs_.activeId());
                return 0;
            }
            if (ctrl && !shift && wp == 'L') {
                if (self->uiWebView_) {
                    self->uiWebView_->PostWebMessageAsString(
                        L"{\"type\":\"focusAddress\"}");
                }
                return 0;
            }
            if (ctrl && !shift && wp == 'R') {
                auto t = self->tabs_.active();
                if (t && t->webview) t->webview->Reload();
                return 0;
            }
            if (wp == VK_F5) {
                auto t = self->tabs_.active();
                if (t && t->webview) t->webview->Reload();
                return 0;
            }
            if (alt && wp == VK_LEFT) {
                auto t = self->tabs_.active();
                if (t && t->webview) t->webview->GoBack();
                return 0;
            }
            if (alt && wp == VK_RIGHT) {
                auto t = self->tabs_.active();
                if (t && t->webview) t->webview->GoForward();
                return 0;
            }
            if (wp == VK_F12) {
                auto t = self->tabs_.active();
                if (t && t->webview) t->webview->OpenDevToolsWindow();
                return 0;
            }
            break;
        }
        case WM_CLOSE:
            break;
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