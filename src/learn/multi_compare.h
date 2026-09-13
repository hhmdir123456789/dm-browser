#pragma once
#include "learn/snapshot.h"

namespace dm::learn {

class MultiCompare {
public:
    // 按 path 对齐两棵 DOM，输出多维差异
    static MultiDimResult compare(const PageSnapshot& ref,
                                  const PageSnapshot& dm);
};

} // namespace dm::learn