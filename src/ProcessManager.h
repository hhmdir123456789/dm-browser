#pragma once
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "common/Types.h"
#include "common/Result.h"

namespace dm {

enum class ProcessType { Renderer, Utility, Gpu };
enum class ProcessState { Starting, Running, Suspended, Terminating };

struct ProcessInfo {
    Pid pid{0};
    PluginId pluginId;
    ProcessType type{ProcessType::Utility};
    ProcessState state{ProcessState::Starting};
    bool sensitive{false};
    std::string pipeName;
    int64_t createdAt{0};
};

class ProcessManager {
public:
    ProcessManager() = default;
    ~ProcessManager();

    // 启动插件宿主进程，返回子进程 pid 和分配的管道名
    Result<ProcessInfo> spawnPluginHost(const PluginId& pluginId,
                                        bool sensitive,
                                        const std::string& exePath);

    // 终止进程
    bool terminate(Pid pid);

    // 查询
    std::optional<ProcessInfo> get(Pid pid) const;
    std::vector<ProcessInfo> byPlugin(const PluginId& pluginId) const;
    bool isSensitive(Pid pid) const;
    size_t count() const;

    // 检查进程是否还活着
    bool isAlive(Pid pid) const;

    // 回收已退出的进程
    void reap();

private:
    mutable std::mutex mu_;
    std::map<Pid, ProcessInfo> processes_;
    std::set<Pid> sensitivePids_;
    Pid nextPid_{1};
};

} // namespace dm