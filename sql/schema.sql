-- 大明DM浏览器 数据库 schema v1.0
PRAGMA foreign_keys = ON;
PRAGMA journal_mode = WAL;

CREATE TABLE IF NOT EXISTS process_table (
    pid INTEGER PRIMARY KEY,
    plugin_id TEXT NOT NULL,
    proc_type TEXT NOT NULL,
    state TEXT NOT NULL,
    sensitive INTEGER DEFAULT 0,
    cpu_budget INTEGER DEFAULT 0,
    mem_budget INTEGER DEFAULT 0,
    created_at INTEGER NOT NULL,
    last_active_at INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS site_process_map (
    site TEXT PRIMARY KEY,
    pid INTEGER NOT NULL,
    is_sensitive INTEGER DEFAULT 0,
    bound_at INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS capability_grant (
    grant_id TEXT PRIMARY KEY,
    plugin_id TEXT NOT NULL,
    endpoint TEXT NOT NULL,
    scope TEXT DEFAULT '{}',
    version INTEGER DEFAULT 1,
    granted_at INTEGER NOT NULL,
    expires_at INTEGER DEFAULT 0,
    revoked_at INTEGER DEFAULT 0,
    requires_confirmation INTEGER DEFAULT 0,
    signature TEXT DEFAULT ''
);

CREATE TABLE IF NOT EXISTS audit_log (
    seq INTEGER PRIMARY KEY AUTOINCREMENT,
    trace_id TEXT NOT NULL,
    plugin_id TEXT NOT NULL,
    endpoint TEXT NOT NULL,
    method TEXT NOT NULL,
    priority TEXT DEFAULT 'normal',
    result TEXT NOT NULL,
    error_code TEXT DEFAULT '',
    duration_ms INTEGER DEFAULT 0,
    retry_count INTEGER DEFAULT 0,
    created_at INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS plugin_manifest (
    plugin_id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    version TEXT NOT NULL,
    tier TEXT NOT NULL,
    endpoints TEXT DEFAULT '[]',
    privacy_labels TEXT DEFAULT '[]',
    dependencies TEXT DEFAULT '[]',
    signature TEXT DEFAULT '',
    author_id TEXT DEFAULT '',
    cached_at INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS plugin_state (
    plugin_id TEXT PRIMARY KEY,
    state TEXT NOT NULL,
    enabled_at INTEGER DEFAULT 0,
    suspended_at INTEGER DEFAULT 0,
    unloaded_at INTEGER DEFAULT 0
);

CREATE TABLE IF NOT EXISTS plugin_storage (
    plugin_id TEXT NOT NULL,
    key TEXT NOT NULL,
    value BLOB,
    updated_at INTEGER NOT NULL,
    PRIMARY KEY (plugin_id, key)
);

CREATE TABLE IF NOT EXISTS bookmarks (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    title TEXT NOT NULL,
    url TEXT NOT NULL,
    created_at INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS downloads (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    url TEXT NOT NULL,
    filename TEXT NOT NULL,
    state TEXT NOT NULL,
    bytes_received INTEGER DEFAULT 0,
    total_bytes INTEGER DEFAULT 0,
    created_at INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS credentials (
    origin TEXT NOT NULL,
    username TEXT NOT NULL,
    encrypted_password TEXT NOT NULL,
    created_at INTEGER NOT NULL,
    PRIMARY KEY (origin, username)
);

CREATE TABLE IF NOT EXISTS market_plugin (
    plugin_id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    version TEXT NOT NULL,
    tier TEXT NOT NULL,
    key_id TEXT DEFAULT '',
    signature TEXT DEFAULT '',
    endpoints TEXT DEFAULT '[]',
    privacy_labels TEXT DEFAULT '[]',
    score REAL DEFAULT 0,
    downloads INTEGER DEFAULT 0,
    author_id TEXT DEFAULT '',
    published_at INTEGER NOT NULL,
    withdrawn_at INTEGER DEFAULT 0
);

CREATE TABLE IF NOT EXISTS dispute_case (
    case_id TEXT PRIMARY KEY,
    plugin_id TEXT NOT NULL,
    reporter_id TEXT NOT NULL,
    reason TEXT NOT NULL,
    status TEXT NOT NULL,
    ruling TEXT DEFAULT 'none',
    resolution TEXT DEFAULT '',
    created_at INTEGER NOT NULL,
    resolved_at INTEGER DEFAULT 0,
    closed_at INTEGER DEFAULT 0
);

CREATE TABLE IF NOT EXISTS dispute_appeal (
    case_id TEXT NOT NULL,
    note TEXT NOT NULL,
    created_at INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS governor (
    member_id TEXT PRIMARY KEY,
    display_name TEXT NOT NULL,
    role TEXT NOT NULL,
    joined_at INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS decision (
    decision_id TEXT PRIMARY KEY,
    title TEXT NOT NULL,
    rationale TEXT DEFAULT '',
    objections TEXT DEFAULT '[]',
    final_ruling TEXT DEFAULT '',
    decided_at INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS vote (
    decision_id TEXT NOT NULL,
    member_id TEXT NOT NULL,
    in_favor INTEGER NOT NULL,
    created_at INTEGER NOT NULL,
    PRIMARY KEY (decision_id, member_id)
);

CREATE TABLE IF NOT EXISTS enterprise_policy (
    policy_id TEXT PRIMARY KEY,
    admin_signature TEXT NOT NULL,
    plugin_whitelist TEXT DEFAULT '[]',
    endpoint_quota TEXT DEFAULT '{}',
    audit_retention_days INTEGER DEFAULT 7,
    mirror_url TEXT DEFAULT '',
    updated_at INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS mirror_plugin (
    plugin_id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    version TEXT NOT NULL,
    mirror_signature TEXT NOT NULL,
    endpoints TEXT DEFAULT '[]'
);

CREATE TABLE IF NOT EXISTS release_record (
    version TEXT PRIMARY KEY,
    channel TEXT NOT NULL,
    published_at INTEGER NOT NULL,
    eol_at INTEGER DEFAULT 0,
    notes TEXT DEFAULT ''
);

CREATE TABLE IF NOT EXISTS ecosystem_metric (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    period TEXT NOT NULL,
    plugin_total INTEGER,
    official_count INTEGER,
    community_count INTEGER,
    unsigned_count INTEGER,
    withdrawn_count INTEGER,
    avg_endpoints REAL,
    governor_count INTEGER,
    dispute_total INTEGER,
    health_score REAL,
    created_at INTEGER NOT NULL
);
