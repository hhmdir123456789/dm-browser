# 大明DM浏览器

[![DM Browser CI](https://github.com/hhmdir123456789/dm-browser/actions/workflows/ci.yml/badge.svg)](https://github.com/hhmdir123456789/dm-browser/actions/workflows/ci.yml)

一个从零构建的浏览器内核原型，包含多进程架构、插件系统、能力式授权、SQLite 持久化、站点隔离、插件热加载和基于 WebView2 的真实渲染 UI。

## 特性

- **多进程内核** — 浏览器主进程与插件宿主进程通过命名管道通信
- **能力式授权** — 每个插件按需申请端点，内核只授予声明过的能力
- **站点隔离** — origin 规范化（去 www、去端口、取 eTLD+1），敏感站点强制独立进程
- **沙箱** — Windows Job Object 内存限制
- **插件市场** — 官方签名 / 社区签名 / 未签名三层，按层级过滤可申请端点
- **企业策略** — 白名单、端点配额、审计保留、离线镜像
- **生态治理** — 治理成员、决策投票、争议处理、生态指标、年度报告
- **SQLite 持久化** — 授权、审计、书签、下载、凭据、市场、治理全部落盘
- **插件热加载** — `LoadLibrary` + `reload` 命令，运行时卸载重载 `.dll`
- **HTML/CSS 双层 WebView2 UI** — 标签栏、工具栏、地址栏用 HTML/CSS 渲染，复刻 Edge 风格
- **专用起始页** — 本地 HTML，DeepSeek 优先，含问候语、时钟、搜索路由
- **94 条自动化测试** — 主进程自测 79 条 + 独立测试套件 15 条

## 构建

### 依赖

- CMake 3.20+
- C++20 编译器（MSVC 19.30+ / GCC 11+ / Clang 14+）
- Windows SDK（Windows 平台）

sqlite3 的 amalgamation 已包含在 `third_party/sqlite3/`。WebView2 SDK 由 CMake 在配置阶段自动从 NuGet 下载（约 30 MB），不需要手动安装。

### Windows

```powershell
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
Linux / macOS
bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
运行
powershell
cd build\Release

# 主进程自测（79 条）
.\dm_browser.exe

# 独立测试套件（15 条）
.\dm_tests.exe

# UI 窗口（HTML/CSS 双层 WebView2）
.\dm_ui.exe
目录结构
text
dm_browser/
├── .github/workflows/ci.yml    CI 配置
├── sql/schema.sql              数据库 schema
├── src/
│   ├── common/                 基础类型、错误、审计
│   ├── db/                     SQLite 封装
│   ├── ipc/                    命名管道 + 消息分帧
│   ├── plugin_host/            插件宿主进程入口
│   ├── plugin_sdk/             插件 API + 示例插件
│   ├── ui/
│   │   ├── BrowserWindow.*     主窗口，双层 WebView2
│   │   ├── TabManager.*        标签数据模型
│   │   ├── RenderWindow.*      单标签 fallback
│   │   ├── ui.html             UI 层（HTML/CSS，复刻 Edge）
│   │   ├── start_page.html     起始页
│   │   └── ui_main.cpp         入口
│   ├── ProcessManager          真实进程管理
│   ├── SiteIsolator            origin 规范化 + 站点隔离
│   ├── Sandbox                 Job Object 沙箱
│   ├── IPCBroker               跨进程 IPC 仲裁
│   ├── GrantStore              授权持久化
│   ├── PluginHost              插件生命周期
│   ├── Market                  插件市场
│   ├── EnterprisePolicy        企业策略
│   ├── LTSChannel              LTS 通道
│   ├── Governance              治理结构
│   ├── Dispute                 争议处理
│   ├── EcosystemMetrics        生态指标
│   └── main.cpp                主入口 + 自测
├── tests/                      独立测试套件
└── third_party/sqlite3/        sqlite3 amalgamation
架构
进程模型
text
Browser 进程
├── GrantStore / AuditLog / Market / Governance（主进程持有）
├── IPCBroker（主进程侧，仲裁 + 审计）
│   ├── 命名管道服务端 \\.\pipe\dm_ipc_<pid>
│   └── 每个插件进程一条管道
└── ProcessManager（主进程侧，创建/管理插件子进程）

插件进程（dm_plugin_host.exe）
├── 连接主进程管道
├── LoadLibrary 加载插件 DLL
├── 通过 Invoke 信封调用端点
└── 支持 reload 命令卸载重载
UI 分层
text
主窗口 HWND
├── WebView2 A（UI 层）—— 高 88px
│   └── 渲染 ui.html：标签栏 + 工具栏 + 地址栏
│   └── 通过 postMessage 与 C++ 双向通信
└── WebView2 B（内容层）—— 剩余高度
    └── 渲染真实网页
    └── 导航事件通知 C++，C++ 再转发给 A 更新地址栏
授权模型
每个插件在安装时声明需要的端点，内核按市场层级过滤后签发 CapabilityGrant：

层级	可申请端点
官方签名	Core 之外的全部端点
社区签名	Granted 层（敏感端点除外）
未签名	仅 Ambient 层
消息协议
text
4 字节长度前缀 + 负载
负载：1 字节类型 + 字符串字段（长度 + 内容）

类型：
  1 = Hello    插件进程启动后第一条
  2 = Invoke   主进程 → 插件
  3 = Result   插件 → 主进程
  4 = Shutdown 关闭
测试
94 条测试覆盖：

模块	测试数
里程碑 1：标签页	6
里程碑 2：书签 + 下载	12
里程碑 3：密码 + 注入	13
里程碑 4：市场 + 签名	18
里程碑 5：企业策略 + LTS	16
里程碑 6：治理 + 争议	23
跨进程 IPC + 插件热加载	12
站点隔离 + 沙箱	7
独立测试套件	15
CI
每次 push 到 main 时自动在 Windows 上：

配置 MSVC 环境

验证 third_party/sqlite3/ 存在

CMake 配置（自动下载 WebView2 SDK）+ 构建

运行 dm_browser.exe（79 条）和 dm_tests.exe（15 条）

验证 5 个产物存在

上传 artifacts

构建时长约 2 分 13 秒。

已知限制
POSIX 管道未实现 — Pipe.cpp 的 POSIX 分支返回错误，Windows 上已验证

沙箱仅 Job Object — 未升级到 AppContainer 完整版

UI 未接入真实 favicon — 标签图标是首字母占位符

标题用 URL 代替 — 真实标题需监听 DocumentTitleChanged 事件

地址栏 JSON 解析是手写的 — 只支持简单结构，复杂场景建议引入 nlohmann/json

许可证
BSD 2-Clause

text

---

## 用 PowerShell 覆盖

```powershell
cd C:\Users\86545\PycharmProjects\pythonProject3\venv\DM_LL\dm_browser

@'
# 大明DM浏览器

[![DM Browser CI](https://github.com/hhmdir123456789/dm-browser/actions/workflows/ci.yml/badge.svg)](https://github.com/hhmdir123456789/dm-browser/actions/workflows/ci.yml)

一个从零构建的浏览器内核原型，包含多进程架构、插件系统、能力式授权、SQLite 持久化、站点隔离、插件热加载和基于 WebView2 的真实渲染 UI。

## 特性

- **多进程内核** — 浏览器主进程与插件宿主进程通过命名管道通信
- **能力式授权** — 每个插件按需申请端点，内核只授予声明过的能力
- **站点隔离** — origin 规范化（去 www、去端口、取 eTLD+1），敏感站点强制独立进程
- **沙箱** — Windows Job Object 内存限制
- **插件市场** — 官方签名 / 社区签名 / 未签名三层，按层级过滤可申请端点
- **企业策略** — 白名单、端点配额、审计保留、离线镜像
- **生态治理** — 治理成员、决策投票、争议处理、生态指标、年度报告
- **SQLite 持久化** — 授权、审计、书签、下载、凭据、市场、治理全部落盘
- **插件热加载** — LoadLibrary + reload 命令，运行时卸载重载 DLL
- **HTML/CSS 双层 WebView2 UI** — 标签栏、工具栏、地址栏用 HTML/CSS 渲染，复刻 Edge 风格
- **专用起始页** — 本地 HTML，DeepSeek 优先，含问候语、时钟、搜索路由
- **94 条自动化测试** — 主进程自测 79 条 + 独立测试套件 15 条

## 构建

### 依赖

- CMake 3.20+
- C++20 编译器（MSVC 19.30+ / GCC 11+ / Clang 14+）
- Windows SDK（Windows 平台）

sqlite3 的 amalgamation 已包含在 third_party/sqlite3/。WebView2 SDK 由 CMake 在配置阶段自动从 NuGet 下载（约 30 MB），不需要手动安装。

### Windows

    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release

### Linux / macOS

    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release

## 运行

    cd build\Release

    # 主进程自测（79 条）
    .\dm_browser.exe

    # 独立测试套件（15 条）
    .\dm_tests.exe

    # UI 窗口（HTML/CSS 双层 WebView2）
    .\dm_ui.exe

## 目录结构

    dm_browser/
    ├── .github/workflows/ci.yml    CI 配置
    ├── sql/schema.sql              数据库 schema
    ├── src/
    │   ├── common/                 基础类型、错误、审计
    │   ├── db/                     SQLite 封装
    │   ├── ipc/                    命名管道 + 消息分帧
    │   ├── plugin_host/            插件宿主进程入口
    │   ├── plugin_sdk/             插件 API + 示例插件
    │   ├── ui/
    │   │   ├── BrowserWindow.*     主窗口，双层 WebView2
    │   │   ├── TabManager.*        标签数据模型
    │   │   ├── RenderWindow.*      单标签 fallback
    │   │   ├── ui.html             UI 层（HTML/CSS，复刻 Edge）
    │   │   ├── start_page.html     起始页
    │   │   └── ui_main.cpp         入口
    │   ├── ProcessManager          真实进程管理
    │   ├── SiteIsolator            origin 规范化 + 站点隔离
    │   ├── Sandbox                 Job Object 沙箱
    │   ├── IPCBroker               跨进程 IPC 仲裁
    │   ├── GrantStore              授权持久化
    │   ├── PluginHost              插件生命周期
    │   ├── Market                  插件市场
    │   ├── EnterprisePolicy        企业策略
    │   ├── LTSChannel              LTS 通道
    │   ├── Governance              治理结构
    │   ├── Dispute                 争议处理
    │   ├── EcosystemMetrics        生态指标
    │   └── main.cpp                主入口 + 自测
    ├── tests/                      独立测试套件
    └── third_party/sqlite3/        sqlite3 amalgamation

## 架构

### 进程模型

    Browser 进程
    ├── GrantStore / AuditLog / Market / Governance（主进程持有）
    ├── IPCBroker（主进程侧，仲裁 + 审计）
    │   ├── 命名管道服务端 \\.\pipe\dm_ipc_<pid>
    │   └── 每个插件进程一条管道
    └── ProcessManager（主进程侧，创建/管理插件子进程）

    插件进程（dm_plugin_host.exe）
    ├── 连接主进程管道
    ├── LoadLibrary 加载插件 DLL
    ├── 通过 Invoke 信封调用端点
    └── 支持 reload 命令卸载重载

### UI 分层

    主窗口 HWND
    ├── WebView2 A（UI 层）—— 高 88px
    │   └── 渲染 ui.html：标签栏 + 工具栏 + 地址栏
    │   └── 通过 postMessage 与 C++ 双向通信
    └── WebView2 B（内容层）—— 剩余高度
        └── 渲染真实网页
        └── 导航事件通知 C++，C++ 再转发给 A 更新地址栏

### 授权模型

每个插件在安装时声明需要的端点，内核按市场层级过滤后签发 CapabilityGrant：

- 官方签名：Core 之外的全部端点
- 社区签名：Granted 层（敏感端点除外）
- 未签名：仅 Ambient 层

### 消息协议

    4 字节长度前缀 + 负载
    负载：1 字节类型 + 字符串字段（长度 + 内容）

    类型：
      1 = Hello    插件进程启动后第一条
      2 = Invoke   主进程 → 插件
      3 = Result   插件 → 主进程
      4 = Shutdown 关闭

## 测试

94 条测试覆盖：

- 里程碑 1：标签页 — 6
- 里程碑 2：书签 + 下载 — 12
- 里程碑 3：密码 + 注入 — 13
- 里程碑 4：市场 + 签名 — 18
- 里程碑 5：企业策略 + LTS — 16
- 里程碑 6：治理 + 争议 — 23
- 跨进程 IPC + 插件热加载 — 12
- 站点隔离 + 沙箱 — 7
- 独立测试套件 — 15

## CI

每次 push 到 main 时自动在 Windows 上：

1. 配置 MSVC 环境
2. 验证 third_party/sqlite3/ 存在
3. CMake 配置（自动下载 WebView2 SDK）+ 构建
4. 运行 dm_browser.exe（79 条）和 dm_tests.exe（15 条）
5. 验证 5 个产物存在
6. 上传 artifacts

构建时长约 2 分 13 秒。

## 已知限制
    
- POSIX 管道未实现 — Pipe.cpp 的 POSIX 分支返回错误，Windows 上已验证
- 沙箱仅 Job Object — 未升级到 AppContainer 完整版
- UI 未接入真实 favicon — 标签图标是首字母占位符
- 标题用 URL 代替 — 真实标题需监听 DocumentTitleChanged 事件
- 地址栏 JSON 解析是手写的 — 只支持简单结构，复杂场景建议引入 nlohmann/json

## 许可证

BSD 2-Clause
'@ | Set-Content -Encoding UTF8 README.md