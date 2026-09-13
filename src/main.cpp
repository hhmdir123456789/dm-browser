#include <iostream>
#include <string>
#include <vector>
#include <cstdio>
#include <thread>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#endif

#include "common/Types.h"
#include "common/Error.h"
#include "common/Result.h"
#include "common/AuditLog.h"
#include "db/Database.h"
#include "Signature.h"
#include "GrantStore.h"
#include "PluginHost.h"
#include "IPCBroker.h"
#include "TabManager.h"
#include "Storage.h"
#include "SecureStore.h"
#include "ScriptInjection.h"
#include "Confirmation.h"
#include "Market.h"
#include "EnterprisePolicy.h"
#include "LTSChannel.h"
#include "Governance.h"
#include "Dispute.h"
#include "EcosystemMetrics.h"
#include "AnnualReport.h"
#include "ProcessManager.h"
#include "SiteIsolator.h"
#include "Sandbox.h"

static int g_pass = 0, g_fail = 0;

void check(bool cond, const std::string& name) {
    if (cond) { ++g_pass; std::cout << "[PASS] " << name << "\n"; }
    else      { ++g_fail; std::cout << "[FAIL] " << name << "\n"; }
}

class DMBrowser {
public:
    DMBrowser()
        : audit_(db_), grants_(db_), plugins_(db_),
          broker_(grants_, audit_, confirm_, pm_),
          bookmarks_(db_), downloads_(db_), secure_(db_),
          market_(db_, verifier_), policy_(db_), lts_(db_),
          council_(db_), disputes_(db_),
          metrics_(market_, disputes_, council_), report_(metrics_) {}

    Result<bool> boot(const std::string& dbPath) {
        int64_t t0 = nowMs();
        auto r = db_.open(dbPath);
        if (!r.isOk()) return r;
        if (!createSchema())
            return Result<bool>::fail(Error::db("schema creation failed"));
        registerHandlers();
        broker_.markSensitive("SecureStore");
        broker_.markSensitive("ScriptInjection");
        verifier_.addKey("official-key", "pub-official", MarketTier::Official);
        verifier_.addKey("community-key", "pub-community", MarketTier::Community);
        siteIsolator_.addSensitiveSite("https://bank.dm");
        siteIsolator_.addSensitiveSite("https://mail.dm");
        bootMs_ = nowMs() - t0;
        return Result<bool>::ok(true);
    }

    Database& db() { return db_; }
    SignatureVerifier& verifier() { return verifier_; }
    GrantStore& grants() { return grants_; }
    PluginHost& plugins() { return plugins_; }
    TabManager& tabs() { return tabs_; }
    BookmarkStore& bookmarks() { return bookmarks_; }
    DownloadStore& downloads() { return downloads_; }
    SecureStore& secure() { return secure_; }
    ScriptInjectionManager& injector() { return injector_; }
    ConfirmationManager& confirm() { return confirm_; }
    PluginMarket& market() { return market_; }
    EnterprisePolicy& policy() { return policy_; }
    LTSChannel& lts() { return lts_; }
    GovernanceCouncil& council() { return council_; }
    DisputeManager& disputes() { return disputes_; }
    EcosystemMetrics& metrics() { return metrics_; }
    AnnualReport& report() { return report_; }
    AuditLog& audit() { return audit_; }
    int64_t bootMs() const { return bootMs_; }

    dm::ProcessManager& processManager() { return pm_; }
    dm::SiteIsolator& siteIsolator() { return siteIsolator_; }
    dm::Sandbox& sandbox() { return sandbox_; }
    IPCBroker& broker() { return broker_; }

    Result<dm::ProcessInfo> spawnPluginHost(const std::string& pluginId,
                                            bool sensitive) {
        auto r = pm_.spawnPluginHost(pluginId, sensitive, "dm_plugin_host.exe");
        if (!r.isOk()) return r;
        auto policy = sensitive ? dm::Sandbox::sensitiveUtilityPolicy()
                                : dm::Sandbox::defaultUtilityPolicy();
        sandbox_.apply(r.value().pid, policy);
        return r;
    }

    InvokeResult call(const PluginId& pid, const std::string& ep,
                      const std::string& method, const std::string& args,
                      const std::string& origin = "") {
        InvokeEnvelope env;
        env.grantId = grants_.grant(pid, ep);
        env.endpoint = ep;
        env.method = method;
        env.traceId = "t-" + std::to_string(++traceSeq_);
        env.args = args;
        env.origin = origin;
        return broker_.invoke(pid, env);
    }

private:
    bool createSchema() {
        const char* stmts[] = {
            "CREATE TABLE IF NOT EXISTS process_table (pid INTEGER PRIMARY KEY, plugin_id TEXT, proc_type TEXT, state TEXT, sensitive INTEGER, cpu_budget INTEGER, mem_budget INTEGER, created_at INTEGER, last_active_at INTEGER)",
            "CREATE TABLE IF NOT EXISTS site_process_map (site TEXT PRIMARY KEY, pid INTEGER, is_sensitive INTEGER, bound_at INTEGER)",
            "CREATE TABLE IF NOT EXISTS capability_grant (grant_id TEXT PRIMARY KEY, plugin_id TEXT, endpoint TEXT, scope TEXT, version INTEGER, granted_at INTEGER, expires_at INTEGER, revoked_at INTEGER, requires_confirmation INTEGER, signature TEXT)",
            "CREATE TABLE IF NOT EXISTS audit_log (seq INTEGER PRIMARY KEY AUTOINCREMENT, trace_id TEXT, plugin_id TEXT, endpoint TEXT, method TEXT, priority TEXT, result TEXT, error_code TEXT, duration_ms INTEGER, retry_count INTEGER, created_at INTEGER)",
            "CREATE TABLE IF NOT EXISTS plugin_manifest (plugin_id TEXT PRIMARY KEY, name TEXT, version TEXT, tier TEXT, endpoints TEXT, privacy_labels TEXT, dependencies TEXT, signature TEXT, author_id TEXT, cached_at INTEGER)",
            "CREATE TABLE IF NOT EXISTS plugin_state (plugin_id TEXT PRIMARY KEY, state TEXT, enabled_at INTEGER, suspended_at INTEGER, unloaded_at INTEGER)",
            "CREATE TABLE IF NOT EXISTS plugin_storage (plugin_id TEXT, key TEXT, value BLOB, updated_at INTEGER, PRIMARY KEY (plugin_id, key))",
            "CREATE TABLE IF NOT EXISTS bookmarks (id INTEGER PRIMARY KEY AUTOINCREMENT, title TEXT, url TEXT, created_at INTEGER)",
            "CREATE TABLE IF NOT EXISTS downloads (id INTEGER PRIMARY KEY AUTOINCREMENT, url TEXT, filename TEXT, state TEXT, bytes_received INTEGER, total_bytes INTEGER, created_at INTEGER)",
            "CREATE TABLE IF NOT EXISTS credentials (origin TEXT, username TEXT, encrypted_password TEXT, created_at INTEGER, PRIMARY KEY (origin, username))",
            "CREATE TABLE IF NOT EXISTS market_plugin (plugin_id TEXT PRIMARY KEY, name TEXT, version TEXT, tier TEXT, key_id TEXT, signature TEXT, endpoints TEXT, privacy_labels TEXT, score REAL, downloads INTEGER, author_id TEXT, published_at INTEGER, withdrawn_at INTEGER)",
            "CREATE TABLE IF NOT EXISTS dispute_case (case_id TEXT PRIMARY KEY, plugin_id TEXT, reporter_id TEXT, reason TEXT, status TEXT, ruling TEXT, resolution TEXT, created_at INTEGER, resolved_at INTEGER, closed_at INTEGER)",
            "CREATE TABLE IF NOT EXISTS dispute_appeal (case_id TEXT, note TEXT, created_at INTEGER)",
            "CREATE TABLE IF NOT EXISTS governor (member_id TEXT PRIMARY KEY, display_name TEXT, role TEXT, joined_at INTEGER)",
            "CREATE TABLE IF NOT EXISTS decision (decision_id TEXT PRIMARY KEY, title TEXT, rationale TEXT, final_ruling TEXT, decided_at INTEGER)",
            "CREATE TABLE IF NOT EXISTS vote (decision_id TEXT, member_id TEXT, in_favor INTEGER, created_at INTEGER, PRIMARY KEY (decision_id, member_id))",
            "CREATE TABLE IF NOT EXISTS enterprise_policy (policy_id TEXT PRIMARY KEY, admin_signature TEXT, plugin_whitelist TEXT, endpoint_quota TEXT, audit_retention_days INTEGER, mirror_url TEXT, updated_at INTEGER)",
            "CREATE TABLE IF NOT EXISTS release_record (version TEXT PRIMARY KEY, channel TEXT, published_at INTEGER, eol_at INTEGER, notes TEXT)",
            "CREATE TABLE IF NOT EXISTS ecosystem_metric (id INTEGER PRIMARY KEY AUTOINCREMENT, period TEXT, plugin_total INTEGER, official_count INTEGER, community_count INTEGER, unsigned_count INTEGER, withdrawn_count INTEGER, avg_endpoints REAL, governor_count INTEGER, dispute_total INTEGER, health_score REAL, created_at INTEGER)"
        };
        for (const char* s : stmts) {
            auto r = db_.exec(s);
            if (!r.isOk()) return false;
        }
        return true;
    }

    void registerHandlers() {
        broker_.registerHandler("BookmarkStore", [this](const InvokeEnvelope& e) {
            InvokeResult r;
            if (e.method == "add") {
                auto pos = e.args.find('|');
                auto res = bookmarks_.add(e.args.substr(0, pos),
                                          e.args.substr(pos + 1));
                if (!res.isOk()) { r.ok = false; r.error = res.error(); return r; }
                r.ok = true; r.value = std::to_string(res.value());
            } else if (e.method == "list") {
                r.ok = true; r.value = "[...]";
            } else { r.ok = false; r.error = Error::internal("unknown"); }
            return r;
        });
        broker_.registerHandler("DownloadStore", [this](const InvokeEnvelope& e) {
            InvokeResult r;
            if (e.method == "create") {
                auto pos = e.args.find('|');
                auto res = downloads_.create(e.args.substr(0, pos),
                                             e.args.substr(pos + 1));
                if (!res.isOk()) { r.ok = false; r.error = res.error(); return r; }
                r.ok = true; r.value = std::to_string(res.value());
            } else { r.ok = false; r.error = Error::internal("unknown"); }
            return r;
        });
        broker_.registerHandler("SecureStore", [this](const InvokeEnvelope& e) {
            InvokeResult r;
            if (e.method == "save") {
                auto p1 = e.args.find('|');
                auto p2 = e.args.find('|', p1 + 1);
                std::string origin = e.args.substr(0, p1);
                std::string user = e.args.substr(p1 + 1, p2 - p1 - 1);
                std::string pwd = e.args.substr(p2 + 1);
                if (origin != e.origin) {
                    r.ok = false;
                    r.error = Error::scope(ErrCode::OriginNotAllowed, "origin mismatch");
                    return r;
                }
                secure_.store(origin, user, pwd);
                r.ok = true; r.value = "saved";
            } else if (e.method == "lookup") {
                if (e.args != e.origin) {
                    r.ok = false;
                    r.error = Error::scope(ErrCode::OriginNotAllowed, "origin mismatch");
                    return r;
                }
                auto creds = secure_.lookup(e.args);
                r.ok = true; r.value = "[" + std::to_string(creds.size()) + " creds]";
            } else { r.ok = false; r.error = Error::internal("unknown"); }
            return r;
        });
        broker_.registerHandler("ScriptInjection", [this](const InvokeEnvelope& e) {
            InvokeResult r;
            auto pos = e.args.find('|');
            TabId tab = std::stoi(e.args.substr(0, pos));
            auto res = injector_.inject(tab, e.origin, e.args.substr(pos + 1),
                                        InjectTiming::DocumentEnd);
            if (!res.isOk()) { r.ok = false; r.error = res.error(); return r; }
            r.ok = true; r.value = "injected";
            return r;
        });
        broker_.registerHandler("URLLoaderFactory", [](const InvokeEnvelope&) {
            InvokeResult r; r.ok = true; r.value = "fetched"; return r;
        });
        broker_.registerHandler("FileDialog", [](const InvokeEnvelope&) {
            InvokeResult r; r.ok = true; r.value = "fh-1|file.txt"; return r;
        });
        broker_.registerHandler("Notification", [](const InvokeEnvelope&) {
            InvokeResult r; r.ok = true; r.value = "notified"; return r;
        });
        broker_.registerHandler("TabOrigin", [this](const InvokeEnvelope& e) {
            InvokeResult r;
            auto t = tabs_.get(std::stoi(e.args));
            if (!t) { r.ok = false; r.error = Error::internal("tab not found"); return r; }
            r.ok = true; r.value = t->origin;
            return r;
        });
        broker_.registerHandler("Ping", [](const InvokeEnvelope&) {
            InvokeResult r; r.ok = true; r.value = "pong"; return r;
        });
    }

    Database db_;
    AuditLog audit_;
    GrantStore grants_;
    PluginHost plugins_;
    ConfirmationManager confirm_;
    dm::ProcessManager pm_;
    IPCBroker broker_;
    TabManager tabs_;
    BookmarkStore bookmarks_;
    DownloadStore downloads_;
    SecureStore secure_;
    ScriptInjectionManager injector_;
    SignatureVerifier verifier_;
    PluginMarket market_;
    EnterprisePolicy policy_;
    LTSChannel lts_;
    GovernanceCouncil council_;
    DisputeManager disputes_;
    EcosystemMetrics metrics_;
    AnnualReport report_;
    dm::SiteIsolator siteIsolator_;
    dm::Sandbox sandbox_;
    int64_t bootMs_{0};
    int64_t traceSeq_{0};
};

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    try {
        std::cout << "=== 大明DM浏览器 完整代码库 + SQLite 持久化自测 ===\n\n";

        std::remove("dm_browser_test.db");

        DMBrowser browser;
        auto bootResult = browser.boot("dm_browser_test.db");
        check(bootResult.isOk(), "内核启动成功");
        if (!bootResult.isOk()) {
            std::cout << "  错误: " << bootResult.error().msg << "\n";
            return 1;
        }
        check(browser.bootMs() < 500, "启动 < 500ms");

        browser.confirm().setPrompt([](const std::string&, const std::string&,
                                        const std::string&) {
            return ConfirmationManager::Decision::Always;
        });

        std::cout << "\n--- 里程碑 1: 标签页 ---\n";
        TabId t1 = browser.tabs().create("https://start.dm");
        check(browser.tabs().count() == 1, "创建标签成功");
        auto tr = browser.call("dm.tabbar", "TabOrigin", "get", std::to_string(t1));
        check(tr.ok, "TabOrigin 调用成功");
        check(tr.value == "https://start.dm", "origin 正确");

        std::cout << "\n--- 里程碑 2: 书签 + 下载 ---\n";
        auto b1 = browser.call("dm.bookmark", "BookmarkStore", "add", "DM|https://dm.dm");
        check(b1.ok, "添加书签成功");
        check(browser.bookmarks().size() == 1, "书签落盘数 1");
        auto b2 = browser.call("dm.bookmark", "BookmarkStore", "add", "Search|https://s.dm");
        check(b2.ok, "添加第二个书签");
        check(browser.bookmarks().size() == 2, "书签落盘数 2");
        auto d1 = browser.call("dm.download", "DownloadStore", "create",
                               "https://f.dm/a.zip|a.zip");
        check(d1.ok, "创建下载记录成功");
        check(browser.downloads().size() == 1, "下载记录落盘数 1");

        std::cout << "\n--- 里程碑 3: 密码 + 注入 ---\n";
        auto s1 = browser.call("dm.password", "SecureStore", "save",
                               "https://bank.dm|alice|pw123", "https://bank.dm");
        check(s1.ok, "保存凭据成功");
        check(browser.secure().size() == 1, "凭据落盘数 1");
        auto s2 = browser.call("dm.password", "SecureStore", "lookup",
                               "https://shop.dm", "https://bank.dm");
        check(!s2.ok, "跨 origin 查询被拒");
        check(s2.error.code == ErrCode::OriginNotAllowed, "错误码正确");
        auto i1 = browser.call("dm.password", "ScriptInjection", "inject",
                               std::to_string(t1) + "|fill();", "https://bank.dm");
        check(i1.ok, "注入脚本成功");
        int rejected = 0;
        for (int i = 0; i < 12; ++i) {
            auto r = browser.call("dm.password", "ScriptInjection", "inject",
                                  std::to_string(t1) + "|x();", "https://bank.dm");
            if (!r.ok) rejected++;
        }
        check(rejected == 5, "注入频率限制生效 (拒绝 5 次)");

        std::cout << "\n--- 里程碑 4: 市场 + 签名 ---\n";
        MarketPlugin p1;
        p1.pluginId = "dm.official.bm";
        p1.name = "Official Bookmarks";
        p1.version = "1.0.0";
        p1.keyId = "official-key";
        p1.endpoints = {"BookmarkStore"};
        std::string pl1 = p1.pluginId + "@" + p1.version;
        p1.signature = SignatureVerifier::makeSignature(p1.keyId, pl1);
        auto m1 = browser.market().publish(p1, pl1);
        check(m1.isOk(), "官方插件上架成功");
        check(browser.market().get("dm.official.bm")->tier == MarketTier::Official,
              "层级为 Official");
        MarketPlugin p2;
        p2.pluginId = "dm.unsigned.theme";
        p2.name = "Theme";
        p2.version = "0.1.0";
        p2.endpoints = {"OSInfo"};
        auto m2 = browser.market().publish(p2, "");
        check(m2.isOk(), "未签名插件上架成功");
        check(browser.market().get("dm.unsigned.theme")->tier == MarketTier::Unsigned,
              "层级为 Unsigned");
        check(browser.market().withdraw("dm.unsigned.theme"), "撤回成功");
        auto m3 = browser.market().publish(p2, "");
        check(!m3.isOk(), "撤回后重新上架被拒");

        std::cout << "\n--- 里程碑 5: 企业策略 + LTS ---\n";
        EnterprisePolicyData policyData;
        policyData.policyId = "ent-001";
        policyData.mirrorUrl = "https://mirror.dm";
        policyData.pluginWhitelist = {"dm.official.bm"};
        policyData.auditRetentionDays = 30;
        std::string adminSig = EnterprisePolicy::makeAdminSignature("admin-key", policyData);
        auto pr = browser.policy().load(policyData, "admin-key", adminSig);
        check(pr.isOk(), "企业策略加载成功");
        check(browser.policy().isWhitelisted("dm.official.bm"), "白名单内");
        check(!browser.policy().isWhitelisted("dm.evil"), "白名单外");
        ReleaseInfo lts1;
        lts1.version = "1.0.0";
        lts1.channel = ReleaseChannel::LTS;
        lts1.publishedAt = nowMs();
        lts1.eolAt = nowMs() + 86400000LL * 365 * 3;
        browser.lts().addRelease(lts1);
        check(browser.lts().currentLTS() == "1.0.0", "LTS 版本 1.0.0");
        check(!browser.lts().isEOL("1.0.0"), "1.0.0 未 EOL");

        std::cout << "\n--- 里程碑 6: 治理 + 争议 ---\n";
        Governor g1;
        g1.memberId = "m1"; g1.displayName = "Alice";
        g1.role = GovernanceRole::CoreMaintainer;
        check(browser.council().addMember(g1), "添加治理成员");
        browser.council().addMember(Governor{"m2", "Bob",
            GovernanceRole::SecurityReviewer, nowMs()});
        browser.council().addMember(Governor{"m3", "Carol",
            GovernanceRole::EcosystemSteward, nowMs()});
        browser.council().addMember(Governor{"m4", "Dave",
            GovernanceRole::CommunityContributor, nowMs()});
        check(browser.council().memberCount() == 4, "成员数 4");
        Decision decision1;
        decision1.decisionId = "d-001";
        decision1.title = "新增 Notification";
        decision1.rationale = "用户需求";
        decision1.finalRuling = "批准";
        browser.council().recordDecision(decision1);
        browser.council().vote("d-001", "m1", true);
        browser.council().vote("d-001", "m2", true);
        browser.council().vote("d-001", "m3", false);
        auto tally = browser.council().tally("d-001");
        check(tally.first == 2 && tally.second == 1, "投票统计 2:1");
        auto c1 = browser.disputes().report("dm.official.bm", "user-1", "疑似数据收集");
        check(c1.isOk(), "提交争议");
        browser.disputes().investigate(c1.value());
        browser.disputes().resolve(c1.value(), DisputeRuling::Warning, "警告");
        browser.disputes().appeal(c1.value(), "已整改");
        browser.disputes().close(c1.value());
        check(browser.disputes().count() == 1, "争议数 1");

        std::cout << "\n--- 生态指标 + 年度报告 ---\n";
        auto m = browser.metrics().compute();
        check(m.pluginTotal >= 1, "插件总数 >= 1");
        check(m.governorCount == 4, "治理成员 4");
        check(m.disputeTotal == 1, "争议数 1");
        check(m.healthScore == -1.0 ||
              (m.healthScore >= 0 && m.healthScore <= 100),
              "健康度合法（样本不足时为 N/A）");
        std::string report = browser.report().generate(2026);
        check(report.find("插件生态") != std::string::npos, "报告含插件生态");
        check(report.find("治理") != std::string::npos, "报告含治理");
        check(report.find("争议") != std::string::npos, "报告含争议");
        std::cout << "\n" << report << "\n";

        std::cout << "\n--- 审计持久化 ---\n";
        auto auditSize = browser.audit().size();
        check(auditSize > 0, "审计记录落盘 (" + std::to_string(auditSize) + " 条)");
        auto recent = browser.audit().recent(5);
        check(!recent.empty(), "最近审计查询成功");
        std::cout << "  最近 5 条审计:\n";
        for (const auto& e : recent) {
            std::cout << "    #" << e.seq << " " << e.pluginId
                      << " " << e.endpoint << "." << e.method
                      << " -> " << e.result << "\n";
        }

        std::cout << "\n--- 授权撤销验证 ---\n";
        auto gid = browser.grants().grant("dm.revoke.test", "TestEndpoint");
        check(browser.grants().validate(gid, "TestEndpoint"), "新授权有效");
        browser.grants().revokeAll("dm.revoke.test");
        check(!browser.grants().validate(gid, "TestEndpoint"), "撤销后授权无效");

        std::cout << "\n--- 重启验证 ---\n";
        size_t bookmarkCount = browser.bookmarks().size();
        size_t downloadCount = browser.downloads().size();
        size_t credentialCount = browser.secure().size();
        size_t grantCount = browser.grants().size();

        DMBrowser browser2;
        auto boot2 = browser2.boot("dm_browser_test.db");
        check(boot2.isOk(), "第二次启动成功");
        check(browser2.bookmarks().size() == bookmarkCount,
              "重启后书签数不变 (" + std::to_string(bookmarkCount) + ")");
        check(browser2.downloads().size() == downloadCount,
              "重启后下载数不变 (" + std::to_string(downloadCount) + ")");
        check(browser2.secure().size() == credentialCount,
              "重启后凭据数不变 (" + std::to_string(credentialCount) + ")");
        check(browser2.grants().size() == grantCount,
              "重启后授权数不变 (" + std::to_string(grantCount) + ")");
        check(browser2.council().memberCount() == 4, "重启后治理成员数不变");
        check(browser2.disputes().count() == 1, "重启后争议数不变");
        check(browser2.lts().currentLTS() == "1.0.0", "重启后 LTS 不变");

        std::cout << "\n--- A+B: 跨进程 IPC ---\n";
        auto spawn = browser.spawnPluginHost("dm.test.plugin", false);
        check(spawn.isOk(), "启动插件宿主进程");
        if (spawn.isOk()) {
            auto info = spawn.value();
            check(info.pid > 0, "进程 PID > 0");
            check(browser.processManager().isAlive(info.pid), "进程存活");

            auto srv = browser.broker().startServerFor(info.pid, info.pipeName);
            check(srv.isOk(), "创建管道服务端");

            std::this_thread::sleep_for(std::chrono::milliseconds(800));

            check(browser.processManager().isAlive(info.pid),
                  "睡眠后插件进程仍存活");

            auto wait = browser.broker().waitForPlugin(info.pid);
            check(wait.isOk(), "插件已连接管道");

            InvokeEnvelope env;
            env.grantId = browser.grants().grant("dm.test.plugin", "Ping");
            env.endpoint = "Ping";
            env.method = "greet";
            env.args = "ipc-test";
            env.traceId = "t-ipc-1";
            auto resp = browser.broker().sendInvokeToPlugin(
                "dm.test.plugin", info.pid, env);
            if (!resp.ok) {
                std::cout << "  [诊断] " << resp.error.msg << "\n";
            }
            check(resp.ok, "跨进程 Invoke 成功");
            check(resp.value.find("hello from example plugin") != std::string::npos,
                  "插件返回内容正确");
            check(resp.value.find("ipc-test") != std::string::npos,
                  "插件收到 args 正确");

            browser.processManager().terminate(info.pid);
            check(!browser.processManager().isAlive(info.pid), "进程已终止");
        }

        // ============================================================
        // 插件热加载
        // ============================================================
        std::cout << "\n--- 插件热加载 ---\n";
        auto spawnHL = browser.spawnPluginHost("dm.hotload.test", false);
        check(spawnHL.isOk(), "启动热加载测试进程");
        if (spawnHL.isOk()) {
            auto hlInfo = spawnHL.value();

            auto srvHL = browser.broker().startServerFor(hlInfo.pid, hlInfo.pipeName);
            check(srvHL.isOk(), "创建热加载管道");

            std::this_thread::sleep_for(std::chrono::milliseconds(800));

            auto waitHL = browser.broker().waitForPlugin(hlInfo.pid);
            check(waitHL.isOk(), "热加载插件已连接");

            // 1. 调用 greet
            InvokeEnvelope env1;
            env1.grantId = browser.grants().grant("dm.hotload.test", "Ping");
            env1.endpoint = "Ping";
            env1.method = "greet";
            env1.args = "world";
            env1.traceId = "t-hl-1";
            auto r1 = browser.broker().sendInvokeToPlugin(
                "dm.hotload.test", hlInfo.pid, env1);
            check(r1.ok, "调用 greet 成功");
            check(r1.value.find("hello from example plugin") != std::string::npos,
                  "greet 返回正确");

            // 2. 发 reload
            auto r2 = browser.broker().sendControlToPlugin(
                "dm.hotload.test", hlInfo.pid, "reload", "");
            check(r2.ok, "reload 命令成功");
            check(r2.value.find("reloaded") != std::string::npos,
                  "reload 返回 reloaded");
            check(r2.value.find("load #2") != std::string::npos,
                  "reload 后 loadCount=2");

            // 3. 重载后再调 greet
            InvokeEnvelope env3;
            env3.grantId = browser.grants().grant("dm.hotload.test", "Ping");
            env3.endpoint = "Ping";
            env3.method = "greet";
            env3.args = "after-reload";
            env3.traceId = "t-hl-3";
            auto r3 = browser.broker().sendInvokeToPlugin(
                "dm.hotload.test", hlInfo.pid, env3);
            check(r3.ok, "重载后 greet 成功");
            check(r3.value.find("after-reload") != std::string::npos,
                  "重载后 greet 返回正确");

            browser.processManager().terminate(hlInfo.pid);
            check(!browser.processManager().isAlive(hlInfo.pid), "热加载进程已终止");
        }

        std::cout << "\n--- A+B: 站点隔离 ---\n";
        std::string origin1 = dm::SiteIsolator::normalizeOrigin(
            "https://www.bank.dm/login");
        check(origin1 == "https://bank.dm", "origin 规范化正确");
        check(dm::SiteIsolator::sameSite("https://a.example.com",
                                          "https://b.example.com"),
              "同站判定正确");
        check(!dm::SiteIsolator::sameSite("https://a.example.com",
                                           "https://b.other.com"),
              "跨站判定正确");
        check(browser.siteIsolator().isSensitive("https://bank.dm/x"),
              "敏感站点识别正确");

        std::cout << "\n--- A+B: 沙箱 ---\n";
        auto sandboxResult = browser.spawnPluginHost("dm.sandbox.test", true);
        check(sandboxResult.isOk(), "敏感插件进程启动");
        if (sandboxResult.isOk()) {
            check(browser.processManager().isSensitive(sandboxResult.value().pid),
                  "标记为敏感进程");
            browser.processManager().terminate(sandboxResult.value().pid);
        }

        std::cout << "\n=== 结果: " << g_pass << " 通过, " << g_fail << " 失败 ===\n";
        return g_fail == 0 ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "\n未捕获异常: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\n未知异常" << std::endl;
        return 1;
    }
}