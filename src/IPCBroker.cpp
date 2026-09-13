#include "IPCBroker.h"
#include <iostream>

// ============================================================
// 进程内调用
// ============================================================
InvokeResult IPCBroker::invoke(const PluginId& pid, const InvokeEnvelope& env) {
    int64_t t0 = nowMs();
    InvokeResult r;

    if (!gs_.validate(env.grantId, env.endpoint)) {
        r.ok = false;
        r.error = Error::auth(ErrCode::GrantNotFound, "grant invalid");
        emitAudit(pid, env, r, t0);
        return r;
    }

    {
        std::lock_guard lock(mu_);
        if (sensitive_.count(env.endpoint)) {
            if (!confirm_.confirm(pid, env.endpoint, env.origin)) {
                r.ok = false;
                r.error = Error::scope(ErrCode::GestureRequired, "denied");
                emitAudit(pid, env, r, t0);
                return r;
            }
        }
    }

    Handler h;
    {
        std::lock_guard lock(mu_);
        auto it = handlers_.find(env.endpoint);
        if (it == handlers_.end()) {
            r.ok = false;
            r.error = Error::internal("no handler");
            emitAudit(pid, env, r, t0);
            return r;
        }
        h = it->second;
    }
    r = h(env);
    emitAudit(pid, env, r, t0);
    return r;
}

// ============================================================
// 为插件进程创建管道服务端
// ============================================================
Result<bool> IPCBroker::startServerFor(Pid pid, const std::string& pipeName) {
    auto pipe = std::make_unique<dm::ipc::Pipe>();
    auto r = pipe->createServer(pipeName);
    if (!r.isOk()) return r;
    std::lock_guard lock(mu_);
    servers_[pid] = std::move(pipe);
    return Result<bool>::ok(true);
}

// ============================================================
// 等待插件连接，并消费 Hello 帧
// ============================================================
Result<bool> IPCBroker::waitForPlugin(Pid pid, int /*timeoutMs*/) {
    dm::ipc::Pipe* pipe = nullptr;
    {
        std::lock_guard lock(mu_);
        auto it = servers_.find(pid);
        if (it == servers_.end())
            return Result<bool>::fail(Error::internal("no server for pid"));
        pipe = it->second.get();
    }

    auto conn = pipe->waitForClient();
    if (!conn.isOk()) return conn;

    // 消费插件发来的第一帧（Hello）
    auto helloFrame = dm::ipc::Framing::readFrame(*pipe);
    if (helloFrame.isOk()) {
        auto kind = dm::ipc::Framing::peekKind(helloFrame.value());
        if (kind == dm::ipc::Framing::Kind::Hello) {
            auto hello = dm::ipc::Framing::decodeHello(helloFrame.value());
            if (hello.isOk()) {
                std::cerr << "[IPC] 收到 Hello, plugin="
                          << hello.value().pluginId << "\n";
            }
        }
    }
    return Result<bool>::ok(true);
}

// ============================================================
// 主进程 → 插件进程：走授权校验的 Invoke
// ============================================================
InvokeResult IPCBroker::sendInvokeToPlugin(
    const PluginId& pid, Pid targetPid, const InvokeEnvelope& env) {

    std::cerr << "[IPC] sendInvokeToPlugin 开始\n";

    if (!gs_.validate(env.grantId, env.endpoint)) {
        std::cerr << "[IPC] 授权校验失败\n";
        InvokeResult out;
        out.ok = false;
        out.error = Error::auth(ErrCode::GrantNotFound, "grant invalid");
        return out;
    }
    std::cerr << "[IPC] 授权校验通过\n";

    dm::ipc::Pipe* pipe = nullptr;
    {
        std::lock_guard lock(mu_);
        auto it = servers_.find(targetPid);
        if (it == servers_.end()) {
            std::cerr << "[IPC] 找不到管道\n";
            InvokeResult out;
            out.ok = false;
            out.error = Error::internal("no server pipe for pid");
            return out;
        }
        pipe = it->second.get();
    }
    std::cerr << "[IPC] 找到管道\n";

    auto payload = dm::ipc::Framing::encodeInvoke(
        env.grantId, env.endpoint, env.method, env.args, env.traceId, env.origin);
    std::cerr << "[IPC] 编码完成, " << payload.size() << " 字节\n";

    auto w = dm::ipc::Framing::writeFrame(*pipe, payload);
    if (!w.isOk()) {
        std::cerr << "[IPC] writeFrame 失败: " << w.error().msg << "\n";
        InvokeResult out;
        out.ok = false;
        out.error = Error::internal("write frame failed: " + w.error().msg);
        return out;
    }
    std::cerr << "[IPC] writeFrame 成功\n";

    // 循环读帧，跳过 Hello 等非 Result 消息
    dm::ipc::Framing::ResultMessage resultMsg;
    bool gotResult = false;
    for (int attempt = 0; attempt < 10; ++attempt) {
        auto response = dm::ipc::Framing::readFrame(*pipe);
        if (!response.isOk()) {
            std::cerr << "[IPC] readFrame 失败: " << response.error().msg << "\n";
            InvokeResult out;
            out.ok = false;
            out.error = Error::internal("read frame failed: " + response.error().msg);
            return out;
        }
        std::cerr << "[IPC] readFrame 成功, " << response.value().size() << " 字节\n";

        auto kind = dm::ipc::Framing::peekKind(response.value());
        if (kind == dm::ipc::Framing::Kind::Hello) {
            std::cerr << "[IPC] 跳过 Hello 帧\n";
            continue;
        }
        if (kind == dm::ipc::Framing::Kind::Result) {
            auto result = dm::ipc::Framing::decodeResult(response.value());
            if (!result.isOk()) {
                std::cerr << "[IPC] decodeResult 失败: " << result.error().msg << "\n";
                InvokeResult out;
                out.ok = false;
                out.error = Error::internal("decode result failed: " + result.error().msg);
                return out;
            }
            resultMsg = result.value();
            gotResult = true;
            break;
        }
        std::cerr << "[IPC] 未知帧类型: " << static_cast<int>(kind) << "\n";
    }

    if (!gotResult) {
        InvokeResult out;
        out.ok = false;
        out.error = Error::internal("no result frame received");
        return out;
    }

    std::cerr << "[IPC] decodeResult 成功, ok=" << resultMsg.ok << "\n";

    InvokeResult out;
    out.ok = resultMsg.ok;
    out.value = resultMsg.value;
    if (!out.ok) out.error = Error::internal(resultMsg.errorMsg);
    return out;
}

// ============================================================
// 主进程 → 插件进程：控制通道，不走授权校验
// ============================================================
InvokeResult IPCBroker::sendControlToPlugin(
    const PluginId& pid, Pid targetPid,
    const std::string& method, const std::string& args) {
    (void)pid;

    dm::ipc::Pipe* pipe = nullptr;
    {
        std::lock_guard lock(mu_);
        auto it = servers_.find(targetPid);
        if (it == servers_.end()) {
            InvokeResult out;
            out.ok = false;
            out.error = Error::internal("no server pipe for pid");
            return out;
        }
        pipe = it->second.get();
    }

    // 编码时用空 grantId，插件侧不校验（控制通道）
    auto payload = dm::ipc::Framing::encodeInvoke(
        "", "control", method, args, "t-ctrl", "");
    auto w = dm::ipc::Framing::writeFrame(*pipe, payload);
    if (!w.isOk()) {
        InvokeResult out;
        out.ok = false;
        out.error = Error::internal("write failed: " + w.error().msg);
        return out;
    }

    auto response = dm::ipc::Framing::readFrame(*pipe);
    if (!response.isOk()) {
        InvokeResult out;
        out.ok = false;
        out.error = Error::internal("read failed: " + response.error().msg);
        return out;
    }

    auto result = dm::ipc::Framing::decodeResult(response.value());
    if (!result.isOk()) {
        InvokeResult out;
        out.ok = false;
        out.error = Error::internal("decode failed: " + result.error().msg);
        return out;
    }

    InvokeResult out;
    out.ok = result.value().ok;
    out.value = result.value().value;
    if (!out.ok) out.error = Error::internal(result.value().errorMsg);
    return out;
}

// ============================================================
// 关闭管道
// ============================================================
void IPCBroker::closeServerFor(Pid pid) {
    std::lock_guard lock(mu_);
    servers_.erase(pid);
}

// ============================================================
// 审计
// ============================================================
void IPCBroker::emitAudit(const PluginId& pid, const InvokeEnvelope& env,
                          const InvokeResult& r, int64_t t0) {
    AuditEntry e;
    e.traceId = env.traceId;
    e.pluginId = pid;
    e.endpoint = env.endpoint;
    e.method = env.method;
    e.result = r.ok ? "ok" : "error";
    e.errorCode = r.ok ? "" : std::to_string(static_cast<int>(r.error.code));
    e.durationMs = nowMs() - t0;
    e.createdAt = nowMs();
    audit_.append(e);
}