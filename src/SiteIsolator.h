#pragma once
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "common/Types.h"

namespace dm {

class SiteIsolator {
public:
    // 解析 origin：scheme://eTLD+1
    static std::string normalizeOrigin(const std::string& url);

    // 判断两个 origin 是否同站
    static bool sameSite(const std::string& a, const std::string& b);

    // 导航时决定使用哪个渲染进程
    // 返回已有进程，或 0 表示需要新进程
    Pid resolveRenderer(const std::string& url) const;

    // 绑定 site -> pid
    void bind(const std::string& site, Pid pid);

    // 敏感站点
    void addSensitiveSite(const std::string& site);
    bool isSensitive(const std::string& url) const;

    // 进程退出时清理
    void onProcessTerminated(Pid pid);

    // 统计
    size_t siteCount() const;
    size_t sensitiveCount() const;

private:
    mutable std::mutex mu_;
    std::map<std::string, Pid> siteToPid_;
    std::set<std::string> sensitiveSites_;
};

} // namespace dm