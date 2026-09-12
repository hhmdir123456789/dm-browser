// 插件宿主进程入口
// 用法：dm_plugin_host.exe <pluginId> <pipeName>
#include <iostream>
#include <string>
#include "ipc/Pipe.h"
#include "ipc/Framing.h"

using namespace dm::ipc;

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: dm_plugin_host <pluginId> <pipeName>\n";
        return 1;
    }
    std::string pluginId = argv[1];
    std::string pipeName = argv[2];

    Pipe pipe;
    auto conn = pipe.connectClient(pipeName);
    if (!conn.ok()) {
        std::cerr << "connect failed: " << conn.error().msg << "\n";
        return 2;
    }

    // 发送 Hello
    auto hello = Framing::encodeHello(pluginId, 0);
    Framing::writeFrame(pipe, hello);

    // 事件循环
    while (true) {
        auto frame = Framing::readFrame(pipe);
        if (!frame.ok()) break;

        auto kind = Framing::peekKind(frame.value());
        if (kind == Framing::Kind::Shutdown) break;

        if (kind == Framing::Kind::Invoke) {
            auto invoke = Framing::decodeInvoke(frame.value());
            if (!invoke.ok()) continue;

            // 插件侧处理逻辑：这里模拟一个简单应答
            // 真实插件会在这里调用自己的业务代码
            std::string response = "plugin " + pluginId +
                                   " handled " + invoke.value().method;

            auto result = Framing::encodeResult(true, response, 0, "");
            Framing::writeFrame(pipe, result);
        }
    }

    return 0;
}