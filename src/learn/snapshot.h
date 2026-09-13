#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace dm::learn {

// 单个节点的多维度快照
struct NodeSnapshot {
    std::string path;          // 稳定路径：html>body>div.1>p.0
    std::string tag;
    std::string id;
    std::string className;
    int depth = 0;
    int childCount = 0;

    // 样式维度
    struct Style {
        std::string color;
        std::string backgroundColor;
        std::string fontSize;
        std::string fontWeight;
        std::string display;
        std::string position;
        std::string textAlign;
        int marginTop = 0, marginBottom = 0;
        int marginLeft = 0, marginRight = 0;
        int paddingTop = 0, paddingBottom = 0;
        int paddingLeft = 0, paddingRight = 0;
        int borderWidth = 0;
        std::string borderStyle;
    } style;

    // 布局维度
    struct Layout {
        float x = 0, y = 0, w = 0, h = 0;
    } layout;

    std::string textPreview;

    // 语义维度
    std::string alt;
    std::string ariaLabel;
    std::string role;
};

// 整页快照
struct PageSnapshot {
    std::string url;
    std::string title;
    int viewportWidth = 0;
    int viewportHeight = 0;
    int totalNodes = 0;
    int maxDepth = 0;

    std::vector<NodeSnapshot> nodes;

    int imageCount = 0;
    int scriptCount = 0;
    int styleSheetCount = 0;
};

// 单个差异
struct DimDiff {
    std::string dimension;     // structural / style / layout
    std::string category;      // missing_node / extra_node / color_diff / ...
    std::string path;
    std::string property;      // 具体属性（如 "fontSize"）
    std::string refValue;
    std::string dmValue;
    int severity = 1;
    std::string description;
};

// 多维对比结果
struct MultiDimResult {
    std::string url;
    double structuralScore = 1.0;
    double styleScore      = 1.0;
    double layoutScore     = 1.0;

    std::vector<DimDiff> diffs;
    int structuralDiffCount = 0;
    int styleDiffCount      = 0;
    int layoutDiffCount     = 0;
};

} // namespace dm::learn