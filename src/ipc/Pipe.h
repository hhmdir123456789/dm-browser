#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "common/Result.h"

#ifdef DM_PLATFORM_WINDOWS
#include <windows.h>
#endif

namespace dm::ipc {

// 命名管道封装，支持 Windows 和 POSIX
class Pipe {
public:
    Pipe() = default;
    ~Pipe();

    Pipe(const Pipe&) = delete;
    Pipe& operator=(const Pipe&) = delete;
    Pipe(Pipe&& other) noexcept;
    Pipe& operator=(Pipe&& other) noexcept;

#ifdef DM_PLATFORM_WINDOWS
    // 服务端：创建管道并等待客户端连接
    Result<bool> createServer(const std::string& name);
    Result<bool> waitForClient();

    // 客户端：连接到已有管道
    Result<bool> connectClient(const std::string& name);
#endif

    // 读写：返回实际字节数，-1 表示错误
    Result<int32_t> write(const std::vector<uint8_t>& data);
    Result<std::vector<uint8_t>> read(size_t maxBytes);

    bool isOpen() const;
    void close();

    // 从管道名中提取可用的名称（不含 \\.\pipe\ 前缀）
    static std::string makePipeName(const std::string& prefix, int pid);

private:
#ifdef DM_PLATFORM_WINDOWS
    HANDLE handle_{INVALID_HANDLE_VALUE};
#else
    int fd_{-1};
#endif
};

} // namespace dm::ipc