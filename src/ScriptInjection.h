#pragma once
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include "common/Types.h"
#include "common/Error.h"
#include "common/Result.h"

enum class InjectTiming { DocumentStart, DocumentEnd };

class ScriptInjectionManager {
public:
    Result<int64_t> inject(TabId tabId, const std::string& origin,
                           const std::string& script, InjectTiming) {
        std::lock_guard lock(mu_);
        int64_t now = nowMs();
        auto& times = injectTimes_[tabId];
        while (!times.empty() && now - times.front() > 1000) times.pop_front();
        if (times.size() >= 8)
            return Result<int64_t>::fail(
                Error::res(ErrCode::BudgetExceeded, "injection frequency exceeded"));
        times.push_back(now);
        total_++;
        return Result<int64_t>::ok(static_cast<int64_t>(total_));
    }

    size_t countByTab(TabId tabId) const {
        std::lock_guard lock(mu_);
        auto it = injectTimes_.find(tabId);
        return it == injectTimes_.end() ? 0 : it->second.size();
    }

    size_t total() const { std::lock_guard lock(mu_); return total_; }

    void onTabClosed(TabId tabId) {
        std::lock_guard lock(mu_);
        injectTimes_.erase(tabId);
    }

private:
    mutable std::mutex mu_;
    std::map<TabId, std::deque<int64_t>> injectTimes_;
    size_t total_{0};
};
