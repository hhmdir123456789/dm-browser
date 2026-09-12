#include "SiteIsolator.h"
#include <algorithm>

namespace dm {

std::string SiteIsolator::normalizeOrigin(const std::string& url) {
    // 找 scheme://
    auto schemeEnd = url.find("://");
    if (schemeEnd == std::string::npos) return url;
    std::string scheme = url.substr(0, schemeEnd);
    std::string rest = url.substr(schemeEnd + 3);

    // 找 host 结束
    auto pathStart = rest.find('/');
    std::string host = (pathStart == std::string::npos)
                       ? rest : rest.substr(0, pathStart);

    // 去掉端口
    auto colon = host.find(':');
    if (colon != std::string::npos) host = host.substr(0, colon);

    // 去掉 www.
    if (host.rfind("www.", 0) == 0) host = host.substr(4);

    // 取 eTLD+1（简化：取最后两段）
    std::vector<std::string> parts;
    size_t start = 0;
    while (true) {
        auto dot = host.find('.', start);
        if (dot == std::string::npos) {
            parts.push_back(host.substr(start));
            break;
        }
        parts.push_back(host.substr(start, dot - start));
        start = dot + 1;
    }

    std::string etld1;
    if (parts.size() >= 2) {
        etld1 = parts[parts.size() - 2] + "." + parts[parts.size() - 1];
    } else {
        etld1 = host;
    }

    return scheme + "://" + etld1;
}

bool SiteIsolator::sameSite(const std::string& a, const std::string& b) {
    return normalizeOrigin(a) == normalizeOrigin(b);
}

Pid SiteIsolator::resolveRenderer(const std::string& url) const {
    std::lock_guard lock(mu_);
    std::string site = normalizeOrigin(url);

    // 敏感站点强制独立进程：查表命中则复用，未命中返回 0 要求新进程
    if (sensitiveSites_.count(site) > 0) {
        auto it = siteToPid_.find(site);
        return it == siteToPid_.end() ? 0 : it->second;
    }

    auto it = siteToPid_.find(site);
    return it == siteToPid_.end() ? 0 : it->second;
}

void SiteIsolator::bind(const std::string& site, Pid pid) {
    std::lock_guard lock(mu_);
    siteToPid_[normalizeOrigin(site)] = pid;
}

void SiteIsolator::addSensitiveSite(const std::string& site) {
    std::lock_guard lock(mu_);
    sensitiveSites_.insert(normalizeOrigin(site));
}

bool SiteIsolator::isSensitive(const std::string& url) const {
    std::lock_guard lock(mu_);
    return sensitiveSites_.count(normalizeOrigin(url)) > 0;
}

void SiteIsolator::onProcessTerminated(Pid pid) {
    std::lock_guard lock(mu_);
    for (auto it = siteToPid_.begin(); it != siteToPid_.end();) {
        if (it->second == pid) it = siteToPid_.erase(it);
        else ++it;
    }
}

size_t SiteIsolator::siteCount() const {
    std::lock_guard lock(mu_);
    return siteToPid_.size();
}

size_t SiteIsolator::sensitiveCount() const {
    std::lock_guard lock(mu_);
    return sensitiveSites_.size();
}

} // namespace dm