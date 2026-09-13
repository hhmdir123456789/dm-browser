#include "ui/BrowserWindow.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <cstdint>
#include <algorithm>
#include <thread>
#include <vector>
#include <utility>
#include <set>
#include "learn/snapshot_parser.h"

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
// 批 4A：学习库分析辅助
// ============================================================

// 把 ExecuteScript 返回的 JSON 字符串字面量解引号
static std::string unquoteJsonString(const std::wstring& w) {
    std::string s = wideToUtf8(w);
    if (s.size() < 2 || s.front() != '"' || s.back() != '"') return s;
    std::string out;
    out.reserve(s.size());
    for (size_t i = 1; i + 1 < s.size(); ++i) {
        char c = s[i];
        if (c == '\\' && i + 2 < s.size()) {
            char e = s[++i];
            switch (e) {
                case 'n': out += '\n'; break;
                case 't': out += '\t'; break;
                case 'r': out += '\r'; break;
                case '"': out += '"';  break;
                case '\\': out += '\\'; break;
                case '/': out += '/';  break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'u': {
                    if (i + 4 < s.size()) {
                        unsigned int cp = 0;
                        for (int k = 0; k < 4; ++k) {
                            char h = s[i + 1 + k];
                            cp <<= 4;
                            if (h >= '0' && h <= '9') cp |= (h - '0');
                            else if (h >= 'a' && h <= 'f') cp |= (h - 'a' + 10);
                            else if (h >= 'A' && h <= 'F') cp |= (h - 'A' + 10);
                        }
                        i += 4;
                        if (cp < 0x80) out += (char)cp;
                        else if (cp < 0x800) {
                            out += (char)(0xC0 | (cp >> 6));
                            out += (char)(0x80 | (cp & 0x3F));
                        } else {
                            out += (char)(0xE0 | (cp >> 12));
                            out += (char)(0x80 | ((cp >> 6) & 0x3F));
                            out += (char)(0x80 | (cp & 0x3F));
                        }
                    }
                    break;
                }
                default: out += e; break;
            }
        } else {
            out += c;
        }
    }
    return out;
}

// 内置参考快照（跟 dm_learn dump 一致）
static dm::learn::PageSnapshot makeDemoRefSnapshot() {
    using namespace dm::learn;
    PageSnapshot s;
    s.url = "https://demo.local/";
    s.title = "Demo";
    s.viewportWidth = 800;
    s.viewportHeight = 600;

    NodeSnapshot n1;
    n1.path = "html>body>div.0";
    n1.tag = "div";
    n1.className = "container";
    n1.style.display = "flex";
    n1.style.color = "0,0,0";
    n1.style.fontSize = "16px";
    n1.style.backgroundColor = "255,255,255";
    n1.layout = {0, 0, 800, 100};
    s.nodes.push_back(n1);

    NodeSnapshot n2;
    n2.path = "html>body>div.0>p.0";
    n2.tag = "p";
    n2.textPreview = "Hello";
    n2.style.display = "block";
    n2.style.color = "0,0,0";
    n2.style.fontSize = "16px";
    n2.layout = {10, 10, 100, 20};
    s.nodes.push_back(n2);

    return s;
}

// 页面快照采集脚本（内联）
static const wchar_t* kCollectSnapshotScript = LR"JSS(
(function () {
  function getDepth(el) {
    let d = 0, cur = el.parentElement;
    while (cur) { d++; cur = cur.parentElement; }
    return d;
  }
  function getPath(el) {
    const parts = [];
    let cur = el;
    while (cur && cur.nodeType === 1) {
      const tag = cur.tagName.toLowerCase();
      const parent = cur.parentElement;
      if (parent) {
        let idx = 0;
        for (const sib of parent.children) {
          if (sib === cur) break;
          if (sib.tagName === cur.tagName) idx++;
        }
        parts.unshift(tag + '.' + idx);
      } else {
        parts.unshift(tag);
      }
      cur = parent;
    }
    return parts.join('>');
  }
  function normColor(s) {
    if (!s) return '';
    const m = s.match(/rgba?\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)(?:\s*,\s*([\d.]+))?\s*\)/);
    if (m) {
      return (m[4] !== undefined && parseFloat(m[4]) < 1)
        ? m[1] + ',' + m[2] + ',' + m[3] + ',' + Math.round(parseFloat(m[4]) * 255)
        : m[1] + ',' + m[2] + ',' + m[3];
    }
    return s;
  }
  function px(v) { const n = parseFloat(v); return isNaN(n) ? 0 : Math.round(n); }
  const nodes = [];
  function collect(el) {
    if (el.nodeType !== 1) return;
    const cs = getComputedStyle(el);
    const rc = el.getBoundingClientRect();
    nodes.push({
      path: getPath(el),
      tag: el.tagName.toLowerCase(),
      id: el.id || '',
      className: (typeof el.className === 'string') ? el.className : '',
      depth: getDepth(el),
      childCount: el.children.length,
      textPreview: (el.textContent || '').trim().slice(0, 80),
      alt: el.getAttribute('alt') || '',
      ariaLabel: el.getAttribute('aria-label') || '',
      role: el.getAttribute('role') || '',
      style: {
        color: normColor(cs.color),
        backgroundColor: normColor(cs.backgroundColor),
        fontSize: cs.fontSize || '',
        fontWeight: cs.fontWeight || '',
        display: cs.display || '',
        position: cs.position || '',
        textAlign: cs.textAlign || '',
        marginTop: px(cs.marginTop),
        marginBottom: px(cs.marginBottom),
        marginLeft: px(cs.marginLeft),
        marginRight: px(cs.marginRight),
        paddingTop: px(cs.paddingTop),
        paddingBottom: px(cs.paddingBottom),
        paddingLeft: px(cs.paddingLeft),
        paddingRight: px(cs.paddingRight),
        borderWidth: px(cs.borderWidth),
        borderStyle: cs.borderStyle || ''
      },
      layout: { x: rc.left, y: rc.top, w: rc.width, h: rc.height }
    });
    for (const c of el.children) collect(c);
  }
  if (!document.body) return '';
  collect(document.body);
  const maxDepth = nodes.reduce((m, n) => Math.max(m, n.depth), 0);
  const snapshot = {
    url: location.href,
    title: document.title || '',
    viewportWidth: window.innerWidth,
    viewportHeight: window.innerHeight,
    totalNodes: nodes.length,
    maxDepth: maxDepth,
    imageCount: document.images.length,
    scriptCount: document.scripts.length,
    styleSheetCount: document.styleSheets.length,
    nodes: nodes
  };
  return JSON.stringify(snapshot);
})();
)JSS";

// ============================================================
// 构造 / 析构
// ============================================================
BrowserWindow::BrowserWindow() = default;

BrowserWindow::~BrowserWindow() {
    bookmarkStore_.reset();
    settings_.reset();
    learnStore_.reset();
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

    std::string dbPath = getExeDir() + "dm_browser.db";
    auto r = db_.open(dbPath);
    if (r.isOk()) {
        db_.exec("CREATE TABLE IF NOT EXISTS bookmarks ("
                 "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                 "title TEXT, url TEXT, created_at INTEGER)");
        bookmarkStore_ = std::make_unique<BookmarkStore>(db_);
        settings_ = std::make_unique<SettingsStore>(db_);
        settings_->init();
        learnStore_ = std::make_unique<dm::learn::LearnStore>(db_);
        if (!learnStore_->init()) {
            std::cerr << "[UI] 学习库初始化失败\n";
        }
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
    else if (type == L"analyzePage") onAnalyzePage();
    else if (type == L"loadFeatures") onLoadFeatures();
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
// Ollama 检测
// ============================================================
void BrowserWindow::onCheckOllama() {
    std::cout << "[C++] onCheckOllama 被调用\n";
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
// 批 4A：学习库分析
// ============================================================
void BrowserWindow::onAnalyzePage() {
    auto t = tabs_.active();
    if (!t || !t->webview) {
        sendAnalyzeError("没有活动标签页");
        return;
    }
    if (t->url.rfind(L"dm://", 0) == 0 ||
        t->url.rfind(L"about:", 0) == 0) {
        sendAnalyzeError("内置页面（新标签页 / about:）无法采集快照");
        return;
    }

    auto self = this;
    t->webview->ExecuteScript(
        kCollectSnapshotScript,
        Microsoft::WRL::Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
            [self](HRESULT hr, LPCWSTR resultJson) -> HRESULT {
                if (FAILED(hr) || !resultJson) {
                    self->sendAnalyzeError("脚本执行失败");
                    return S_OK;
                }

                std::string json = unquoteJsonString(resultJson);
                if (json.empty()) {
                    self->sendAnalyzeError("页面快照为空");
                    return S_OK;
                }

                auto dm = dm::learn::parseSnapshotJson(json);
                if (dm.nodes.empty()) {
                    self->sendAnalyzeError("快照解析失败（节点数 0）");
                    return S_OK;
                }

                auto ref = makeDemoRefSnapshot();
                auto result = dm::learn::MultiCompare::compare(ref, dm);

                std::set<std::string> feats;
                for (auto& d : result.diffs) {
                    if (d.dimension == "style") {
                        if (d.property == "display") {
                            if (d.refValue.find("flex") != std::string::npos)
                                feats.insert("display:flex");
                            else if (d.refValue.find("grid") != std::string::npos)
                                feats.insert("display:grid");
                            else if (d.refValue.find("inline-block") != std::string::npos)
                                feats.insert("display:inline-block");
                        }
                        if (d.property == "position") {
                            if (d.refValue == "absolute") feats.insert("position:absolute");
                            if (d.refValue == "fixed")    feats.insert("position:fixed");
                            if (d.refValue == "relative") feats.insert("position:relative");
                        }
                        if (d.property == "fontSize") feats.insert("font-size");
                        if (d.property == "color")    feats.insert("color");
                        if (d.property == "backgroundColor")
                            feats.insert("background-color");
                    }
                    if (d.dimension == "layout") {
                        if (d.property == "width")  feats.insert("width:auto");
                        if (d.property == "height") feats.insert("height:auto");
                    }
                    if (d.dimension == "structural" &&
                        d.category == "missing_node") {
                        feats.insert("node:" + d.refValue);
                    }
                }

                if (self->learnStore_) {
                    for (auto& f : feats) {
                        self->learnStore_->recordFeature(f, false);
                    }
                    self->learnStore_->recalcPriorities();
                }

                self->sendAnalyzeResult(result,
                    std::vector<std::string>(feats.begin(), feats.end()));
                return S_OK;
            }).Get());
}

void BrowserWindow::onLoadFeatures() {
    if (!sidebarWebView_) return;

    std::wostringstream out;
    out << L"{\"type\":\"featuresList\",\"items\":[";

    if (learnStore_) {
        auto items = learnStore_->listFeaturesByPriority();
        bool first = true;
        for (auto& f : items) {
            if (!first) out << L",";
            first = false;
            out << L"{\"feature\":\"" << escapeJson(utf8ToWide(f.feature)) << L"\",";
            out << L"\"tested\":"   << f.tested   << L",";
            out << L"\"passed\":"   << f.passed   << L",";
            out << L"\"failed\":"   << f.failed   << L",";
            out << L"\"priority\":" << f.priority << L"}";
        }
    }
    out << L"]}";
    sidebarWebView_->PostWebMessageAsString(out.str().c_str());
}

void BrowserWindow::sendAnalyzeResult(
    const dm::learn::MultiDimResult& result,
    const std::vector<std::string>& inferred) {
    if (!sidebarWebView_) return;

    std::wostringstream out;
    out << L"{\"type\":\"analyzeResult\",\"result\":{";
    out << L"\"structuralScore\":" << result.structuralScore << L",";
    out << L"\"styleScore\":"      << result.styleScore      << L",";
    out << L"\"layoutScore\":"     << result.layoutScore     << L",";
    out << L"\"structuralDiffCount\":" << result.structuralDiffCount << L",";
    out << L"\"styleDiffCount\":"      << result.styleDiffCount      << L",";
    out << L"\"layoutDiffCount\":"     << result.layoutDiffCount     << L",";

    out << L"\"diffs\":[";
    bool first = true;
    for (auto& d : result.diffs) {
        if (!first) out << L",";
        first = false;
        out << L"{\"dimension\":\"" << escapeJson(utf8ToWide(d.dimension)) << L"\",";
        out << L"\"category\":\""   << escapeJson(utf8ToWide(d.category))  << L"\",";
        out << L"\"path\":\""       << escapeJson(utf8ToWide(d.path))      << L"\",";
        out << L"\"property\":\""   << escapeJson(utf8ToWide(d.property))  << L"\",";
        out << L"\"refValue\":\""   << escapeJson(utf8ToWide(d.refValue))  << L"\",";
        out << L"\"dmValue\":\""    << escapeJson(utf8ToWide(d.dmValue))   << L"\",";
        out << L"\"severity\":"     << d.severity << L"}";
    }
    out << L"],";

    out << L"\"inferred\":[";
    first = true;
    for (auto& f : inferred) {
        if (!first) out << L",";
        first = false;
        out << L"\"" << escapeJson(utf8ToWide(f)) << L"\"";
    }
    out << L"]}}";

    sidebarWebView_->PostWebMessageAsString(out.str().c_str());
}

void BrowserWindow::sendAnalyzeError(const std::string& error) {
    if (!sidebarWebView_) return;
    std::wostringstream out;
    out << L"{\"type\":\"analyzeError\",\"error\":\""
        << escapeJson(utf8ToWide(error)) << L"\"}";
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