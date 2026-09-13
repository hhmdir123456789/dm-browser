# 大明 DM 浏览器

> 新一代本地浏览器。自研渲染引擎 + 学习库 + AI 助手。

一个从零构建的 Windows 桌面浏览器项目，目标是**用自研渲染引擎替代 WebView2**，并通过跨引擎对比，持续发现并补齐渲染能力差距。

---

## 目录

- [核心理念](#核心理念)
- [项目结构](#项目结构)
- [核心组件](#核心组件)
- [快速开始](#快速开始)
- [自研渲染引擎](#自研渲染引擎)
- [学习库与跨引擎对比](#学习库与跨引擎对比)
- [开发进度](#开发进度)
- [构建](#构建)
- [开发工具](#开发工具)
- [架构图](#架构图)
- [未来方向](#未来方向)

---

## 核心理念
WebView2 渲染（Chromium） ─┐
├─→ 多维对比 → 差异列表 → 学习库 → 待补能力清单
DM 自研引擎渲染 ─┘

text

**不是**"做个浏览器"——那太大。

**而是**：让学习库有真实的对比对象，让自研引擎有明确的推进方向。

每一轮迭代：
1. 用同一份 HTML 分别在 WebView2 和自研引擎中渲染
2. 采集两边的 DOM 快照（节点、样式、布局坐标）
3. 多维对比，找出差异
4. 差异按严重度排序 = **下一步要攻的能力清单**

---

## 项目结构
dm_browser/
├── src/
│ ├── main.cpp # dm_browser 入口（进程隔离、沙箱）
│ ├── db/ # SQLite 封装
│ ├── ipc/ # 进程间通信（Pipe / Framing）
│ ├── plugin_host/ # 插件宿主
│ ├── plugin_sdk/ # 插件 SDK 示例
│ ├── ui/ # UI 层（WebView2 容器、侧边栏）
│ │ ├── BrowserWindow.cpp # 主窗口 + 侧边栏 + AI 对话
│ │ ├── TabManager.cpp # 多标签管理
│ │ ├── AiClient.cpp # Ollama 客户端
│ │ ├── ui.html # 浏览器 UI（标签栏、地址栏）
│ │ ├── sidebar.html # 侧边栏（AI 助手 / 分析 / 隐私 / 历史）
│ │ └── start_page.html # 新标签页
│ ├── learn/ # 学习库（快照 + 对比 + 特性覆盖率）
│ │ ├── learn_store.h/.cpp # SQLite 数据层
│ │ ├── snapshot.h # PageSnapshot / NodeSnapshot / DimDiff
│ │ ├── snapshot_parser.h/.cpp # JSON 序列化 / 解析
│ │ ├── multi_compare.h/.cpp # 多维对比算法
│ │ └── learn_main.cpp # dm_learn CLI
│ ├── render/ # ★ 自研渲染引擎
│ │ ├── render_node.h # DOM 节点 + ComputedStyle + Layout
│ │ ├── html_parser.h/.cpp # HTML 解析
│ │ ├── css_parser.h/.cpp # CSS 解析（后代选择器）
│ │ ├── style_resolver.h/.cpp # 样式计算（继承、优先级）
│ │ ├── layout_engine.h/.cpp # 布局（block / inline / flex / absolute / 换行）
│ │ ├── text_measure.h/.cpp # 文字度量（估算 / GDI 可切换）
│ │ ├── snapshot_dumper.h/.cpp # 输出为 PageSnapshot JSON
│ │ ├── render_window.h/.cpp # GDI 渲染窗口
│ │ └── main.cpp # dm_render_test CLI
│ └── knowledge/ # 知识库（HTML/CSS/JS 特性数据）
├── tests/
│ ├── pages/ # 测试页面
│ │ ├── test_render.html # 基础测试页（block + inline）
│ │ └── china.html # 进阶测试页（flex + absolute + 换行）
│ └── test_*.cpp # 单元测试
├── scripts/
│ └── compare_engines.ps1 # 跨引擎对比流程脚本
├── knowledge/ # 知识库 JSON
├── third_party/ # sqlite3 / nlohmann-json
└── CMakeLists.txt

text

---

## 核心组件

### `dm_browser` — 核心进程

基于 WebView2 + 自研 IPC 的多进程浏览器骨架。支持沙箱隔离、站点隔离、插件加载。

### `dm_ui` — 浏览器界面

带侧边栏的桌面浏览器：
- 多标签（拖拽、固定、休眠）
- 命令面板（`Ctrl+K`）
- 侧边栏 4 个 tab：**AI 助手 / 分析 / 隐私 / 历史**
- AI 对话（Ollama 本地模型）
- **页面分析**：一键采集当前页 DOM 快照 → 多维对比 → 差异高亮

### `dm_learn` — 学习库 CLI

命令行工具，管理"参考快照"和"特性覆盖率"。

### `dm_render_test` — 自研渲染引擎 CLI

命令行渲染引擎，支持三种模式：
- 默认：DOM 树 + 布局坐标
- `--dump`：输出快照 JSON
- `--window`：GDI 窗口可视化

### `dm_kb` — 知识库工具

管理 61 条 HTML/CSS/JS 特性数据。

---

## 快速开始

### 依赖

- **Windows 10/11**
- **Visual Studio 2022**（C++20）
- **CMake 3.20+**
- **Python 3.8+**（仅用于 `http.server` 测试）

### 克隆 + 构建

```powershell
git clone https://github.com/hhmdir123456789/dm-browser.git
cd dm-browser

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
CMake 会自动下载 WebView2 SDK（首次编译约 1 分钟）。

跑浏览器
powershell
cd build\Release
.\dm_ui.exe
侧边栏 → 「分析」→ 打开任意网页 → 点「分析本页」。

自研渲染引擎
已支持
类别	特性
HTML	标签、id/class、文本、实体解码、注释、script/style 跳过、属性内 >
CSS 选择器	tag / .class / #id / 组合 / 后代选择器（.card p）
CSS 属性	color / background-color / font-size / font-weight / display / position / text-align / margin / padding / border / flex-direction / left / top
CSS 优先级	!important / specificity / 继承 / 默认值对齐 Chromium
布局	block 垂直堆叠 / inline 横排 / flex 横排 / position:absolute / 文字自动换行 / margin 塌陷
文字度量	内置估算（ASCII 0.5em / CJK 1.0em）/ GDI 可选
输出	DOM 树打印 / 快照 JSON / GDI 窗口可视化
使用方式
powershell
# 1. 打印 DOM 树 + 布局
.\dm_render_test.exe test.html 1024 768

# 2. 输出快照 JSON
.\dm_render_test.exe test.html 1024 768 --dump

# 3. 打开渲染窗口（部分场景有 bug）
.\dm_render_test.exe test.html 800 600 --window

# 4. 使用 GDI 度量（默认关闭）
.\dm_render_test.exe test.html 800 600 --gdi --dump
当前精度
在测试页 test_render.html 上，与 WebView2 对比达到：

text
结构相似度: 100%
样式相似度: 100%
布局相似度: 100%
差异总数: 0
真实网页（如 china.com）会暴露更多缺失：flex 细节 / float / 文字换行精确度 / 图片尺寸等——这是下一步的清单。

学习库与跨引擎对比
学习库命令
powershell
# 查看参考快照列表
.\dm_learn.exe refs

# 查看某个 URL 的参考详情
.\dm_learn.exe inspect https://www.china.com/

# 导出参考快照为 JSON
.\dm_learn.exe export https://www.china.com/ china.json

# 对比两份快照
.\dm_learn.exe compare webview2.json render.json

# 生成 Markdown 报告
.\dm_learn.exe report webview2.json render.json report.md

# 特性覆盖率排行（按优先级）
.\dm_learn.exe features
跨引擎对比流程
powershell
# 前置：先用 dm_ui 打开测试页，点「分析本页」存为参考

# 1. 导出 WebView2 参考
.\dm_learn.exe export http://localhost:8080/test.html webview2.json

# 2. 自研引擎生成快照
.\dm_render_test.exe test.html 1156 753 --dump

# 3. 对比
.\dm_learn.exe compare webview2.json test.html.snapshot.json

# 4. 生成报告
.\dm_learn.exe report webview2.json test.html.snapshot.json report.md
一键脚本
powershell
.\scripts\compare_engines.ps1 `
    -Url "http://localhost:8080/test_render.html" `
    -HtmlPath "tests\pages\test_render.html"
开发进度
批次	内容	状态
批 1-3	浏览器骨架 / IPC / 知识库	✅
批 3B	dm_learn 工具链（dump / compare / infer）	✅
批 4A	侧边栏集成学习库分析	✅
批 4B-1	统一数据库 dm_browser.db	✅
批 4B-2	参考快照入库，首次保存 / 二次对比	✅
批 4B-3	点差异条目高亮 DOM 节点	✅
批 C	CLI 增加 refs / inspect / export / report	✅
批 D	自研渲染引擎（HTML / CSS / Layout / 快照 / GDI）	✅
批 F	跨引擎对比实战（81 → 0 差异）	✅
批 G	渲染能力扩展（行高 / inline 定位 / 文字宽度 / 非可视节点过滤）	✅
批 H	HTML 实体 / !important / 属性内 >	✅
批 I	文字度量接口（估算 / GDI 可切换）	✅
批 J	flex / position:absolute / 文字自动换行	✅
批 K	渲染窗口白屏修复	⏳ 待办
构建
Release 构建
powershell
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
单独构建某目标
powershell
cmake --build build --config Release --target dm_render_test
cmake --build build --config Release --target dm_learn
cmake --build build --config Release --target dm_ui
清理重编
powershell
cmake --build build --config Release --clean-first --target dm_render_test
开发工具
测试页
文件	内容
tests/pages/test_render.html	基础版：block + inline + 颜色 + padding
tests/pages/china.html	进阶版：flex 导航 + absolute badge + 换行
启动本地 HTTP 服务（用于真实 URL 采集）
powershell
cd tests\pages
python -m http.server 8080
然后 dm_ui 打开 http://localhost:8080/test_render.html。

已知问题
问题	影响	优先级
dm_render_test --window 白屏	可视化不可用	中
position:absolute baseX 用 paddingLeft 而非 borderWidth	偏差 ~10px	低
结构相似度 81%（html/head/title 引擎定义差异）	报告不完美	低
文字宽度用估算而非 DirectWrite	精度 ±5%	中
架构图
text
┌─────────────────────────────────────────────────────────────┐
│  dm_ui.exe                                                  │
│  ┌──────────────────────┐  ┌─────────────────────────────┐  │
│  │  内容 WebView2        │  │  侧边栏 WebView2            │  │
│  │  (Chromium 渲染)      │  │  (AI 助手 / 分析 / 隐私)     │  │
│  └──────────┬───────────┘  └──────────┬──────────────────┘  │
│             │ WebMessage              │ WebMessage           │
│             ▼                          ▼                     │
│  ┌──────────────────────────────────────────────────────┐   │
│  │  BrowserWindow (C++)                                  │   │
│  │  · 采集快照：ExecuteScript(collectSnapshot.js)        │   │
│  │  · 解析：parseSnapshotJson()                          │   │
│  │  · 对比：MultiCompare::compare()                      │   │
│  │  · 写库：LearnStore::recordFeature()                  │   │
│  └──────────────────────┬───────────────────────────────┘   │
└─────────────────────────┼───────────────────────────────────┘
                          │ SQLite
                          ▼
             ┌─────────────────────────┐
             │  dm_browser.db           │
             │  · site_sample           │
             │  · diff_record           │
             │  · feature_coverage      │
             └──────────┬──────────────┘
                        │
        ┌───────────────┼───────────────────┐
        ▼               ▼                   ▼
   ┌─────────┐    ┌──────────┐      ┌──────────────┐
   │dm_learn │    │dm_kb     │      │dm_render_test│
   │CLI 工具 │    │知识库 CLI│      │自研引擎 CLI  │
   └─────────┘    └──────────┘      └──────┬───────┘
                                            │
                                            ▼
                              ┌──────────────────────────┐
                              │  HTML → CSS → Layout     │
                              │  → Snapshot → Compare    │
                              └──────────────────────────┘
未来方向
短期（批 K ~ M）
修窗口白屏 —— 让 GDI 可视化可用

修 absolute baseX —— 对齐 CSS 规范

接 DirectWrite —— 替代 GDI 度量，跟 Chromium 完全对齐

中期（批 N ~ R）
flex 细节：justify-content / align-items / flex-grow

float / clear

position: fixed / sticky

图片尺寸：从 <img width height> 或 URL 解析

长期
JS 引擎（js_lite） —— 最小可用的 JS 执行

真实网页自动对比 —— 爬取 + 对比 + 生成"能力差距报告"

学习库自动化 —— AI 分析差异 + 建议补丁

最终目标
一个可替代 Chromium 的自研渲染引擎，用学习库持续驱动演进。

致谢
WebView2 — 参考渲染 + 初期容器

SQLite — 学习库存储

nlohmann/json — JSON 处理

Ollama — 本地 AI 模型

License
MIT