#include "learn/multi_compare.h"
#include <cmath>
#include <map>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace dm::learn {

// ============================================================
// 颜色归一化：把各种格式统一成 "r,g,b" 或 "r,g,b,a"
// 支持：#RGB / #RRGGBB / rgb(r,g,b) / rgba(r,g,b,a) / "r,g,b" / "r,g,b,a"
// 已经是 "r,g,b" 或 "r,g,b,a" 的原样返回
// ============================================================
static std::string normColor(const std::string& s) {
    if (s.empty()) return s;

    // 已经是 "r,g,b" 或 "r,g,b,a"
    if (s.find(',') != std::string::npos) return s;

    // #RRGGBB
    if (s[0] == '#' && s.size() >= 7) {
        unsigned int r = 0, g = 0, b = 0;
        std::sscanf(s.c_str() + 1, "%02x%02x%02x", &r, &g, &b);
        return std::to_string(r) + "," +
               std::to_string(g) + "," +
               std::to_string(b);
    }

    // #RGB
    if (s[0] == '#' && s.size() >= 4) {
        char rs[2] = {s[1], s[1]};
        char gs[2] = {s[2], s[2]};
        char bs[2] = {s[3], s[3]};
        unsigned int r = 0, g = 0, b = 0;
        std::sscanf(rs, "%x", &r);
        std::sscanf(gs, "%x", &g);
        std::sscanf(bs, "%x", &b);
        return std::to_string(r) + "," +
               std::to_string(g) + "," +
               std::to_string(b);
    }

    // rgb(...) / rgba(...)
    if (s.find("rgb") != std::string::npos) {
        const char* p = s.c_str();
        while (*p && *p != '(') p++;
        if (*p == '(') p++;
        int r = 0, g = 0, b = 0;
        std::sscanf(p, "%d , %d , %d", &r, &g, &b);
        return std::to_string(r) + "," +
               std::to_string(g) + "," +
               std::to_string(b);
    }

    return s;
}

// ============================================================
// 结构维度
// ============================================================
static void compareStructural(
    const std::vector<NodeSnapshot>& refNodes,
    const std::vector<NodeSnapshot>& dmNodes,
    MultiDimResult& out) {

    std::map<std::string, const NodeSnapshot*> refMap, dmMap;
    for (auto& n : refNodes) refMap[n.path] = &n;
    for (auto& n : dmNodes)  dmMap[n.path] = &n;

    // 参考有、我们缺
    for (auto& kv : refMap) {
        if (dmMap.find(kv.first) == dmMap.end()) {
            DimDiff d;
            d.dimension = "structural";
            d.category = "missing_node";
            d.path = kv.first;
            d.property = "existence";
            d.refValue = kv.second->tag;
            d.dmValue = "(缺失)";
            d.severity = kv.second->depth >= 5 ? 4
                       : (kv.second->depth >= 3 ? 3 : 2);
            d.description = "缺少 " + kv.second->tag + " 节点";
            out.diffs.push_back(d);
            out.structuralDiffCount++;
        }
    }

    // 我们有、参考没有
    for (auto& kv : dmMap) {
        if (refMap.find(kv.first) == refMap.end()) {
            DimDiff d;
            d.dimension = "structural";
            d.category = "extra_node";
            d.path = kv.first;
            d.property = "existence";
            d.refValue = "(无)";
            d.dmValue = kv.second->tag;
            d.severity = 2;
            d.description = "多余 " + kv.second->tag + " 节点";
            out.diffs.push_back(d);
            out.structuralDiffCount++;
        }
    }

    int total = (int)(refMap.size() > dmMap.size() ? refMap.size() : dmMap.size());
    if (total > 0) {
        out.structuralScore = 1.0 - (double)out.structuralDiffCount / total;
        if (out.structuralScore < 0) out.structuralScore = 0;
    }
}

// ============================================================
// 样式维度
// ============================================================
static void compareStyle(
    const NodeSnapshot& ref, const NodeSnapshot& dm,
    MultiDimResult& out) {

    auto addDiff = [&](const std::string& prop, const std::string& rv,
                       const std::string& dv, int sev) {
        DimDiff d;
        d.dimension = "style";
        d.category = prop + "_diff";
        d.path = ref.path;
        d.property = prop;
        d.refValue = rv;
        d.dmValue = dv;
        d.severity = sev;
        d.description = prop + " 不一致";
        out.diffs.push_back(d);
        out.styleDiffCount++;
    };

    // 颜色类属性先归一化再比较
    if (!ref.style.color.empty() &&
        normColor(ref.style.color) != normColor(dm.style.color))
        addDiff("color", ref.style.color, dm.style.color, 3);

    if (!ref.style.backgroundColor.empty() &&
        normColor(ref.style.backgroundColor) != normColor(dm.style.backgroundColor) &&
        normColor(ref.style.backgroundColor) != "0,0,0,0" &&
        normColor(ref.style.backgroundColor) != "0,0,0")
        addDiff("backgroundColor",
                ref.style.backgroundColor, dm.style.backgroundColor, 3);

    if (ref.style.fontSize != dm.style.fontSize)
        addDiff("fontSize", ref.style.fontSize, dm.style.fontSize, 4);
    if (ref.style.fontWeight != dm.style.fontWeight)
        addDiff("fontWeight", ref.style.fontWeight, dm.style.fontWeight, 2);
    if (ref.style.display != dm.style.display)
        addDiff("display", ref.style.display, dm.style.display, 5);
    if (ref.style.position != dm.style.position)
        addDiff("position", ref.style.position, dm.style.position, 4);

    if (ref.style.marginTop != dm.style.marginTop)
        addDiff("marginTop", std::to_string(ref.style.marginTop),
                std::to_string(dm.style.marginTop), 2);
    if (ref.style.marginBottom != dm.style.marginBottom)
        addDiff("marginBottom", std::to_string(ref.style.marginBottom),
                std::to_string(dm.style.marginBottom), 2);
    if (ref.style.paddingLeft != dm.style.paddingLeft)
        addDiff("paddingLeft", std::to_string(ref.style.paddingLeft),
                std::to_string(dm.style.paddingLeft), 2);
    if (ref.style.paddingRight != dm.style.paddingRight)
        addDiff("paddingRight", std::to_string(ref.style.paddingRight),
                std::to_string(dm.style.paddingRight), 2);
}

// ============================================================
// 布局维度
// ============================================================
static void compareLayout(
    const NodeSnapshot& ref, const NodeSnapshot& dm,
    MultiDimResult& out) {

    auto addDiff = [&](const std::string& prop, float rv, float dv, int sev) {
        DimDiff d;
        d.dimension = "layout";
        d.category = prop + "_diff";
        d.path = ref.path;
        d.property = prop;
        d.refValue = std::to_string((int)rv);
        d.dmValue = std::to_string((int)dv);
        d.severity = sev;
        d.description = prop + " 偏差 " +
            std::to_string((int)std::abs(rv - dv)) + "px";
        out.diffs.push_back(d);
        out.layoutDiffCount++;
    };

    const float posTol = 5.0f;
    const float sizeTol = 5.0f;

    if (std::abs(ref.layout.x - dm.layout.x) > posTol)
        addDiff("x", ref.layout.x, dm.layout.x, 3);
    if (std::abs(ref.layout.y - dm.layout.y) > posTol)
        addDiff("y", ref.layout.y, dm.layout.y, 3);
    if (std::abs(ref.layout.w - dm.layout.w) > sizeTol)
        addDiff("width", ref.layout.w, dm.layout.w, 4);
    if (std::abs(ref.layout.h - dm.layout.h) > sizeTol)
        addDiff("height", ref.layout.h, dm.layout.h, 4);
}

// ============================================================
// 主入口
// ============================================================
MultiDimResult MultiCompare::compare(
    const PageSnapshot& ref, const PageSnapshot& dm) {

    MultiDimResult result;
    result.url = ref.url;

    compareStructural(ref.nodes, dm.nodes, result);

    std::map<std::string, const NodeSnapshot*> refMap, dmMap;
    for (auto& n : ref.nodes) refMap[n.path] = &n;
    for (auto& n : dm.nodes)  dmMap[n.path] = &n;

    for (auto& kv : refMap) {
        auto it = dmMap.find(kv.first);
        if (it == dmMap.end()) continue;
        compareStyle(*kv.second, *it->second, result);
        compareLayout(*kv.second, *it->second, result);
    }

    int totalNodes = (int)ref.nodes.size();
    if (totalNodes > 0) {
        result.styleScore  = 1.0 - (double)result.styleDiffCount / totalNodes;
        result.layoutScore = 1.0 - (double)result.layoutDiffCount / totalNodes;
        if (result.styleScore < 0) result.styleScore = 0;
        if (result.layoutScore < 0) result.layoutScore = 0;
    }

    return result;
}

} // namespace dm::learn