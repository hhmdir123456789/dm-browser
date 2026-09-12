#include "ui/Window.h"
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
