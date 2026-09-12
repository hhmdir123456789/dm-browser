#pragma once
#include <functional>
#include <mutex>
#include <set>
#include <string>

class ConfirmationManager {
public:
    enum class Decision { Denied, Once, Always };
    using PromptFn = std::function<Decision(const std::string&, const std::string&,
                                             const std::string&)>;

    void setPrompt(PromptFn fn) { prompt_ = std::move(fn); }

    bool confirm(const std::string& pid, const std::string& ep,
                 const std::string& origin) {
        std::lock_guard lock(mu_);
        std::string key = pid + "|" + ep + "|" + origin;
        if (alwaysAllowed_.count(key)) return true;
        if (alwaysDenied_.count(key)) return false;
        Decision d = prompt_ ? prompt_(pid, ep, origin) : Decision::Denied;
        if (d == Decision::Always) { alwaysAllowed_.insert(key); return true; }
        if (d == Decision::Once) return true;
        alwaysDenied_.insert(key);
        return false;
    }

    void reset(const std::string& pid) {
        std::lock_guard lock(mu_);
        for (auto it = alwaysAllowed_.begin(); it != alwaysAllowed_.end();) {
            if (it->substr(0, pid.size()) == pid) it = alwaysAllowed_.erase(it);
            else ++it;
        }
        for (auto it = alwaysDenied_.begin(); it != alwaysDenied_.end();) {
            if (it->substr(0, pid.size()) == pid) it = alwaysDenied_.erase(it);
            else ++it;
        }
    }

private:
    mutable std::mutex mu_;
    std::set<std::string> alwaysAllowed_;
    std::set<std::string> alwaysDenied_;
    PromptFn prompt_;
};
