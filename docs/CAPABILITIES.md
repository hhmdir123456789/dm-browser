# 能力矩阵

最后更新：2026-09-15
当前版本：v1.0（迭代 1–5 完成）

## 已实现

### 解析
- HTML：标签、id、class、所有属性、文本、HTML 实体、注释、`<!DOCTYPE>`
- CSS：tag / .class / #id / [attr] / [attr=value]
- 选择器组合：后代、子 `>`、相邻 `+`、兄弟 `~`
- 伪类：`:not(简单选择器)`、`:first-child`、`:last-child`、`:only-child`、`:first-of-type`、`:last-of-type`、`:only-of-type`
- `@media` 条件判断：`min-width` / `max-width` / `min-height` / `max-height` / `print`
- `@supports` 展开
- `@font-face`：本地 TTF 加载（`AddFontResourceExW` + `FR_PRIVATE`）
- CSS 变量 `var(--x, fallback)`

### 样式
- 颜色：`#RGB` / `#RRGGBB` / `rgb()` / `rgba()` / `hsl()` / `hsla()`
- 长度：`px` / `%` / `em` / `rem` / `vw` / `vh` / `calc()`
- 继承：`color` / `font-size` / `font-weight` / `font-family` / `text-align` / `line-height`
- `box-sizing`：`content-box` / `border-box`
- `display`：`block` / `inline` / `inline-block` / `none` / `flex` / `inline-flex` / `grid` / `list-item`
- `position`：`static` / `relative`（偏移） / `absolute` / `fixed`
- `float`：`left` / `right`
- `opacity`：0–1
- `font-style: italic`
- `list-style`：`disc` / `circle` / `square` / `decimal` / `none`
- `line-height`：`normal` / 无单位数字 / 带单位长度
- 字体 fallback 链：`font-family: "X", "Y", sans-serif`
- `background` / `background-image`：纯色 + `linear-gradient` / `radial-gradient`
- `filter`：`grayscale` / `brightness` / `contrast` / `invert` / `blur`
- `transition`：仅解析并存入快照，不做真动画

### 布局
- block + margin 折叠
- inline / inline-block
- flex：`flex-wrap` / `justify-content` / `align-items`
- grid：`grid-template-columns`（`repeat` / `fr` / `px`） / `gap`
- float 环绕
- `list-item` 标记占位
- 盒模型宽度计算（`margin` / `padding` / `border` 全部扣减）
- `@media` 按 viewport 过滤

### 渲染（GDI）
- 背景色 / 背景图（渐变）
- 边框 / 圆角
- 文本（字体 fallback / 斜体 / 字重）
- 图片（WIC 加载 + AlphaBlend）
- 列表标记（disc / circle / square / decimal）
- `transform`：`rotate` / `scale` / `scaleX` / `scaleY` / `translate`
- `overflow`：`hidden` / `clip`
- `z-index` 排序
- `opacity` 合成
- `filter` 逐像素
- 表单控件外观：`input` / `button` / `select`

### 快照 / 对比
- 快照 JSON：节点路径、tag、id、class、深度、文本预览
- 样式：color、backgroundColor、fontSize、fontWeight、display、position、textAlign、margin、padding、border、opacity、fontStyle、listStyleType、backgroundImage、intrinsicW/H、filter、transition
- 布局：x / y / w / h
- `MultiCompare`：结构 / 样式 / 布局相似度 + 差异明细

## Out of Scope（不实现）

- JS 执行
- SVG / canvas / iframe
- 音视频
- `lab` / `lch` / `color()` 颜色空间
- 双向文本 / 复杂字形 / CJK fallback 链
- `backdrop-filter`
- 完整表格模型（`colspan` / `rowspan` / `colgroup` 自动列宽）
- `transition` / `animation` 真动画
- `@font-face` 网络加载（仅本地文件）
- CSS Grid 高级：`grid-row` / `grid-template-rows` / `grid-auto-*`
- flex 高级：`flex-grow` / `flex-shrink` / `flex-basis` / `order`
- 伪元素 `:before` / `:after`
- `:hover` / `:focus` / `:active` 等交互态伪类
