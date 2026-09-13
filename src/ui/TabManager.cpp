#include "ui/TabManager.h"
#include <algorithm>

namespace dm::ui {

int TabManager::create() {
    Tab t;
    t.id = nextId_++;
    tabs_.push_back(t);
    if (activeId_ == 0) activeId_ = t.id;
    return t.id;
}

bool TabManager::close(int id) {
    auto it = std::find_if(tabs_.begin(), tabs_.end(),
        [id](const Tab& t) { return t.id == id; });
    if (it == tabs_.end()) return false;

    bool wasActive = (it->id == activeId_);
    tabs_.erase(it);

    if (wasActive) {
        activeId_ = tabs_.empty() ? 0 : tabs_.front().id;
    }
    return true;
}

bool TabManager::activate(int id) {
    auto it = std::find_if(tabs_.begin(), tabs_.end(),
        [id](const Tab& t) { return t.id == id; });
    if (it == tabs_.end()) return false;
    activeId_ = id;
    return true;
}

Tab* TabManager::get(int id) {
    for (auto& t : tabs_) if (t.id == id) return &t;
    return nullptr;
}

Tab* TabManager::active() {
    return get(activeId_);
}

} // namespace dm::ui