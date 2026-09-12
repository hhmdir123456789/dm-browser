#include "ProcessManager.h"
#include "ipc/Pipe.h"

#ifdef DM_PLATFORM_WINDOWS
#include <windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif

namespace dm {

ProcessManager::~ProcessManager() {
    std::lock_guard lock(mu_);
#ifdef DM_PLATFORM_WINDOWS
    for (auto& [pid, info] : processes_) {
        HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        if (h) { TerminateProcess(h, 0); CloseHandle(h); }
    }
#endif
}

Result<ProcessInfo> ProcessManager::spawnPluginHost(
    const PluginId& pluginId, bool sensitive, const std::string& exePath) {
    std::lock_guard lock(mu_);

    Pid pid = nextPid_++;
    std::string pipeName = ipc::Pipe::makePipeName("dm_ipc", pid);

    ProcessInfo info;
    info.pid = pid;
    info.pluginId = pluginId;
    info.type = ProcessType::Utility;
    info.state = ProcessState::Starting;
    info.sensitive = sensitive;
    info.pipeName = pipeName;
    info.createdAt = nowMs();

#ifdef DM_PLATFORM_WINDOWS
    // 构造命令行：dm_plugin_host.exe <pluginId> <pipeName>
    std::string cmd = exePath + " " + pluginId + " " + pipeName;

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    std::vector<char> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back('\0');

    BOOL ok = CreateProcessA(
        nullptr, cmdBuf.data(),
        nullptr, nullptr, FALSE,
        CREATE_NO_WINDOW,
        nullptr, nullptr,
        &si, &pi);
    if (!ok) {
        return Result<ProcessInfo>::fail(
            Error::internal("CreateProcess failed: " + std::to_string(GetLastError())));
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    // 用真实 Windows PID 替换内部 ID
    info.pid = static_cast<Pid>(pi.dwProcessId);
    info.state = ProcessState::Running;
    processes_[info.pid] = info;
    if (sensitive) sensitivePids_.insert(info.pid);

    return Result<ProcessInfo>::ok(info);
#else
    // POSIX 占位
    return Result<ProcessInfo>::fail(
        Error::internal("POSIX spawn not implemented"));
#endif
}

bool ProcessManager::terminate(Pid pid) {
    std::lock_guard lock(mu_);
    auto it = processes_.find(pid);
    if (it == processes_.end()) return false;
#ifdef DM_PLATFORM_WINDOWS
    HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (h) { TerminateProcess(h, 0); CloseHandle(h); }
#endif
    processes_.erase(it);
    sensitivePids_.erase(pid);
    return true;
}

std::optional<ProcessInfo> ProcessManager::get(Pid pid) const {
    std::lock_guard lock(mu_);
    auto it = processes_.find(pid);
    if (it == processes_.end()) return std::nullopt;
    return it->second;
}

std::vector<ProcessInfo> ProcessManager::byPlugin(const PluginId& pluginId) const {
    std::lock_guard lock(mu_);
    std::vector<ProcessInfo> out;
    for (const auto& [pid, info] : processes_)
        if (info.pluginId == pluginId) out.push_back(info);
    return out;
}

bool ProcessManager::isSensitive(Pid pid) const {
    std::lock_guard lock(mu_);
    return sensitivePids_.count(pid) > 0;
}

size_t ProcessManager::count() const {
    std::lock_guard lock(mu_);
    return processes_.size();
}

bool ProcessManager::isAlive(Pid pid) const {
#ifdef DM_PLATFORM_WINDOWS
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return false;
    DWORD code = 0;
    BOOL ok = GetExitCodeProcess(h, &code);
    CloseHandle(h);
    return ok && code == STILL_ACTIVE;
#else
    return true;
#endif
}

void ProcessManager::reap() {
    std::lock_guard lock(mu_);
    for (auto it = processes_.begin(); it != processes_.end();) {
        if (!isAlive(it->first)) {
            sensitivePids_.erase(it->first);
            it = processes_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace dm