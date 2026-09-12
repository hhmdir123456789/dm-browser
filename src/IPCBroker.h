#pragma once
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include "GrantStore.h"
#include "Confirmation.h"
#include "ProcessManager.h"
#include "ipc/Pipe.h"
#include "ipc/Framing.h"
#include "common/AuditLog.h"

struct InvokeEnvelope {
    GrantId grantId;
    std::string endpoint;
    std::string method;
    std::string args;
    TraceId traceId;
    std::string origin;
};

struct InvokeResult {
    bool ok{false};
    std::string value;
    Error error;
};

class IPCBroker {
public:
    using Handler = std::function<InvokeResult(const InvokeEnvelope&)>;

    IPCBroker(GrantStore& gs, AuditLog& audit, ConfirmationManager& confirm,
              dm::ProcessManager& pm)
        : gs_(gs), audit_(audit), confirm_(confirm), pm_(pm) {}

    // 注册端点处理器（进程内调用）
    void registerHandler(const std::string& ep, Handler h) {
        std::lock_guard lock(mu_);
        handlers_[ep] = std::move(h);
    }

    // 标记敏感端点，调用前需二次确认
    void markSensitive(const std::string& ep) {
        std::lock_guard lock(mu_);
        sensitive_.insert(ep);
    }

    // 进程内调用：校验授权 + 二次确认 + 审计
    InvokeResult invoke(const PluginId& pid, const InvokeEnvelope& env);

    // 为指定插件进程创建命名管道服务端
    Result<bool> startServerFor(Pid pid, const std::string& pipeName);

    // 等待插件进程连接到管道
    Result<bool> waitForPlugin(Pid pid, int timeoutMs = 5000);

    // 向插件进程发送 Invoke 并读回结果
    InvokeResult sendInvokeToPlugin(const PluginId& pid, Pid targetPid,
                                    const InvokeEnvelope& env);

    // 关闭指定插件进程的管道
    void closeServerFor(Pid pid);

private:
    void emitAudit(const PluginId& pid, const InvokeEnvelope& env,
                   const InvokeResult& r, int64_t t0);

    GrantStore& gs_;
    AuditLog& audit_;
    ConfirmationManager& confirm_;
    dm::ProcessManager& pm_;

    std::mutex mu_;
    std::map<std::string, Handler> handlers_;
    std::set<std::string> sensitive_;
    std::map<Pid, std::unique_ptr<dm::ipc::Pipe>> servers_;
};