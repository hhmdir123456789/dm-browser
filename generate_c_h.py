#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import os

ROOT = "."

FILES = {}

# ============================================================
# C: UI
# ============================================================
FILES["src/ui/Window.h"] = r'''#pragma once
#include <string>
#include <vector>
#include <functional>
#include <windows.h>

namespace dm::ui {

struct TabView {
    int id;
    std::string title;
    std::string origin;
    bool active;
};

class Window {
public:
    Window();
    ~Window();

    bool create(const std::string& title, int width, int height);
    void setTabs(const std::vector<TabView>& tabs);

    using NewTabFn    = std::function<void()>;
    using CloseTabFn  = std::function<void(int)>;
    using SwitchTabFn = std::function<void(int)>;
    using NavigateFn  = std::function<void(const std::string&)>;

    void onNewTab(NewTabFn fn)      { newTabFn_ = std::move(fn); }
    void onCloseTab(CloseTabFn fn)  { closeTabFn_ = std::move(fn); }
    void onSwitchTab(SwitchTabFn fn){ switchTabFn_ = std::move(fn); }
    void onNavigate(NavigateFn fn)  { navigateFn_ = std::move(fn); }

    int run();

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    void paint(HDC hdc);
    void layout();
    int hitTestTab(int x, int y);

    HWND hwnd_{nullptr};
    std::vector<TabView> tabs_;
    std::string address_;
    int width_{1024}, height_{768};

    static constexpr int kTabHeight = 32;
    static constexpr int kTabWidth  = 180;
    static constexpr int kAddrHeight = 36;
    static constexpr int kStatusHeight = 24;

    NewTabFn newTabFn_;
    CloseTabFn closeTabFn_;
    SwitchTabFn switchTabFn_;
    NavigateFn navigateFn_;
};

} // namespace dm::ui
'''

FILES["src/ui/Window.cpp"] = r'''#include "ui/Window.h"
#include <sstream>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

namespace dm::ui {

static const wchar_t* kClassName = L"DMBrowserWindow";

Window::Window() = default;
Window::~Window() {
    if (hwnd_) DestroyWindow(hwnd_);
}

bool Window::create(const std::string& title, int w, int h) {
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

    std::wstring wtitle(title.begin(), title.end());
    hwnd_ = CreateWindowExW(
        0, kClassName, wtitle.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, w, h,
        nullptr, nullptr, GetModuleHandle(nullptr), this);
    if (!hwnd_) return false;

    SetWindowLongPtrW(hwnd_, GWLP_USERDATA, (LONG_PTR)this);
    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);
    return true;
}

void Window::setTabs(const std::vector<TabView>& tabs) {
    tabs_ = tabs;
    if (hwnd_) InvalidateRect(hwnd_, nullptr, TRUE);
}

LRESULT CALLBACK Window::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    Window* self = (Window*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCT* cs = (CREATESTRUCT*)lp;
            self = (Window*)cs->lpCreateParams;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)self);
            self->hwnd_ = hwnd;
            return 0;
        }
        case WM_PAINT: {
            if (self) {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                self->paint(hdc);
                EndPaint(hwnd, &ps);
            }
            return 0;
        }
        case WM_LBUTTONDOWN: {
            if (self) {
                int x = LOWORD(lp), y = HIWORD(lp);
                int tabIdx = self->hitTestTab(x, y);
                if (tabIdx >= 0) {
                    if (x - tabIdx * kTabWidth > kTabWidth - 24) {
                        if (self->closeTabFn_)
                            self->closeTabFn_(self->tabs_[tabIdx].id);
                    } else {
                        if (self->switchTabFn_)
                            self->switchTabFn_(self->tabs_[tabIdx].id);
                    }
                } else if (y > kTabHeight && y < kTabHeight + kAddrHeight) {
                    if (self->newTabFn_) self->newTabFn_();
                }
            }
            return 0;
        }
        case WM_KEYDOWN: {
            if (self && wp == VK_RETURN) {
                if (self->navigateFn_) self->navigateFn_(self->address_);
            }
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

void Window::paint(HDC hdc) {
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, width_, height_);
    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

    RECT rc{0, 0, width_, height_};
    HBRUSH bg = CreateSolidBrush(RGB(245, 245, 245));
    FillRect(memDC, &rc, bg);
    DeleteObject(bg);

    for (size_t i = 0; i < tabs_.size(); ++i) {
        int x = (int)i * kTabWidth;
        RECT tabRc{x, 0, x + kTabWidth - 4, kTabHeight};
        HBRUSH tabBg = CreateSolidBrush(tabs_[i].active
            ? RGB(255, 255, 255) : RGB(220, 220, 220));
        FillRect(memDC, &tabRc, tabBg);
        DeleteObject(tabBg);

        std::wstring wtitle(tabs_[i].title.begin(), tabs_[i].title.end());
        SetBkMode(memDC, TRANSPARENT);
        SetTextColor(memDC, RGB(0, 0, 0));
        DrawTextW(memDC, wtitle.c_str(), -1, &tabRc,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        RECT closeRc{x + kTabWidth - 28, 6, x + kTabWidth - 8, 26};
        DrawTextW(memDC, L"x", -1, &closeRc,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    RECT newRc{(int)tabs_.size() * kTabWidth, 0,
               (int)tabs_.size() * kTabWidth + 32, kTabHeight};
    DrawTextW(memDC, L"+", -1, &newRc,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    RECT addrRc{8, kTabHeight + 4, width_ - 8, kTabHeight + kAddrHeight - 4};
    HBRUSH addrBg = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(memDC, &addrRc, addrBg);
    DeleteObject(addrBg);
    FrameRect(memDC, &addrRc, (HBRUSH)GetStockObject(GRAY_BRUSH));

    RECT statusRc{0, height_ - kStatusHeight, width_, height_};
    HBRUSH statusBg = CreateSolidBrush(RGB(230, 230, 230));
    FillRect(memDC, &statusRc, statusBg);
    DeleteObject(statusBg);

    std::wstringstream wss;
    wss << L"标签数: " << tabs_.size() << L"  |  大明DM浏览器";
    std::wstring status = wss.str();
    SetTextColor(memDC, RGB(80, 80, 80));
    DrawTextW(memDC, status.c_str(), -1, &statusRc,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    BitBlt(hdc, 0, 0, width_, height_, memDC, 0, 0, SRCCOPY);
    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);
}

void Window::layout() {}

int Window::hitTestTab(int x, int y) {
    if (y < 0 || y > kTabHeight) return -1;
    int idx = x / kTabWidth;
    if (idx < 0 || idx >= (int)tabs_.size()) return -1;
    return idx;
}

int Window::run() {
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}

} // namespace dm::ui
'''

FILES["src/ui/ui_main.cpp"] = r'''#include "ui/Window.h"
#include <iostream>

int main() {
    dm::ui::Window win;
    if (!win.create("大明DM浏览器", 1024, 768)) {
        std::cerr << "窗口创建失败\n";
        return 1;
    }

    std::vector<dm::ui::TabView> tabs = {
        {1, "起始页", "https://start.dm", true},
        {2, "书签", "https://bookmarks.dm", false},
    };
    win.setTabs(tabs);

    win.onNewTab([&]() { std::cout << "[UI] 新标签\n"; });
    win.onCloseTab([&](int id) { std::cout << "[UI] 关闭标签 " << id << "\n"; });
    win.onSwitchTab([&](int id) { std::cout << "[UI] 切换标签 " << id << "\n"; });
    win.onNavigate([&](const std::string& url) {
        std::cout << "[UI] 导航到 " << url << "\n";
    });

    return win.run();
}
'''

# ============================================================
# E: 插件 SDK
# ============================================================
FILES["src/plugin_sdk/plugin_api.h"] = r'''#pragma once
#include <string>

extern "C" {
    __declspec(dllexport) const char* dm_plugin_name();
    __declspec(dllexport) const char* dm_plugin_version();
    __declspec(dllexport) const char* dm_plugin_handle(
        const char* method, const char* args);
    __declspec(dllexport) void dm_plugin_free(const char* s);
}
'''

FILES["src/plugin_sdk/example_plugin.cpp"] = r'''#include "plugin_sdk/plugin_api.h"
#include <cstring>
#include <string>

extern "C" {

__declspec(dllexport) const char* dm_plugin_name() {
    return "example";
}

__declspec(dllexport) const char* dm_plugin_version() {
    return "1.0.0";
}

__declspec(dllexport) const char* dm_plugin_handle(
    const char* method, const char* args) {
    std::string result;
    if (std::strcmp(method, "greet") == 0) {
        result = "hello from example plugin, args=" + std::string(args);
    } else {
        result = "unknown method: " + std::string(method);
    }
    char* buf = new char[result.size() + 1];
    std::strcpy(buf, result.c_str());
    return buf;
}

__declspec(dllexport) void dm_plugin_free(const char* s) {
    delete[] s;
}

} // extern "C"
'''

# ============================================================
# D: 测试框架
# ============================================================
FILES["tests/test_framework.h"] = r'''#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <stdexcept>

namespace dm::test {

struct TestCase {
    std::string name;
    std::function<void()> fn;
};

class Registry {
public:
    static Registry& instance() {
        static Registry r;
        return r;
    }
    void add(const std::string& name, std::function<void()> fn) {
        tests_.push_back({name, fn});
    }
    int runAll() {
        int pass = 0, fail = 0;
        for (auto& t : tests_) {
            try {
                t.fn();
                std::cout << "[PASS] " << t.name << "\n";
                pass++;
            } catch (const std::exception& e) {
                std::cout << "[FAIL] " << t.name << " — " << e.what() << "\n";
                fail++;
            }
        }
        std::cout << "\n=== " << pass << " 通过, " << fail << " 失败 ===\n";
        return fail == 0 ? 0 : 1;
    }
private:
    std::vector<TestCase> tests_;
};

} // namespace dm::test

#define TEST(name) \
    static void test_##name(); \
    static struct Reg_##name { \
        Reg_##name() { dm::test::Registry::instance().add(#name, test_##name); } \
    } reg_##name; \
    static void test_##name()

#define EXPECT_TRUE(x) do { if (!(x)) throw std::runtime_error("EXPECT_TRUE failed: " #x); } while(0)
#define EXPECT_EQ(a, b) do { if (!((a) == (b))) throw std::runtime_error("EXPECT_EQ failed: " #a " != " #b); } while(0)
'''

FILES["tests/test_main.cpp"] = r'''#include "test_framework.h"

int main() {
    return dm::test::Registry::instance().runAll();
}
'''

FILES["tests/test_milestone1.cpp"] = r'''#include "test_framework.h"
#include "TabManager.h"

using namespace dm::test;

TEST(M1_CreateTab) {
    TabManager tm;
    TabId id = tm.create("https://start.dm");
    EXPECT_TRUE(id > 0);
    EXPECT_EQ(tm.count(), (size_t)1);
}

TEST(M1_CloseTab) {
    TabManager tm;
    TabId id = tm.create("https://a.dm");
    EXPECT_TRUE(tm.close(id));
    EXPECT_EQ(tm.count(), (size_t)0);
}

TEST(M1_SwitchTab) {
    TabManager tm;
    tm.create("https://a.dm");
    TabId id2 = tm.create("https://b.dm");
    EXPECT_TRUE(tm.activate(id2));
    EXPECT_EQ(tm.activeId(), id2);
}
'''

FILES["tests/test_milestone2.cpp"] = r'''#include "test_framework.h"
#include "db/Database.h"
#include "Storage.h"

using namespace dm::test;

TEST(M2_BookmarkAdd) {
    Database db;
    db.open(":memory:");
    db.exec("CREATE TABLE IF NOT EXISTS bookmarks (id INTEGER PRIMARY KEY AUTOINCREMENT, title TEXT, url TEXT, created_at INTEGER)");
    BookmarkStore store(db);
    auto r = store.add("DM", "https://dm.dm");
    EXPECT_TRUE(r.ok());
    EXPECT_EQ(store.size(), (size_t)1);
}

TEST(M2_BookmarkRemove) {
    Database db;
    db.open(":memory:");
    db.exec("CREATE TABLE IF NOT EXISTS bookmarks (id INTEGER PRIMARY KEY AUTOINCREMENT, title TEXT, url TEXT, created_at INTEGER)");
    BookmarkStore store(db);
    auto r = store.add("A", "https://a.dm");
    EXPECT_TRUE(store.remove(r.value()));
    EXPECT_EQ(store.size(), (size_t)0);
}

TEST(M2_DownloadCreate) {
    Database db;
    db.open(":memory:");
    db.exec("CREATE TABLE IF NOT EXISTS downloads (id INTEGER PRIMARY KEY AUTOINCREMENT, url TEXT, filename TEXT, state TEXT, bytes_received INTEGER, total_bytes INTEGER, created_at INTEGER)");
    DownloadStore store(db);
    auto r = store.create("https://f.dm/a.zip", "a.zip");
    EXPECT_TRUE(r.ok());
    EXPECT_EQ(store.size(), (size_t)1);
}
'''

FILES["tests/test_milestone3.cpp"] = r'''#include "test_framework.h"
#include "db/Database.h"
#include "SecureStore.h"

using namespace dm::test;

TEST(M3_SecureStoreSaveLookup) {
    Database db;
    db.open(":memory:");
    db.exec("CREATE TABLE IF NOT EXISTS credentials (origin TEXT, username TEXT, encrypted_password TEXT, created_at INTEGER, PRIMARY KEY (origin, username))");
    SecureStore store(db);
    EXPECT_TRUE(store.store("https://bank.dm", "alice", "pw"));
    auto creds = store.lookup("https://bank.dm");
    EXPECT_EQ(creds.size(), (size_t)1);
    EXPECT_EQ(creds[0].username, std::string("alice"));
}
'''

FILES["tests/test_milestone4.cpp"] = r'''#include "test_framework.h"
#include "Signature.h"

using namespace dm::test;

TEST(M4_SignatureVerify) {
    SignatureVerifier v;
    v.addKey("k1", "pub", MarketTier::Official);
    auto r = v.verify("k1", "payload", "sig:k1:payload");
    EXPECT_TRUE(r.ok());
    EXPECT_EQ((int)r.value(), (int)MarketTier::Official);
}

TEST(M4_SignatureInvalid) {
    SignatureVerifier v;
    v.addKey("k1", "pub", MarketTier::Official);
    auto r = v.verify("k1", "payload", "wrong");
    EXPECT_TRUE(!r.ok());
}
'''

FILES["tests/test_milestone5.cpp"] = r'''#include "test_framework.h"
#include "EnterprisePolicy.h"
#include "db/Database.h"

using namespace dm::test;

TEST(M5_PolicyLoad) {
    Database db;
    db.open(":memory:");
    db.exec("CREATE TABLE IF NOT EXISTS enterprise_policy (policy_id TEXT PRIMARY KEY, admin_signature TEXT, plugin_whitelist TEXT, endpoint_quota TEXT, audit_retention_days INTEGER, mirror_url TEXT, updated_at INTEGER)");
    EnterprisePolicy p(db);
    EnterprisePolicyData d;
    d.policyId = "ent-1";
    d.mirrorUrl = "https://mirror.dm";
    std::string sig = EnterprisePolicy::makeAdminSignature("admin", d);
    auto r = p.load(d, "admin", sig);
    EXPECT_TRUE(r.ok());
    EXPECT_TRUE(p.isLoaded());
}
'''

FILES["tests/test_milestone6.cpp"] = r'''#include "test_framework.h"
#include "db/Database.h"
#include "Dispute.h"

using namespace dm::test;

TEST(M6_DisputeReport) {
    Database db;
    db.open(":memory:");
    db.exec("CREATE TABLE IF NOT EXISTS dispute_case (case_id TEXT PRIMARY KEY, plugin_id TEXT, reporter_id TEXT, reason TEXT, status TEXT, ruling TEXT, resolution TEXT, created_at INTEGER, resolved_at INTEGER, closed_at INTEGER)");
    db.exec("CREATE TABLE IF NOT EXISTS dispute_appeal (case_id TEXT, note TEXT, created_at INTEGER)");
    DisputeManager dm(db);
    auto r = dm.report("plugin.a", "user1", "reason");
    EXPECT_TRUE(r.ok());
    EXPECT_EQ(dm.count(), (size_t)1);
}
'''

FILES["tests/test_ipc.cpp"] = r'''#include "test_framework.h"
#include "ipc/Framing.h"

using namespace dm::test;

TEST(IPC_EncodeDecodeInvoke) {
    auto frame = dm::ipc::Framing::encodeInvoke(
        "g-1", "Ping", "hello", "args", "t-1", "https://a.dm");
    auto kind = dm::ipc::Framing::peekKind(frame);
    EXPECT_EQ((int)kind, (int)dm::ipc::Framing::Kind::Invoke);
    auto m = dm::ipc::Framing::decodeInvoke(frame);
    EXPECT_TRUE(m.ok());
    EXPECT_EQ(m.value().endpoint, std::string("Ping"));
    EXPECT_EQ(m.value().method, std::string("hello"));
}

TEST(IPC_EncodeDecodeResult) {
    auto frame = dm::ipc::Framing::encodeResult(true, "value", 0, "");
    auto kind = dm::ipc::Framing::peekKind(frame);
    EXPECT_EQ((int)kind, (int)dm::ipc::Framing::Kind::Result);
    auto m = dm::ipc::Framing::decodeResult(frame);
    EXPECT_TRUE(m.ok());
    EXPECT_TRUE(m.value().ok);
    EXPECT_EQ(m.value().value, std::string("value"));
}
'''

FILES["tests/test_persistence.cpp"] = r'''#include "test_framework.h"
#include "db/Database.h"

using namespace dm::test;

TEST(Persist_BasicInsertQuery) {
    Database db;
    db.open(":memory:");
    db.exec("CREATE TABLE t (id INTEGER PRIMARY KEY, name TEXT)");
    db.exec("INSERT INTO t (name) VALUES (?)", {"alice"});
    auto rows = db.query("SELECT name FROM t");
    EXPECT_EQ(rows.size(), (size_t)1);
    EXPECT_EQ(rows[0][0], std::string("alice"));
}

TEST(Persist_CountRows) {
    Database db;
    db.open(":memory:");
    db.exec("CREATE TABLE t (id INTEGER PRIMARY KEY)");
    db.exec("INSERT INTO t DEFAULT VALUES");
    db.exec("INSERT INTO t DEFAULT VALUES");
    auto rows = db.query("SELECT COUNT(*) FROM t");
    EXPECT_EQ(rows[0][0], std::string("2"));
}
'''

def main():
    print(f"=== 生成 C-H 缺失文件 ===\n")
    count = 0
    for path, content in FILES.items():
        full = os.path.join(ROOT, path)
        os.makedirs(os.path.dirname(full), exist_ok=True)
        with open(full, "w", encoding="utf-8") as f:
            f.write(content)
        print(f"  写入 {path}")
        count += 1
    print(f"\n完成，共 {count} 个文件")

if __name__ == "__main__":
    main()