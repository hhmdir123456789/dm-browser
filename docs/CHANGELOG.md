# 变更日志

## v1.0 — 2026-09-15

### 迭代 1
- 伪类：`:not()` / `:first-child` / `:last-child` / `:only-child` 等
- `position: relative` 偏移
- `opacity`
- `font-style: italic`

### 迭代 2
- `list-style`：`disc` / `circle` / `square` / `decimal`
- `linear-gradient` / `radial-gradient`
- 字体 fallback 链
- `line-height` 显式处理（`normal` / 数字 / 长度）

### 迭代 3
- 图片加载（WIC 解码 + AlphaBlend）
- `<img>` 的 HTML `width` / `height` 属性
- `@font-face`（本地 TTF，`AddFontResourceExW`）
- `table` 基础布局
- `snapshot.h` 加 `intrinsicW/H`

### 迭代 4a
- `@media` 条件判断：`min-width` / `max-width` / `min-height` / `max-height` / `print`
- 嵌套 `@media` 条件合并
- `resolveStyles` 带 viewport 参数

### 迭代 4b
- `filter`：`grayscale` / `brightness` / `contrast` / `invert` / `blur`
- 新增 `src/render/filter.h` / `filter.cpp`
- `paintNodeWithFilter` 离屏合成

### 迭代 4c
- 表单控件默认外观：`input` / `button` / `select`
- 默认尺寸 / 边框 / 背景
- 表单文字绘制

### 迭代 5a
- `transition` 静态解析：只存串，不做真动画

### 迭代 5b
- 文档：`CAPABILITIES.md` / `STATUS.md` / `CHANGELOG.md`

### 基线修复（迭代 0）
- 盒模型宽度计算：`contentW` 扣减 `padding` / `border`
- `layout.w` 保持 border-box
- `color` / `font-size` / `display` 默认值
- `display` 各值计算
