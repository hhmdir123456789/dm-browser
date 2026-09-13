#define _CRT_SECURE_NO_WARNINGS

#include "render/render_window.h"
#include <cstdio>
#include <cstring>
#include <cctype>
#include <string>

namespace dm::render {

static const wchar_t* kClassName = L"DMRenderWindow";

static COLORREF parseColor(const std::string& s) {
    if (s.empty()) return RGB(0, 0, 0);

    if (s[0] == '#' && s.size() >= 7) {
        unsigned int r = 0, g = 0, b = 0;
        std::sscanf(s.c_str() + 1, "%02x%02x%02x", &r, &g, &b);
        return RGB(r, g, b);
    }
    if (s[0] == '#' && s.size() >= 4) {
        char rs[2] = {s[1], s[1]};
        char gs[2] = {s[2], s[2]};
        char bs[2] = {s[3], s[3]};
        unsigned int r = 0, g = 0, b = 0;
        std::sscanf(rs, "%x", &r);
        std::sscanf(gs, "%x", &g);
        std::sscanf(bs, "%x", &b);
        return RGB(r, g, b);
    }
    // "r,g,b" 或 "r,g,b,a"
    int r = 0, g = 0, b = 0;
    if (std::sscanf(s.c_str(), "%d,%d,%d", &r, &g, &b) == 3) {
        return RGB(r, g, b);
    }
    return RGB(0, 0, 0);
}

// UTF-8 -> wide
static std::wstring utf8ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(),
                                   (int)s.size(), nullptr, 0);
    if (len <= 0) return L"";
    std::wstring out(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(),
                        (int)s.size(), &out[0], len);
    return out;
}

RenderWindow::RenderWindow() = default;

RenderWindow::~RenderWindow() {
    if (hwnd_) DestroyWindow(hwnd_);
}

bool RenderWindow::create(const std::wstring& title, int w, int h) {
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
    return true;
}

void RenderWindow::paintNode(HDC hdc, const RenderNode* node) {
    if (!node) return;
    if (!node->isText && node->style.display == "none") return;

    if (node->tag == "#document") {
        for (auto& c : node->children) paintNode(hdc, c.get());
        return;
    }

    // 文本节点
    if (node->isText) {
        if (node->text.empty()) return;
        const Layout& L = node->layout;
        if (L.w <= 0 || L.h <= 0) return;

        int x = (int)L.x, y = (int)L.y, w = (int)L.w, h = (int)L.h;

        int fs = 16;
        if (!node->style.fontSize.empty()) {
            try { fs = (int)std::stod(node->style.fontSize); } catch (...) {}
        }
        if (fs <= 0) fs = 16;

        HFONT font = CreateFontW(fs, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                 CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                 DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei");
        HGDIOBJ oldFont = SelectObject(hdc, font);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, parseColor(node->style.color));

        std::wstring wtext = utf8ToWide(node->text);
        RECT tr{x, y, x + w + 4, y + h + 4};
        DrawTextW(hdc, wtext.c_str(), -1, &tr,
                  DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOCLIP);

        SelectObject(hdc, oldFont);
        DeleteObject(font);
        return;
    }

    // 普通节点
    const Layout& L = node->layout;
    int x = (int)L.x, y = (int)L.y, w = (int)L.w, h = (int)L.h;

    if (w > 0 && h > 0) {
        int saved = SaveDC(hdc);

        // 背景色
        if (!node->style.backgroundColor.empty() &&
            node->style.backgroundColor != "0,0,0,0" &&
            node->style.backgroundColor != "0,0,0") {
            RECT r{x, y, x + w, y + h};
            HBRUSH br = CreateSolidBrush(parseColor(node->style.backgroundColor));
            FillRect(hdc, &r, br);
            DeleteObject(br);
        }

        // 边框
        if (node->style.borderWidth > 0 &&
            !node->style.borderStyle.empty() &&
            node->style.borderStyle != "none") {
            COLORREF bc = RGB(128, 128, 128);
            if (!node->style.borderColor.empty()) {
                bc = parseColor(node->style.borderColor);
            }
            HPEN pen = CreatePen(PS_SOLID, node->style.borderWidth, bc);
            HGDIOBJ oldPen = SelectObject(hdc, pen);
            HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Rectangle(hdc, x, y, x + w, y + h);
            SelectObject(hdc, oldPen);
            SelectObject(hdc, oldBrush);
            DeleteObject(pen);
        }

        // 节点自身的 text（如果有，非文本子节点）
        if (!node->text.empty()) {
            int fs = 16;
            if (!node->style.fontSize.empty()) {
                try { fs = (int)std::stod(node->style.fontSize); } catch (...) {}
            }
            if (fs <= 0) fs = 16;

            HFONT font = CreateFontW(fs, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                     CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                     DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei");
            HGDIOBJ oldFont = SelectObject(hdc, font);

            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, parseColor(node->style.color));

            std::wstring wtext = utf8ToWide(node->text);
            RECT tr{x, y, x + w, y + h + 4};
            DrawTextW(hdc, wtext.c_str(), -1, &tr,
                      DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);

            SelectObject(hdc, oldFont);
            DeleteObject(font);
        }

        RestoreDC(hdc, saved);
    }

    // 递归子节点
    for (auto& c : node->children) paintNode(hdc, c.get());
}

void RenderWindow::paint(HDC hdc) {
    RECT rc; GetClientRect(hwnd_, &rc);
    HBRUSH bg = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(hdc, &rc, bg);
    DeleteObject(bg);

    if (root_) paintNode(hdc, root_);
}

LRESULT CALLBACK RenderWindow::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    RenderWindow* self = (RenderWindow*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (msg) {
        case WM_CREATE: {
            auto* cs = (CREATESTRUCT*)lp;
            self = (RenderWindow*)cs->lpCreateParams;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)self);
            if (self) self->hwnd_ = hwnd;
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
        case WM_SIZE: {
            if (self) {
                self->width_ = LOWORD(lp);
                self->height_ = HIWORD(lp);
                InvalidateRect(hwnd, nullptr, TRUE);
            }
            return 0;
        }
        case WM_ERASEBKGND:
            // 我们自己在 paint 里填背景，避免闪烁
            return 1;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int RenderWindow::run() {
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}

} // namespace dm::render