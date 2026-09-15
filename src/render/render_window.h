#pragma once
#include "render/render_node.h"
#include <windows.h>
#include <string>
#include <iostream>

namespace dm::render {

class RenderWindow {
public:
    RenderWindow();
    ~RenderWindow();

    bool create(const std::wstring& title, int w, int h);

    void setRoot(const RenderNode* root) {
        root_ = root;
        if (hwnd_) InvalidateRect(hwnd_, nullptr, TRUE);
    }

    int run();

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    void paint(HDC hdc);
    void paintNode(HDC hdc, const RenderNode* node);
    void paintNodeInner(HDC hdc, const RenderNode* node);
    void paintNodeWithOpacity(HDC hdc, const RenderNode* node, float opacity);
    void paintNodeWithFilter(HDC hdc, const RenderNode* node,
                             const std::string& filterStr, float opacity);

    HWND hwnd_{nullptr};
    const RenderNode* root_{nullptr};
    int width_{1024}, height_{768};
};

} // namespace dm::render