#include "ipc/Pipe.h"
#include <cstring>

#ifdef DM_PLATFORM_WINDOWS
#include <windows.h>
#else
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <cerrno>
#endif

namespace dm::ipc {

Pipe::~Pipe() { close(); }

Pipe::Pipe(Pipe&& other) noexcept {
#ifdef DM_PLATFORM_WINDOWS
    handle_ = other.handle_;
    other.handle_ = INVALID_HANDLE_VALUE;
#else
    fd_ = other.fd_;
    other.fd_ = -1;
#endif
}

Pipe& Pipe::operator=(Pipe&& other) noexcept {
    if (this != &other) {
        close();
#ifdef DM_PLATFORM_WINDOWS
        handle_ = other.handle_;
        other.handle_ = INVALID_HANDLE_VALUE;
#else
        fd_ = other.fd_;
        other.fd_ = -1;
#endif
    }
    return *this;
}

#ifdef DM_PLATFORM_WINDOWS

Result<bool> Pipe::createServer(const std::string& name) {
    std::string fullName = "\\\\.\\pipe\\" + name;
    handle_ = CreateNamedPipeA(
        fullName.c_str(),
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
        1,              // 最多一个实例
        65536,          // 输出缓冲
        65536,          // 输入缓冲
        0,              // 默认超时
        nullptr);
    if (handle_ == INVALID_HANDLE_VALUE) {
        return Result<bool>::fail(Error::internal("CreateNamedPipe failed"));
    }
    return Result<bool>::ok(true);
}

Result<bool> Pipe::waitForClient() {
    if (handle_ == INVALID_HANDLE_VALUE) {
        return Result<bool>::fail(Error::internal("pipe not created"));
    }
    BOOL ok = ConnectNamedPipe(handle_, nullptr);
    if (!ok && GetLastError() != ERROR_PIPE_CONNECTED) {
        return Result<bool>::fail(Error::internal("ConnectNamedPipe failed"));
    }
    return Result<bool>::ok(true);
}

Result<bool> Pipe::connectClient(const std::string& name) {
    std::string fullName = "\\\\.\\pipe\\" + name;
    // 等待管道可用
    if (!WaitNamedPipeA(fullName.c_str(), 5000)) {
        return Result<bool>::fail(Error::internal("WaitNamedPipe timeout"));
    }
    handle_ = CreateFileA(
        fullName.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0, nullptr,
        OPEN_EXISTING,
        0, nullptr);
    if (handle_ == INVALID_HANDLE_VALUE) {
        return Result<bool>::fail(Error::internal("CreateFile pipe failed"));
    }
    return Result<bool>::ok(true);
}

Result<int32_t> Pipe::write(const std::vector<uint8_t>& data) {
    if (handle_ == INVALID_HANDLE_VALUE) {
        return Result<int32_t>::fail(Error::internal("pipe not open"));
    }
    DWORD written = 0;
    BOOL ok = WriteFile(handle_, data.data(),
                        static_cast<DWORD>(data.size()), &written, nullptr);
    if (!ok) {
        return Result<int32_t>::fail(Error::internal("WriteFile failed"));
    }
    return Result<int32_t>::ok(static_cast<int32_t>(written));
}

Result<std::vector<uint8_t>> Pipe::read(size_t maxBytes) {
    if (handle_ == INVALID_HANDLE_VALUE) {
        return Result<std::vector<uint8_t>>::fail(
            Error::internal("pipe not open"));
    }
    std::vector<uint8_t> buf(maxBytes);
    DWORD read = 0;
    BOOL ok = ReadFile(handle_, buf.data(),
                       static_cast<DWORD>(maxBytes), &read, nullptr);
    if (!ok) {
        return Result<std::vector<uint8_t>>::fail(
            Error::internal("ReadFile failed"));
    }
    buf.resize(read);
    return Result<std::vector<uint8_t>>::ok(std::move(buf));
}

bool Pipe::isOpen() const {
    return handle_ != INVALID_HANDLE_VALUE;
}

void Pipe::close() {
    if (handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
    }
}

#else // POSIX 占位实现

Result<bool> Pipe::createServer(const std::string&) {
    return Result<bool>::fail(Error::internal("POSIX pipe not implemented"));
}
Result<bool> Pipe::waitForClient() {
    return Result<bool>::fail(Error::internal("POSIX pipe not implemented"));
}
Result<bool> Pipe::connectClient(const std::string&) {
    return Result<bool>::fail(Error::internal("POSIX pipe not implemented"));
}
Result<int32_t> Pipe::write(const std::vector<uint8_t>&) {
    return Result<int32_t>::fail(Error::internal("POSIX pipe not implemented"));
}
Result<std::vector<uint8_t>> Pipe::read(size_t) {
    return Result<std::vector<uint8_t>>::fail(
        Error::internal("POSIX pipe not implemented"));
}
bool Pipe::isOpen() const { return fd_ >= 0; }
void Pipe::close() { if (fd_ >= 0) { ::close(fd_); fd_ = -1; } }

#endif

std::string Pipe::makePipeName(const std::string& prefix, int pid) {
    return prefix + "_" + std::to_string(pid);
}

} // namespace dm::ipc