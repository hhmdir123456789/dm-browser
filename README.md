# 大明DM浏览器

[![DM Browser CI](https://github.com/hhmdir123456789/dm-browser/actions/workflows/ci.yml/badge.svg)](https://github.com/hhmdir123456789/dm-browser/actions/workflows/ci.yml)

一个从零构建的浏览器内核原型，包含多进程架构、插件系统、能力式授权、SQLite 持久化、站点隔离和企业级治理。

## 特性

- **多进程内核** — 浏览器主进程与插件宿主进程通过命名管道通信
- **能力式授权** — 每个插件按需申请端点，内核只授予声明过的能力
- **站点隔离** — origin 规范化（去 www、去端口、取 eTLD+1），敏感站点强制独立进程
- **沙箱** — Windows Job Object 内存限制
- **插件市场** — 官方签名 / 社区签名 / 未签名三层，按层级过滤可申请端点
- **企业策略** — 白名单、端点配额、审计保留、离线镜像
- **生态治理** — 治理成员、决策投票、争议处理、生态指标、年度报告
- **SQLite 持久化** — 授权、审计、书签、下载、凭据、市场、治理全部落盘
- **82 条自动化测试** — 主进程自测 67 条 + 独立测试套件 15 条

## 构建

### 依赖

- CMake 3.20+
- C++20 编译器（MSVC 19.30+ / GCC 11+ / Clang 14+）
- Windows SDK（Windows 平台）

sqlite3 的 amalgamation 已包含在 `third_party/sqlite3/`，无需额外安装。

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

# 主进程自测（67 条）
.\dm_browser.exe

# 独立测试套件（15 条）
.\dm_tests.exe

# UI 窗口
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
│   ├── ui/                     Win32 窗口
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
├── 通过 Invoke 信封调用端点
└── 由 PluginHost 按 manifest 启动
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
82 条测试覆盖：

模块	测试数
里程碑 1：标签页	6
里程碑 2：书签 + 下载	12
里程碑 3：密码 + 注入	13
里程碑 4：市场 + 签名	18
里程碑 5：企业策略 + LTS	16
里程碑 6：治理 + 争议	23
跨进程 IPC	6
站点隔离 + 沙箱	7
独立测试套件	15
CI
每次 push 到 main 时自动在 Windows 上：

配置 MSVC 环境

验证 third_party/sqlite3/ 存在

CMake 配置 + 构建

运行 dm_browser.exe（67 条）和 dm_tests.exe（15 条）

验证 5 个产物存在

上传 artifacts

构建时长约 2 分钟。

已知限制
POSIX 管道未实现 — Pipe.cpp 的 POSIX 分支返回错误，Windows 上已验证

沙箱仅 Job Object — 未升级到 AppContainer 完整版

UI 是 Win32 原生窗口 — 未接真实渲染引擎

插件静态编译 — dm_plugin_host.exe 里的插件逻辑是编译进去的，未做运行时 LoadLibrary

许可证
BSD 2-Clause