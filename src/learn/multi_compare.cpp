#include "learn/multi_compare.h"
#include <cmath>
#include <map>

namespace dm::learn {

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

    if (!ref.style.color.empty() && ref.style.color != dm.style.color)
        addDiff("color", ref.style.color, dm.style.color, 3);
    if (!ref.style.backgroundColor.empty() &&
        ref.style.backgroundColor != dm.style.backgroundColor &&
        ref.style.backgroundColor != "0,0,0,0")
        addDiff("backgroundColor", ref.style.backgroundColor,
                dm.style.backgroundColor, 3);
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