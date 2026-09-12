#pragma once
#include <string>
#include <vector>
#include "common/Types.h"
#include "common/Result.h"
#include "ProcessManager.h"

namespace dm {

enum class Platform { Linux, MacOS, Windows };

struct SandboxPolicy {
    ProcessType processType{ProcessType::Utility};
    bool allowNetwork{false};
    bool allowFileWrite{false};
    bool allowProcessCreate{false};
    std::vector<std::string> allowedEndpoints;
};

class Sandbox {
public:
    Sandbox() = default;

    Platform currentPlatform() const;

    Result<bool> apply(Pid pid, const SandboxPolicy& policy);

    static SandboxPolicy defaultRendererPolicy();
    static SandboxPolicy defaultUtilityPolicy();
    static SandboxPolicy sensitiveUtilityPolicy();

private:
    Result<bool> applyWindows(Pid pid, const SandboxPolicy& policy);
    Result<bool> applyLinux(Pid pid, const SandboxPolicy& policy);
    Result<bool> applyMacOS(Pid pid, const SandboxPolicy& policy);
};

} // namespace dm