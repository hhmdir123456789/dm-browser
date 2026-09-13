#pragma once
#include "learn/snapshot.h"
#include <string>

namespace dm::learn {

// 从 JSON 字符串解析 PageSnapshot
PageSnapshot parseSnapshotJson(const std::string& json);

// 从文件解析
PageSnapshot parseSnapshotFile(const std::string& path);

// 序列化回 JSON（用于生成测试样本）
std::string serializeSnapshot(const PageSnapshot& s);

} // namespace dm::learn