#include "render/length.h"
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <vector>

namespace dm::render {

namespace {
bool isSpace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && isSpace(s[a])) a++;
    while (b > a && isSpace(s[b-1])) b--;
    return s.substr(a, b - a);
}
}

Length parseLength(const std::string& s) {
    Length l;
    std::string v = trim(s);
    if (v.empty()) return l;
    if (v.rfind("calc(", 0) == 0) {
        l.value = evalCalc(v, 16, 16, 1024, 768, 0);
        l.unit = Length::Px;
        return l;
    }

    size_t i = 0;
    if (i < v.size() && (v[i] == '-' || v[i] == '+')) i++;
    while (i < v.size() && (std::isdigit((unsigned char)v[i]) || v[i] == '.')) i++;

    std::string num = v.substr(0, i);
    std::string unit = trim(v.substr(i));

    l.value = (float)std::atof(num.c_str());

    if (unit == "%") l.unit = Length::Percent;
    else if (unit == "em") l.unit = Length::Em;
    else if (unit == "rem") l.unit = Length::Rem;
    else if (unit == "vw") l.unit = Length::Vw;
    else if (unit == "vh") l.unit = Length::Vh;
    else l.unit = Length::Px;
    return l;
}

float resolveLength(const Length& len, int fontSizePx, int rootFontSize,
                    int viewportW, int viewportH, float percentBase) {
    if (len.isZero()) return 0;
    switch (len.unit) {
        case Length::Px:      return len.value;
        case Length::Percent: return percentBase * len.value / 100.0f;
        case Length::Em:      return len.value * fontSizePx;
        case Length::Rem:     return len.value * rootFontSize;
        case Length::Vw:      return len.value * viewportW / 100.0f;
        case Length::Vh:      return len.value * viewportH / 100.0f;
    }
    return len.value;
}

float evalCalc(const std::string& expr,
               int fontSizePx, int rootFontSize,
               int viewportW, int viewportH, float percentBase) {
    std::string e = trim(expr);
    if (e.rfind("calc(", 0) == 0 && !e.empty() && e.back() == ')') {
        e = e.substr(5, e.size() - 6);
    }

    std::vector<std::string> tokens;
    std::string cur;
    for (size_t i = 0; i < e.size(); ++i) {
        char c = e[i];
        if (c == '(') {
            int depth = 1;
            cur += c;
            size_t j = i + 1;
            while (j < e.size() && depth > 0) {
                if (e[j] == '(') depth++;
                else if (e[j] == ')') depth--;
                cur += e[j];
                j++;
            }
            i = j - 1;
        } else if (c == '+' || c == '-' || c == '*' || c == '/') {
            if (!cur.empty()) { tokens.push_back(trim(cur)); cur.clear(); }
            tokens.push_back(std::string(1, c));
        } else if (isSpace(c)) {
            if (!cur.empty()) { tokens.push_back(trim(cur)); cur.clear(); }
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) tokens.push_back(trim(cur));

    std::vector<float> nums;
    std::vector<char> ops;

    auto toNum = [&](const std::string& s) -> float {
        std::string t = trim(s);
        if (t.rfind("calc(", 0) == 0) {
            return evalCalc(t, fontSizePx, rootFontSize, viewportW, viewportH, percentBase);
        }
        Length l = parseLength(t);
        return resolveLength(l, fontSizePx, rootFontSize, viewportW, viewportH, percentBase);
    };

    for (size_t i = 0; i < tokens.size(); ++i) {
        const std::string& t = tokens[i];
        if (t == "*" || t == "/") {
            if (nums.empty() || i + 1 >= tokens.size()) return 0;
            float rhs = toNum(tokens[i + 1]);
            if (t == "*") nums.back() *= rhs;
            else if (rhs != 0) nums.back() /= rhs;
            i++;
        } else if (t == "+" || t == "-") {
            ops.push_back(t[0]);
        } else {
            nums.push_back(toNum(t));
        }
    }

    if (nums.empty()) return 0;
    float result = nums[0];
    for (size_t i = 0; i < ops.size() && i + 1 < nums.size(); ++i) {
        if (ops[i] == '+') result += nums[i + 1];
        else result -= nums[i + 1];
    }
    return result;
}

} // namespace dm::render