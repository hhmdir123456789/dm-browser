#include "ui/TabManager.h"
#include <algorithm>
#include <iterator>

namespace dm::ui {

void TabManager::fixActiveId() {
    if (tabs_.empty()) {
        activeId_ = 0;
        return;
    }
    auto it = std::find_if(tabs_.begin(), tabs_.end(),
        [this](const std::shared_ptr<Tab>& t) { return t->id == activeId_; });
    if (it == tabs_.end()) {
        activeId_ = tabs_.front()->id;
    }
}

int64_t TabManager::create() {
    std::lock_guard lock(mu_);
    auto t = std::make_shared<Tab>();
    t->id = nextId_++;
    tabs_.push_back(t);
    if (activeId_ == 0) activeId_ = t->id;
    return t->id;
}

bool TabManager::close(int64_t id) {
    std::lock_guard lock(mu_);
    auto it = std::find_if(tabs_.begin(), tabs_.end(),
        [id](const std::shared_ptr<Tab>& t) { return t->id == id; });
    if (it == tabs_.end()) return false;

    bool wasActive = ((*it)->id == activeId_);
    size_t idx = std::distance(tabs_.begin(), it);
    tabs_.erase(it);

    if (wasActive) {
        if (tabs_.empty()) {
            activeId_ = 0;
        } else if (idx < tabs_.size()) {
            activeId_ = tabs_[idx]->id;
        } else {
            activeId_ = tabs_.back()->id;
        }
    }
    fixActiveId();
    return true;
}

bool TabManager::activate(int64_t id) {
    std::lock_guard lock(mu_);
    auto it = std::find_if(tabs_.begin(), tabs_.end(),
        [id](const std::shared_ptr<Tab>& t) { return t->id == id; });
    if (it == tabs_.end()) return false;
    activeId_ = id;
    return true;
}

std::shared_ptr<Tab> TabManager::get(int64_t id) {
    std::lock_guard lock(mu_);
    for (auto& t : tabs_) if (t->id == id) return t;
    return nullptr;
}

std::shared_ptr<Tab> TabManager::active() {
    std::lock_guard lock(mu_);
    for (auto& t : tabs_) if (t->id == activeId_) return t;
    return nullptr;
}

std::vector<std::shared_ptr<Tab>> TabManager::all() const {
    std::lock_guard lock(mu_);
    return tabs_;
}

std::vector<std::shared_ptr<Tab>> TabManager::allMutable() {
    std::lock_guard lock(mu_);
    return tabs_;
}

size_t TabManager::count() const {
    std::lock_guard lock(mu_);
    return tabs_.size();
}

int64_t TabManager::activeId() const {
    std::lock_guard lock(mu_);
    return activeId_;
}

} // namespace dm::ui