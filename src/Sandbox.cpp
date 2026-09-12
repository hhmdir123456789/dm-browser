#include "Sandbox.h"

#ifdef DM_PLATFORM_WINDOWS
#include <windows.h>
#endif

namespace dm {

Platform Sandbox::currentPlatform() const {
#ifdef DM_PLATFORM_WINDOWS
    return Platform::Windows;
#elif defined(__APPLE__)
    return Platform::MacOS;
#else
    return Platform::Linux;
#endif
}

SandboxPolicy Sandbox::defaultRendererPolicy() {
    SandboxPolicy p;
    p.processType = ProcessType::Renderer;
    p.allowNetwork = false;
    p.allowFileWrite = false;
    p.allowProcessCreate = false;
    return p;
}

SandboxPolicy Sandbox::defaultUtilityPolicy() {
    SandboxPolicy p;
    p.processType = ProcessType::Utility;
    p.allowNetwork = false;
    p.allowFileWrite = false;
    p.allowProcessCreate = false;
    return p;
}

SandboxPolicy Sandbox::sensitiveUtilityPolicy() {
    SandboxPolicy p = defaultUtilityPolicy();
    // 敏感插件：额外限制，由端点级授权二次约束
    return p;
}

Result<bool> Sandbox::apply(Pid pid, const SandboxPolicy& policy) {
    switch (currentPlatform()) {
        case Platform::Windows: return applyWindows(pid, policy);
        case Platform::Linux:   return applyLinux(pid, policy);
        case Platform::MacOS:   return applyMacOS(pid, policy);
    }
    return Result<bool>::fail(Error::internal("unknown platform"));
}

#ifdef DM_PLATFORM_WINDOWS

// Windows 上用 Job Object 做资源限制
// 真实 AppContainer 沙箱需要更复杂的配置，这里实现 Job Object 基础版
Result<bool> Sandbox::applyWindows(Pid pid, const SandboxPolicy& policy) {
    HANDLE hProcess = OpenProcess(
        PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE, pid);
    if (!hProcess) {
        return Result<bool>::fail(
            Error::internal("OpenProcess failed: " + std::to_string(GetLastError())));
    }

    HANDLE hJob = CreateJobObjectA(nullptr, nullptr);
    if (!hJob) {
        CloseHandle(hProcess);
        return Result<bool>::fail(Error::internal("CreateJobObject failed"));
    }

    // 限制内存
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};
    info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_PROCESS_MEMORY;
    info.ProcessMemoryLimit = 256 * 1024 * 1024;  // 256 MB

    if (!SetInformationJobObject(hJob, JobObjectExtendedLimitInformation,
                                  &info, sizeof(info))) {
        CloseHandle(hProcess);
        CloseHandle(hJob);
        return Result<bool>::fail(Error::internal("SetInformationJobObject failed"));
    }

    if (!AssignProcessToJobObject(hJob, hProcess)) {
        CloseHandle(hProcess);
        CloseHandle(hJob);
        return Result<bool>::fail(Error::internal("AssignProcessToJobObject failed"));
    }

    CloseHandle(hProcess);
    // Job handle 故意不关闭，进程退出时系统自动清理
    return Result<bool>::ok(true);
}

#else

Result<bool> Sandbox::applyWindows(Pid, const SandboxPolicy&) {
    return Result<bool>::fail(Error::internal("not on Windows"));
}

#endif

Result<bool> Sandbox::applyLinux(Pid, const SandboxPolicy&) {
    return Result<bool>::fail(Error::internal("Linux sandbox not implemented"));
}

Result<bool> Sandbox::applyMacOS(Pid, const SandboxPolicy&) {
    return Result<bool>::fail(Error::internal("macOS sandbox not implemented"));
}

} // namespace dm