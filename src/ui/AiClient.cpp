#include "ui/AiClient.h"
#include <windows.h>
#include <winhttp.h>
#include <sstream>
#include <vector>

#pragma comment(lib, "winhttp.lib")

namespace dm::ui {

// ============================================================
// 内部工具
// ============================================================

static bool parseEndpoint(const std::string& url,
                          std::wstring& host,
                          INTERNET_PORT& port,
                          bool& https) {
    if (url.empty()) return false;
    std::wstring wurl(url.begin(), url.end());
    URL_COMPONENTS uc{};
    uc.dwStructSize = sizeof(uc);
    wchar_t hostBuf[256]{};
    uc.lpszHostName = hostBuf;
    uc.dwHostNameLength = 255;
    if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &uc)) return false;
    host.assign(hostBuf, uc.dwHostNameLength);
    port = uc.nPort;
    https = (uc.nScheme == INTERNET_SCHEME_HTTPS);
    return true;
}

static std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

// 从一行 Ollama 响应 JSON 里抠出 message.content
static std::string extractContent(const std::string& json) {
    auto pos = json.find("\"content\":\"");
    if (pos == std::string::npos) return "";
    pos += 11;
    std::string out;
    bool esc = false;
    for (size_t i = pos; i < json.size(); ++i) {
        char c = json[i];
        if (esc) {
            switch (c) {
                case 'n': out += '\n'; break;
                case 't': out += '\t'; break;
                case 'r': out += '\r'; break;
                case '"': out += '"';  break;
                case '\\': out += '\\'; break;
                case '/': out += '/';  break;
                default:  out += c;    break;
            }
            esc = false;
        } else if (c == '\\') {
            esc = true;
        } else if (c == '"') {
            break;
        } else {
            out += c;
        }
    }
    return out;
}

// ============================================================
// chatStream
// ============================================================

bool AiClient::chatStream(
    const std::vector<std::pair<std::string, std::string>>& history,
    const std::string& userMessage,
    std::function<void(const std::string&)> onChunk,
    std::function<void(const std::string&)> onError) {

    AiConfig cfg = config();
    if (cfg.engine == AiEngine::Ollama) {
        return chatOllama(history, userMessage, onChunk, onError);
    }
    onError("不支持的 AI 引擎");
    return false;
}

bool AiClient::chatOllama(
    const std::vector<std::pair<std::string, std::string>>& history,
    const std::string& userMessage,
    std::function<void(const std::string&)> onChunk,
    std::function<void(const std::string&)> onError) {

    AiConfig cfg = config();

    std::wstring host;
    INTERNET_PORT port = 11434;
    bool https = false;
    if (!parseEndpoint(cfg.endpoint, host, port, https)) {
        onError("Ollama 地址解析失败: " + cfg.endpoint);
        return false;
    }

    HINTERNET session = WinHttpOpen(L"DM-Browser/0.1",
        WINHTTP_ACCESS_TYPE_NO_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) { onError("WinHttpOpen 失败"); return false; }

    HINTERNET conn = WinHttpConnect(session, host.c_str(), port, 0);
    if (!conn) {
        WinHttpCloseHandle(session);
        onError("无法连接 " + cfg.endpoint + " (Ollama 是否在运行?)");
        return false;
    }

    HINTERNET req = WinHttpOpenRequest(conn, L"POST", L"/api/chat",
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        https ? WINHTTP_FLAG_SECURE : 0);
    if (!req) {
        WinHttpCloseHandle(conn);
        WinHttpCloseHandle(session);
        onError("WinHttpOpenRequest 失败");
        return false;
    }

    std::ostringstream body;
    body << "{\"model\":\"" << jsonEscape(cfg.model) << "\","
         << "\"stream\":true,\"messages\":[";
    bool first = true;
    for (const auto& h : history) {
        if (!first) body << ",";
        first = false;
        body << "{\"role\":\"" << jsonEscape(h.first) << "\","
             << "\"content\":\"" << jsonEscape(h.second) << "\"}";
    }
    if (!first) body << ",";
    body << "{\"role\":\"user\",\"content\":\"" << jsonEscape(userMessage) << "\"}]}";
    std::string bodyStr = body.str();

    const wchar_t* headers = L"Content-Type: application/json\r\n";

    if (!WinHttpSendRequest(req, headers, -1L,
            (LPVOID)bodyStr.c_str(), (DWORD)bodyStr.size(),
            (DWORD)bodyStr.size(), 0)) {
        WinHttpCloseHandle(req);
        WinHttpCloseHandle(conn);
        WinHttpCloseHandle(session);
        onError("发送请求失败 (Ollama 是否在运行?)");
        return false;
    }

    if (!WinHttpReceiveResponse(req, nullptr)) {
        WinHttpCloseHandle(req);
        WinHttpCloseHandle(conn);
        WinHttpCloseHandle(session);
        onError("接收响应失败");
        return false;
    }

    DWORD statusCode = 0, scLen = sizeof(statusCode);
    WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &scLen, WINHTTP_NO_HEADER_INDEX);
    if (statusCode != 200) {
        WinHttpCloseHandle(req);
        WinHttpCloseHandle(conn);
        WinHttpCloseHandle(session);
        onError("Ollama 返回 HTTP " + std::to_string(statusCode) +
                " (模型 " + cfg.model + " 是否已安装?)");
        return false;
    }

    std::string buffer;
    DWORD bytesAvail = 0;
    bool gotError = false;

    while (WinHttpQueryDataAvailable(req, &bytesAvail) && bytesAvail > 0) {
        std::vector<char> chunk(bytesAvail);
        DWORD read = 0;
        if (!WinHttpReadData(req, chunk.data(), bytesAvail, &read)) break;
        if (read == 0) break;
        buffer.append(chunk.data(), read);

        size_t nl;
        while ((nl = buffer.find('\n')) != std::string::npos) {
            std::string line = buffer.substr(0, nl);
            buffer.erase(0, nl + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;

            auto errPos = line.find("\"error\":\"");
            if (errPos != std::string::npos) {
                std::string err = extractContent(line);
                onError("Ollama: " + err);
                gotError = true;
                break;
            }

            std::string content = extractContent(line);
            if (!content.empty() && onChunk) onChunk(content);

            if (line.find("\"done\":true") != std::string::npos) break;
        }
        if (gotError) break;
    }

    WinHttpCloseHandle(req);
    WinHttpCloseHandle(conn);
    WinHttpCloseHandle(session);
    return !gotError;
}

// ============================================================
// listModels
// ============================================================

std::vector<std::string> AiClient::listModels() {
    std::vector<std::string> out;
    AiConfig cfg = config();

    std::wstring host;
    INTERNET_PORT port = 11434;
    bool https = false;
    if (!parseEndpoint(cfg.endpoint, host, port, https)) return out;

    HINTERNET session = WinHttpOpen(L"DM-Browser/0.1",
        WINHTTP_ACCESS_TYPE_NO_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) return out;

    HINTERNET conn = WinHttpConnect(session, host.c_str(), port, 0);
    if (!conn) { WinHttpCloseHandle(session); return out; }

    HINTERNET req = WinHttpOpenRequest(conn, L"GET", L"/api/tags",
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        https ? WINHTTP_FLAG_SECURE : 0);
    if (!req) {
        WinHttpCloseHandle(conn);
        WinHttpCloseHandle(session);
        return out;
    }

    if (WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(req, nullptr)) {
        std::string body;
        DWORD bytesAvail = 0;
        while (WinHttpQueryDataAvailable(req, &bytesAvail) && bytesAvail > 0) {
            std::vector<char> chunk(bytesAvail);
            DWORD read = 0;
            if (!WinHttpReadData(req, chunk.data(), bytesAvail, &read)) break;
            if (read == 0) break;
            body.append(chunk.data(), read);
        }
        size_t pos = 0;
        const std::string key = "\"name\":\"";
        while ((pos = body.find(key, pos)) != std::string::npos) {
            pos += key.size();
            size_t end = body.find('"', pos);
            if (end == std::string::npos) break;
            out.push_back(body.substr(pos, end - pos));
            pos = end + 1;
        }
    }

    WinHttpCloseHandle(req);
    WinHttpCloseHandle(conn);
    WinHttpCloseHandle(session);
    return out;
}

} // namespace dm::ui