#pragma once
#include <map>
#include <optional>
#include <vector>
#include "common/Types.h"

struct TabInfo {
    TabId id{0};
    std::string origin;
    std::string title;
    bool active{false};
};

class TabManager {
public:
    TabId create(const std::string& origin) {
        TabId id = nextId_++;
        TabInfo t;
        t.id = id; t.origin = origin; t.title = origin;
        if (tabs_.empty()) t.active = true;
        tabs_[id] = t;
        if (t.active) activeId_ = id;
        return id;
    }
    bool close(TabId id) {
        auto it = tabs_.find(id);
        if (it == tabs_.end()) return false;
        bool wasActive = it->second.active;
        tabs_.erase(it);
        if (wasActive) {
            activeId_ = 0;
            if (!tabs_.empty()) {
                tabs_.begin()->second.active = true;
                activeId_ = tabs_.begin()->first;
            }
        }
        return true;
    }
    bool activate(TabId id) {
        auto it = tabs_.find(id);
        if (it == tabs_.end()) return false;
        for (auto& [tid, t] : tabs_) t.active = false;
        it->second.active = true;
        activeId_ = id;
        return true;
    }
    std::optional<TabInfo> get(TabId id) const {
        auto it = tabs_.find(id);
        if (it == tabs_.end()) return std::nullopt;
        return it->second;
    }
    std::vector<TabInfo> list() const {
        std::vector<TabInfo> out;
        for (const auto& [id, t] : tabs_) out.push_back(t);
        return out;
    }
    size_t count() const { return tabs_.size(); }
    TabId activeId() const { return activeId_; }
private:
    std::map<TabId, TabInfo> tabs_;
    TabId nextId_{1};
    TabId activeId_{0};
};
