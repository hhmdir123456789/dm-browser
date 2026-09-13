// 插件宿主进程入口
// 用法：dm_plugin_host.exe <pluginId> <pipeName>
#include <iostream>
#include <string>
#include <windows.h>
#include "ipc/Pipe.h"
#include "ipc/Framing.h"
#include "plugin_sdk/plugin_api.h"

using namespace dm::ipc;

// ============================================================
// 插件句柄：封装 LoadLibrary / GetProcAddress / FreeLibrary
// ============================================================
struct PluginHandle {
    HMODULE dll{nullptr};
    NameFn name{nullptr};
    VersionFn version{nullptr};
    HandleFn handle{nullptr};
    FreeFn freeFn{nullptr};
    std::string path;
    int loadCount{0};

    bool load(const std::string& dllPath) {
        path = dllPath;
        dll = LoadLibraryA(dllPath.c_str());
        if (!dll) {
            std::cerr << "[host] LoadLibrary failed: " << dllPath
                      << " err=" << GetLastError() << "\n";
            return false;
        }
        name = (NameFn)GetProcAddress(dll, "dm_plugin_name");
        version = (VersionFn)GetProcAddress(dll, "dm_plugin_version");
        handle = (HandleFn)GetProcAddress(dll, "dm_plugin_handle");
        freeFn = (FreeFn)GetProcAddress(dll, "dm_plugin_free");

        if (!handle || !freeFn) {
            std::cerr << "[host] missing exports in " << dllPath << "\n";
            FreeLibrary(dll);
            dll = nullptr;
            return false;
        }
        loadCount++;
        std::cerr << "[host] loaded plugin: "
                  << (name ? name() : "?")
                  << " v" << (version ? version() : "?")
                  << " (load #" << loadCount << ")\n";
        return true;
    }

    void unload() {
        if (dll) {
            FreeLibrary(dll);
            dll = nullptr;
            name = nullptr;
            version = nullptr;
            handle = nullptr;
            freeFn = nullptr;
            std::cerr << "[host] unloaded plugin: " << path << "\n";
        }
    }

    bool isLoaded() const { return dll != nullptr; }
};

// ============================================================
// 主流程
// ============================================================
int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: dm_plugin_host <pluginId> <pipeName>\n";
        return 1;
    }
    std::string pluginId = argv[1];
    std::string pipeName = argv[2];

    // 默认加载同目录的 example_plugin.dll
    std::string dllPath = "example_plugin.dll";

    // 连接管道
    Pipe pipe;
    auto conn = pipe.connectClient(pipeName);
    if (!conn.isOk()) {
        std::cerr << "[host] connect failed: " << conn.error().msg << "\n";
        return 2;
    }

    // 加载插件
    PluginHandle plugin;
    bool loaded = plugin.load(dllPath);

    // 发送 Hello
    auto hello = Framing::encodeHello(pluginId, 0);
    Framing::writeFrame(pipe, hello);

    // 事件循环
    while (true) {
        auto frame = Framing::readFrame(pipe);
        if (!frame.isOk()) break;

        auto kind = Framing::peekKind(frame.value());
        if (kind == Framing::Kind::Shutdown) break;

        if (kind != Framing::Kind::Invoke) continue;

        auto invoke = Framing::decodeInvoke(frame.value());
        if (!invoke.isOk()) continue;

        const std::string& method = invoke.value().method;
        const std::string& args = invoke.value().args;
        std::string response;

        // ---- reload：卸载重载 ----
        if (method == "reload") {
            plugin.unload();
            loaded = plugin.load(dllPath);
            if (loaded) {
                response = "reloaded: " + std::string(plugin.name())
                         + " v" + std::string(plugin.version())
                         + " (load #" + std::to_string(plugin.loadCount) + ")";
            } else {
                response = "reload failed";
            }
        }
        // ---- status：查询插件状态 ----
        else if (method == "status") {
            if (plugin.isLoaded()) {
                response = "loaded: " + std::string(plugin.name())
                         + " v" + std::string(plugin.version())
                         + " (load #" + std::to_string(plugin.loadCount) + ")";
            } else {
                response = "not loaded";
            }
        }
        // ---- 其他方法：转发给插件 ----
        else if (loaded) {
            const char* r = plugin.handle(method.c_str(), args.c_str());
            if (r) {
                response = r;
                plugin.freeFn(r);
            } else {
                response = "plugin returned null";
            }
        } else {
            response = "plugin not loaded";
        }

        // 发送结果
        auto result = Framing::encodeResult(true, response, 0, "");
        Framing::writeFrame(pipe, result);
    }

    // 退出前卸载
    plugin.unload();
    return 0;
}