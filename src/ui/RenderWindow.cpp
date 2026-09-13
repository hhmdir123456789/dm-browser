#include "ui/RenderWindow.h"
#include <iostream>
#include <WebView2.h>
#include <WebView2EnvironmentOptions.h>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

namespace dm::ui {

static const wchar_t* kClassName = L"DMRenderWindow";

RenderWindow::RenderWindow() = default;

RenderWindow::~RenderWindow() {
    if (controller_) controller_->Close();
    if (hwnd_) DestroyWindow(hwnd_);
}

bool RenderWindow::create(const std::wstring& title, int w, int h) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = kClassName;
    RegisterClassExW(&wc);

    hwnd_ = CreateWindowExW(
        0, kClassName, title.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, w, h,
        nullptr, nullptr, GetModuleHandle(nullptr), this);
    if (!hwnd_) return false;

    SetWindowLongPtrW(hwnd_, GWLP_USERDATA, (LONG_PTR)this);
    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);

    // 初始化 WebView2 环境
    auto options = Microsoft::WRL::Make<CoreWebView2EnvironmentOptions>();
    options->put_AdditionalBrowserArguments(L"--disable-web-security");

    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, nullptr, options.Get(),
        Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                if (FAILED(result) || !env) {
                    std::cerr << "[UI] WebView2 环境创建失败\n";
                    return S_OK;
                }
                env->CreateCoreWebView2Controller(hwnd_,
                    Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [this](HRESULT result, ICoreWebView2Controller* ctrl) -> HRESULT {
                            if (FAILED(result) || !ctrl) {
                                std::cerr << "[UI] WebView2 控制器创建失败\n";
                                return S_OK;
                            }
                            controller_ = ctrl;
                            controller_->get_CoreWebView2(&webview_);

                            RECT bounds;
                            GetClientRect(hwnd_, &bounds);
                            controller_->put_Bounds(bounds);
                            controller_->put_IsVisible(TRUE);

                            // 新窗口请求
                            webview_->add_NewWindowRequested(
                                Microsoft::WRL::Callback<ICoreWebView2NewWindowRequestedEventHandler>(
                                    [this](ICoreWebView2*,
                                           ICoreWebView2NewWindowRequestedEventArgs* args) -> HRESULT {
                                        if (newWindowFn_) {
                                            LPWSTR uri = nullptr;
                                            args->get_Uri(&uri);
                                            if (uri) {
                                                newWindowFn_(uri);
                                                CoTaskMemFree(uri);
                                            }
                                        }
                                        return S_OK;
                                    }).Get(), nullptr);

                            onWebViewReady();
                            return S_OK;
                        }).Get());
                return S_OK;
            }).Get());

    if (FAILED(hr)) {
        std::cerr << "[UI] CreateCoreWebView2EnvironmentWithOptions 失败\n";
        return false;
    }
    return true;
}

void RenderWindow::navigate(const std::wstring& url) {
    if (webview_) webview_->Navigate(url.c_str());
}

void RenderWindow::navigateToString(const std::wstring& html) {
    if (webview_) webview_->NavigateToString(html.c_str());
}

void RenderWindow::onWebViewReady() {
    std::cout << "[UI] WebView2 就绪\n";
    if (readyFn_) readyFn_();
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
        case WM_SIZE: {
            if (self && self->controller_) {
                RECT bounds;
                GetClientRect(hwnd, &bounds);
                self->controller_->put_Bounds(bounds);
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

} // namespace dm::ui