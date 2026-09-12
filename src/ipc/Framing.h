#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "common/Result.h"

namespace dm::ipc {

// 消息分帧：4 字节长度前缀 + 负载
// 负载格式：1 字节类型 + 字符串字段（长度 + 内容）
class Framing {
public:
    // 编码一条 Invoke 信封
    static std::vector<uint8_t> encodeInvoke(
        const std::string& grantId,
        const std::string& endpoint,
        const std::string& method,
        const std::string& args,
        const std::string& traceId,
        const std::string& origin);

    // 编码一条 Invoke 响应
    static std::vector<uint8_t> encodeResult(bool ok,
                                             const std::string& value,
                                             int32_t errorCode,
                                             const std::string& errorMsg);

    // 编码一条握手消息（插件进程启动后第一条）
    static std::vector<uint8_t> encodeHello(const std::string& pluginId,
                                            int32_t pid);

    // 解码
    struct InvokeMessage {
        std::string grantId, endpoint, method, args, traceId, origin;
    };
    struct ResultMessage {
        bool ok{false};
        std::string value;
        int32_t errorCode{0};
        std::string errorMsg;
    };
    struct HelloMessage {
        std::string pluginId;
        int32_t pid{0};
    };

    enum class Kind : uint8_t {
        Hello = 1,
        Invoke = 2,
        Result = 3,
        Shutdown = 4
    };

    static Kind peekKind(const std::vector<uint8_t>& frame);
    static Result<InvokeMessage> decodeInvoke(const std::vector<uint8_t>& frame);
    static Result<ResultMessage> decodeResult(const std::vector<uint8_t>& frame);
    static Result<HelloMessage>  decodeHello(const std::vector<uint8_t>& frame);

    // 写入/读取完整帧
    static Result<bool> writeFrame(class Pipe& pipe,
                                   const std::vector<uint8_t>& payload);
    static Result<std::vector<uint8_t>> readFrame(class Pipe& pipe);

private:
    static void putString(std::vector<uint8_t>& out, const std::string& s);
    static Result<std::string> getString(const std::vector<uint8_t>& in,
                                         size_t& offset);
};

} // namespace dm::ipc