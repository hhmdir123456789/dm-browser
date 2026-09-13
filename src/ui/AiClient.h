#pragma once
#include <string>
#include <functional>
#include <vector>
#include <mutex>

namespace dm::ui {

// AI 引擎类型。当前只支持 Ollama，云端引擎以后作为插件扩展。
enum class AiEngine {
    Ollama = 0,
};

struct AiConfig {
    AiEngine engine = AiEngine::Ollama;
    std::string endpoint = "http://localhost:11434";
    std::string model    = "llama3.2";
};

// 本地 Ollama 客户端。
// 通过 C++ WinHTTP 代理请求，绕开浏览器 CORS 限制。
// 调用方在后台线程调用 chatStream，回调也在该线程执行。
class AiClient {
public:
    AiClient() = default;

    void setConfig(const AiConfig& cfg) {
        std::lock_guard lock(mu_);
        cfg_ = cfg;
    }

    AiConfig config() const {
        std::lock_guard lock(mu_);
        return cfg_;
    }

    // 流式对话。
    // history: 历史消息 {role, content}，role 取值 "user"/"assistant"/"system"
    // userMessage: 本轮用户输入
    // onChunk: 每收到一段增量文本回调一次
    // onError: 出错时回调，返回错误描述
    // 返回：是否成功完成
    bool chatStream(
        const std::vector<std::pair<std::string, std::string>>& history,
        const std::string& userMessage,
        std::function<void(const std::string&)> onChunk,
        std::function<void(const std::string&)> onError);

    // 检测 Ollama 是否在运行，返回可用模型列表。失败时返回空。
    std::vector<std::string> listModels();

private:
    bool chatOllama(
        const std::vector<std::pair<std::string, std::string>>& history,
        const std::string& userMessage,
        std::function<void(const std::string&)> onChunk,
        std::function<void(const std::string&)> onError);

    mutable std::mutex mu_;
    AiConfig cfg_;
};

} // namespace dm::ui