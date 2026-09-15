#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace dm::learn {

struct NodeSnapshot {
    std::string path;
    std::string tag;
    std::string id;
    std::string className;
    int depth = 0;
    int childCount = 0;

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

        // iter1
        std::string opacity;
        std::string fontStyle;

        // iter2
        std::string listStyleType;
        std::string backgroundImage;

        // iter3
        int intrinsicW = 0;
        int intrinsicH = 0;
                // iter4
        std::string filter;
    } style;

    struct Layout {
        float x = 0, y = 0, w = 0, h = 0;
    } layout;

    std::string textPreview;
    std::string alt;
    std::string ariaLabel;
    std::string role;
};

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

struct DimDiff {
    std::string dimension;
    std::string category;
    std::string path;
    std::string property;
    std::string refValue;
    std::string dmValue;
    int severity = 1;
    std::string description;
};

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