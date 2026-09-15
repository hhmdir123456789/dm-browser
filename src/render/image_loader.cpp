#include "render/image_loader.h"
#include <windows.h>
#include <wincodec.h>
#include <shlwapi.h>
#include <vector>
#include <string>

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shlwapi.lib")

namespace dm::render {

namespace {

template <typename T>
void safeRelease(T*& p) {
    if (p) { p->Release(); p = nullptr; }
}

std::wstring utf8ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    if (len <= 0) return L"";
    std::wstring out(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], len);
    return out;
}

} // namespace

bool loadImageWic(const std::string& path, ImageData& out) {
    out = ImageData{};
    if (path.empty()) return false;

    // 路径支持：绝对、相对；带 file:// 前缀
    std::string p = path;
    if (p.rfind("file://", 0) == 0) p = p.substr(7);
    if (p.empty()) return false;

    std::wstring wpath = utf8ToWide(p);

    HRESULT hrInit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool needUninit = SUCCEEDED(hrInit);

    IWICImagingFactory* factory = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* converter = nullptr;

    bool ok = false;
    do {
        HRESULT hr = CoCreateInstance(
            CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&factory));
        if (FAILED(hr)) break;

        hr = factory->CreateDecoderFromFilename(
            wpath.c_str(), nullptr, GENERIC_READ,
            WICDecodeMetadataCacheOnLoad, &decoder);
        if (FAILED(hr)) break;

        hr = decoder->GetFrame(0, &frame);
        if (FAILED(hr)) break;

        hr = factory->CreateFormatConverter(&converter);
        if (FAILED(hr)) break;

        hr = converter->Initialize(
            frame, GUID_WICPixelFormat32bppPBGRA,
            WICBitmapDitherTypeNone, nullptr, 0.0,
            WICBitmapPaletteTypeCustom);
        if (FAILED(hr)) break;

        UINT w = 0, h = 0;
        hr = converter->GetSize(&w, &h);
        if (FAILED(hr) || w == 0 || h == 0) break;

        out.bgra.resize((size_t)w * h * 4);
        hr = converter->CopyPixels(
            nullptr, w * 4, (UINT)out.bgra.size(), out.bgra.data());
        if (FAILED(hr)) { out.bgra.clear(); break; }

        out.width = (int)w;
        out.height = (int)h;
        ok = true;
    } while (false);

    safeRelease(converter);
    safeRelease(frame);
    safeRelease(decoder);
    safeRelease(factory);

    if (needUninit) CoUninitialize();

    return ok;
}

} // namespace dm::render