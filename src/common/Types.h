#pragma once
#include <chrono>
#include <cstdint>
#include <string>

using PluginId = std::string;
using GrantId  = std::string;
using TraceId  = std::string;
using TabId    = int32_t;
using BookmarkId = int32_t;
using DownloadId = int32_t;
using CaseId   = std::string;
using Pid      = int32_t;

static inline int64_t nowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}
