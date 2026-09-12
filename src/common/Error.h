#pragma once
#include <string>

enum class ErrLayer { None, Auth, Scope, Resource, Internal };
enum class ErrCode {
    Ok = 0,
    GrantNotFound, GrantRevoked, GrantExpired,
    OriginNotAllowed, QuotaExceeded, GestureRequired,
    Timeout, BudgetExceeded, PoolExhausted,
    SignatureInvalid, SignatureExpired, KeyNotFound,
    TierInsufficient, PluginWithdrawn, ManifestInvalid,
    PolicyViolation, PluginNotWhitelisted, PluginNotInMirror,
    MirrorSignatureInvalid, ChannelMismatch,
    CaseNotFound, CaseAlreadyClosed, InvalidVote,
    DbError,
    InternalFailure
};

struct Error {
    ErrLayer layer{ErrLayer::None};
    ErrCode  code{ErrCode::Ok};
    std::string msg;

    bool ok() const { return layer == ErrLayer::None; }

    static Error none() { return {}; }
    static Error auth(ErrCode c, std::string m)  { return {ErrLayer::Auth, c, std::move(m)}; }
    static Error scope(ErrCode c, std::string m) { return {ErrLayer::Scope, c, std::move(m)}; }
    static Error res(ErrCode c, std::string m)   { return {ErrLayer::Resource, c, std::move(m)}; }
    static Error internal(std::string m)         { return {ErrLayer::Internal, ErrCode::InternalFailure, std::move(m)}; }
    static Error db(std::string m)               { return {ErrLayer::Internal, ErrCode::DbError, std::move(m)}; }
};
