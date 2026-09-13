#include "ui/TabManager.h"
#include <algorithm>
#include <iterator>

namespace dm::ui {

void TabManager::fixActiveId() {
    if (tabs_.empty()) { activeId_ = 0; return; }
    auto it = std::find_if(tabs_.begin(), tabs_.end(),
        [this](const std::shared_ptr<Tab>& t) { return t->id == activeId_; });
    if (it == tabs_.end()) activeId_ = tabs_.front()->id;
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
        if (tabs_.empty()) activeId_ = 0;
        else if (idx < tabs_.size()) activeId_ = tabs_[idx]->id;
        else activeId_ = tabs_.back()->id;
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

bool TabManager::setPinned(int64_t id, bool pinned) {
    std::lock_guard lock(mu_);
    for (auto& t : tabs_) {
        if (t->id == id) { t->pinned = pinned; return true; }
    }
    return false;
}

bool TabManager::setDiscarded(int64_t id, bool discarded) {
    std::lock_guard lock(mu_);
    for (auto& t : tabs_) {
        if (t->id == id) { t->discarded = discarded; return true; }
    }
    return false;
}

int TabManager::closeOthers(int64_t id) {
    std::lock_guard lock(mu_);
    int n = 0;
    auto newEnd = std::remove_if(tabs_.begin(), tabs_.end(),
        [id, &n](const std::shared_ptr<Tab>& t) {
            if (t->id != id && !t->pinned) { n++; return true; }
            return false;
        });
    tabs_.erase(newEnd, tabs_.end());
    fixActiveId();
    return n;
}

int TabManager::closeRight(int64_t id) {
    std::lock_guard lock(mu_);
    auto it = std::find_if(tabs_.begin(), tabs_.end(),
        [id](const std::shared_ptr<Tab>& t) { return t->id == id; });
    if (it == tabs_.end()) return 0;
    size_t idx = std::distance(tabs_.begin(), it);
    int n = 0;
    auto newEnd = std::remove_if(tabs_.begin() + idx + 1, tabs_.end(),
        [&n](const std::shared_ptr<Tab>& t) {
            if (!t->pinned) { n++; return true; }
            return false;
        });
    tabs_.erase(newEnd, tabs_.end());
    fixActiveId();
    return n;
}

void TabManager::groupByDomain() {
    std::lock_guard lock(mu_);
    for (auto& t : tabs_) {
        if (t->url.empty()) { t->group.clear(); continue; }
        std::wstring url = t->url;
        auto pos = url.find(L"://");
        if (pos != std::wstring::npos) url = url.substr(pos + 3);
        pos = url.find(L'/');
        if (pos != std::wstring::npos) url = url.substr(0, pos);
        // 去掉 www.
        if (url.rfind(L"www.", 0) == 0) url = url.substr(4);
        t->group = url;
    }
}

} // namespace dm::ui