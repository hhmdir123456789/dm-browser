#include "ui/BrowserWindow.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <cstdint>
#include <algorithm>

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

BrowserWindow::BrowserWindow() = default;

BrowserWindow::~BrowserWindow() {
    bookmarkStore_.reset();
    db_.close();
    if (uiController_) uiController_->Close();
    for (auto& t : tabs_.allMutable()) {
        if (t->controller) t->controller->Close();
    }
    Sleep(100);
    if (hwnd_) DestroyWindow(hwnd_);
}

bool BrowserWindow::create(const std::wstring& title, int w, int h) {
    width_ = w; height_ = h;

    std::string dbPath = getExeDir() + "dm_ui.db";
    auto r = db_.open(dbPath);
    if (r.isOk()) {
        db_.exec("CREATE TABLE IF NOT EXISTS bookmarks ("
                 "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                 "title TEXT, url TEXT, created_at INTEGER)");
        bookmarkStore_ = std::make_unique<BookmarkStore>(db_);
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
                    uiWebView_->NavigateToString(html.c_str());
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

                RECT bounds{0, uiHeight_, width_, height_};
                ctrl->put_Bounds(bounds);
                ctrl->put_IsVisible(tabs_.activeId() == id ? TRUE : FALSE);

                // 内容层消息接收
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
                layout();
                activateTab(id);                              // ← 新增：重新激活，确保可见
                InvalidateRect(hwnd_, nullptr, TRUE);         // ← 新增：强制重绘
                UpdateWindow(hwnd_);                          // ← 新增：立即刷新
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
    syncTabsToUI(true);
}

void BrowserWindow::activateTab(int64_t id) {
    if (!tabs_.activate(id)) return;
    for (auto& t : tabs_.allMutable()) {
        if (t->controller) {
            if (t->id == id) {
                RECT bounds{0, uiHeight_, width_, height_};
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

                BOOL canBack = FALSE, canForward = FALSE;
                sender->get_CanGoBack(&canBack);
                sender->get_CanGoForward(&canForward);
                if (tabs_.activeId() == tabId) {
                    syncNavStateToUI(canBack, canForward);
                }

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

void BrowserWindow::syncTabsToUI(bool force) {
    std::cout << "[C++] syncTabsToUI 被调用, force=" << force
              << ", uiWebView_=" << (uiWebView_ ? "yes" : "null") << "\n";
    if (!uiWebView_) return;

    ULONGLONG now = GetTickCount64();
    ULONGLONG last = lastSyncMs_.load();
    if (!force && last != 0 && now - last < 200) {
        std::cout << "[C++] syncTabsToUI 被节流跳过\n";
        return;
    }
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
             << L",\"loading\":" << (t->loading ? L"true" : L"false")
             << L"}";
    }
    json << L"]}";

    std::wcout << L"[C++] syncTabsToUI 发送: " << json.str() << L"\n";
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

void BrowserWindow::handleUIMessage(const std::wstring& json) {
    std::wcout << L"[C++] 收到 UI 消息: " << json << L"\n";

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
        pos++;
        while (pos < json.size() && (json[pos] == L' ' || json[pos] == L'\t')) pos++;
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
    if (type.empty()) {
        std::cout << "[C++] type 为空, 忽略\n";
        return;
    }
    std::wcout << L"[C++] type = " << type << L"\n";

    if (type == L"newTab") {
        std::cout << "[C++] 执行 createTab\n";
        createTab(L"");
        std::cout << "[C++] createTab 返回, tabs_.count()=" << tabs_.count() << "\n";
    } else if (type == L"closeTab") {
        int64_t id = findInt(L"id");
        std::cout << "[C++] closeTab id=" << id << "\n";
        closeTab(id);
    } else if (type == L"activateTab") {
        int64_t id = findInt(L"id");
        std::cout << "[C++] activateTab id=" << id << "\n";
        activateTab(id);
    } else if (type == L"navigate") {
        std::wstring url = findType(L"url");
        std::wcout << L"[C++] navigate url=" << url << L"\n";
        auto t = tabs_.active();
        std::cout << "[C++] active tab=" << (t ? t->id : -1)
                  << ", webview=" << (t && t->webview ? "yes" : "null") << "\n";
        navigateActive(url);
    } else if (type == L"back") {
        auto t = tabs_.active();
        std::cout << "[C++] back, active=" << (t ? t->id : -1)
                  << ", webview=" << (t && t->webview ? "yes" : "null") << "\n";
        if (t && t->webview) t->webview->GoBack();
    } else if (type == L"forward") {
        auto t = tabs_.active();
        std::cout << "[C++] forward, active=" << (t ? t->id : -1)
                  << ", webview=" << (t && t->webview ? "yes" : "null") << "\n";
        if (t && t->webview) t->webview->GoForward();
    } else if (type == L"reload") {
        auto t = tabs_.active();
        std::cout << "[C++] reload, active=" << (t ? t->id : -1)
                  << ", webview=" << (t && t->webview ? "yes" : "null") << "\n";
        if (t && t->webview) t->webview->Reload();
    } else if (type == L"home") {
        std::cout << "[C++] home\n";
        navigateActive(L"dm://start");
    } else if (type == L"star") {
        auto t = tabs_.active();
        std::cout << "[C++] star, active=" << (t ? t->id : -1) << "\n";
        if (t && bookmarkStore_) {
            std::string title = wideToUtf8(t->title);
            if (title.empty()) title = wideToUtf8(t->url);
            auto r = bookmarkStore_->add(title, wideToUtf8(t->url));
            if (r.isOk()) {
                std::cout << "[C++] 收藏成功\n";
                syncStarStateToUI(true);
                sendBookmarksToContent();
            } else {
                std::cout << "[C++] 收藏失败\n";
            }
        }
    } else if (type == L"tabContextMenu") {
        closeTab(findInt(L"id"));
    } else if (type == L"pageContextMenu") {
        auto t = tabs_.active();
        if (t && t->webview) t->webview->Reload();
    } else if (type == L"uiReady") {
        std::cout << "[C++] uiReady, 启动定时器\n";
        SetTimer(hwnd_, 1, 50, nullptr);
    } else {
        std::wcout << L"[C++] 未知 type: " << type << L"\n";
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
        case WM_TIMER: {
            if (self && wp == 1) {
                KillTimer(hwnd, 1);
                self->syncTabsToUI(true);
                auto t = self->tabs_.active();
                if (t) self->syncAddressToUI(t->url);
            }
            return 0;
        }
        case WM_CLOSE:
            std::cout << "[UI] WM_CLOSE 收到\n";
            break;   // 继续走 DefWindowProc，触发 WM_DESTROY
        case WM_DESTROY:
            std::cout << "[UI] WM_DESTROY 收到, 窗口将被销毁\n";
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
    std::cout << "[UI] 消息循环退出, msg=" << msg.message
              << ", wParam=" << msg.wParam << "\n";
    return (int)msg.wParam;
}

} // namespace dm::ui