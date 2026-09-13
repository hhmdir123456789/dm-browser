#define _CRT_SECURE_NO_WARNINGS

#include "render/render_window.h"
#include <cstdio>
#include <cstring>
#include <cctype>

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
    return RGB(0, 0, 0);
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
    if (node->style.display == "none") return;
    if (node->tag == "#document") {
        for (auto& c : node->children) paintNode(hdc, c.get());
        return;
    }

    const Layout& L = node->layout;
    int x = (int)L.x, y = (int)L.y, w = (int)L.w, h = (int)L.h;

    if (!node->style.backgroundColor.empty()) {
        RECT r{x, y, x + w, y + h};
        HBRUSH br = CreateSolidBrush(parseColor(node->style.backgroundColor));
        FillRect(hdc, &r, br);
        DeleteObject(br);
    }

    if (node->style.borderWidth > 0) {
        HPEN pen = CreatePen(PS_SOLID, node->style.borderWidth,
                             RGB(128, 128, 128));
        HGDIOBJ oldPen = SelectObject(hdc, pen);
        HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, x, y, x + w, y + h);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
        DeleteObject(pen);
    }

    if (!node->text.empty()) {
        int fs = 16;
        std::string fss = node->style.fontSize;
        if (!fss.empty()) {
            try { fs = (int)std::stod(fss); } catch (...) {}
        }
        if (fs <= 0) fs = 16;

        HFONT font = CreateFontA(fs, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                 CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                 DEFAULT_PITCH | FF_DONTCARE, "Microsoft YaHei");
        HGDIOBJ oldFont = SelectObject(hdc, font);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, parseColor(node->style.color));

        RECT tr{x, y, x + w, y + h + 4};
        DrawTextA(hdc, node->text.c_str(), -1, &tr,
                  DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);

        SelectObject(hdc, oldFont);
        DeleteObject(font);
    }

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
        case WM_SIZE: {
            if (self) {
                self->width_ = LOWORD(lp);
                self->height_ = HIWORD(lp);
                InvalidateRect(hwnd, nullptr, TRUE);
            }
            return 0;
        }
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