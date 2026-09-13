#include "ipc/Framing.h"
#include "ipc/Pipe.h"
#include <cstring>

namespace dm::ipc {

void Framing::putString(std::vector<uint8_t>& out, const std::string& s) {
    uint32_t len = static_cast<uint32_t>(s.size());
    out.push_back(static_cast<uint8_t>(len & 0xff));
    out.push_back(static_cast<uint8_t>((len >> 8) & 0xff));
    out.push_back(static_cast<uint8_t>((len >> 16) & 0xff));
    out.push_back(static_cast<uint8_t>((len >> 24) & 0xff));
    out.insert(out.end(), s.begin(), s.end());
}

Result<std::string> Framing::getString(const std::vector<uint8_t>& in,
                                       size_t& offset) {
    if (offset + 4 > in.size())
        return Result<std::string>::fail(Error::internal("truncated length"));
    uint32_t len = static_cast<uint32_t>(in[offset])
                 | (static_cast<uint32_t>(in[offset + 1]) << 8)
                 | (static_cast<uint32_t>(in[offset + 2]) << 16)
                 | (static_cast<uint32_t>(in[offset + 3]) << 24);
    offset += 4;
    if (offset + len > in.size())
        return Result<std::string>::fail(Error::internal("truncated string"));
    std::string s(reinterpret_cast<const char*>(in.data() + offset), len);
    offset += len;
    return Result<std::string>::ok(std::move(s));
}

std::vector<uint8_t> Framing::encodeInvoke(
    const std::string& grantId, const std::string& endpoint,
    const std::string& method, const std::string& args,
    const std::string& traceId, const std::string& origin) {
    std::vector<uint8_t> out;
    out.push_back(static_cast<uint8_t>(Kind::Invoke));
    putString(out, grantId);
    putString(out, endpoint);
    putString(out, method);
    putString(out, args);
    putString(out, traceId);
    putString(out, origin);
    return out;
}

std::vector<uint8_t> Framing::encodeResult(bool ok, const std::string& value,
                                           int32_t errorCode,
                                           const std::string& errorMsg) {
    std::vector<uint8_t> out;
    out.push_back(static_cast<uint8_t>(Kind::Result));
    out.push_back(ok ? 1 : 0);
    putString(out, value);
    out.push_back(static_cast<uint8_t>(errorCode & 0xff));
    out.push_back(static_cast<uint8_t>((errorCode >> 8) & 0xff));
    out.push_back(static_cast<uint8_t>((errorCode >> 16) & 0xff));
    out.push_back(static_cast<uint8_t>((errorCode >> 24) & 0xff));
    putString(out, errorMsg);
    return out;
}

std::vector<uint8_t> Framing::encodeHello(const std::string& pluginId,
                                          int32_t pid) {
    std::vector<uint8_t> out;
    out.push_back(static_cast<uint8_t>(Kind::Hello));
    putString(out, pluginId);
    out.push_back(static_cast<uint8_t>(pid & 0xff));
    out.push_back(static_cast<uint8_t>((pid >> 8) & 0xff));
    out.push_back(static_cast<uint8_t>((pid >> 16) & 0xff));
    out.push_back(static_cast<uint8_t>((pid >> 24) & 0xff));
    return out;
}

Framing::Kind Framing::peekKind(const std::vector<uint8_t>& frame) {
    if (frame.empty()) return Kind::Shutdown;
    return static_cast<Kind>(frame[0]);
}

Result<Framing::InvokeMessage> Framing::decodeInvoke(
    const std::vector<uint8_t>& frame) {
    if (frame.empty() || frame[0] != static_cast<uint8_t>(Kind::Invoke))
        return Result<InvokeMessage>::fail(Error::internal("not an invoke"));
    size_t off = 1;
    InvokeMessage m;
    auto r1 = getString(frame, off); if (!r1.isOk()) return Result<InvokeMessage>::fail(r1.error()); m.grantId = r1.value();
    auto r2 = getString(frame, off); if (!r2.isOk()) return Result<InvokeMessage>::fail(r2.error()); m.endpoint = r2.value();
    auto r3 = getString(frame, off); if (!r3.isOk()) return Result<InvokeMessage>::fail(r3.error()); m.method = r3.value();
    auto r4 = getString(frame, off); if (!r4.isOk()) return Result<InvokeMessage>::fail(r4.error()); m.args = r4.value();
    auto r5 = getString(frame, off); if (!r5.isOk()) return Result<InvokeMessage>::fail(r5.error()); m.traceId = r5.value();
    auto r6 = getString(frame, off); if (!r6.isOk()) return Result<InvokeMessage>::fail(r6.error()); m.origin = r6.value();
    return Result<InvokeMessage>::ok(std::move(m));
}

Result<Framing::ResultMessage> Framing::decodeResult(
    const std::vector<uint8_t>& frame) {
    if (frame.empty() || frame[0] != static_cast<uint8_t>(Kind::Result))
        return Result<ResultMessage>::fail(Error::internal("not a result"));
    size_t off = 1;
    if (off >= frame.size()) return Result<ResultMessage>::fail(Error::internal("truncated"));
    ResultMessage m;
    m.ok = frame[off++] != 0;
    auto r1 = getString(frame, off); if (!r1.isOk()) return Result<ResultMessage>::fail(r1.error()); m.value = r1.value();
    if (off + 4 > frame.size()) return Result<ResultMessage>::fail(Error::internal("truncated code"));
    m.errorCode = static_cast<int32_t>(frame[off])
                | (static_cast<int32_t>(frame[off + 1]) << 8)
                | (static_cast<int32_t>(frame[off + 2]) << 16)
                | (static_cast<int32_t>(frame[off + 3]) << 24);
    off += 4;
    auto r2 = getString(frame, off); if (!r2.isOk()) return Result<ResultMessage>::fail(r2.error()); m.errorMsg = r2.value();
    return Result<ResultMessage>::ok(std::move(m));
}

Result<Framing::HelloMessage> Framing::decodeHello(
    const std::vector<uint8_t>& frame) {
    if (frame.empty() || frame[0] != static_cast<uint8_t>(Kind::Hello))
        return Result<HelloMessage>::fail(Error::internal("not a hello"));
    size_t off = 1;
    HelloMessage m;
    auto r1 = getString(frame, off); if (!r1.isOk()) return Result<HelloMessage>::fail(r1.error()); m.pluginId = r1.value();
    if (off + 4 > frame.size()) return Result<HelloMessage>::fail(Error::internal("truncated pid"));
    m.pid = static_cast<int32_t>(frame[off])
          | (static_cast<int32_t>(frame[off + 1]) << 8)
          | (static_cast<int32_t>(frame[off + 2]) << 16)
          | (static_cast<int32_t>(frame[off + 3]) << 24);
    return Result<HelloMessage>::ok(std::move(m));
}

Result<bool> Framing::writeFrame(Pipe& pipe,
                                 const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> frame;
    uint32_t len = static_cast<uint32_t>(payload.size());
    frame.push_back(static_cast<uint8_t>(len & 0xff));
    frame.push_back(static_cast<uint8_t>((len >> 8) & 0xff));
    frame.push_back(static_cast<uint8_t>((len >> 16) & 0xff));
    frame.push_back(static_cast<uint8_t>((len >> 24) & 0xff));
    frame.insert(frame.end(), payload.begin(), payload.end());

    auto r = pipe.write(frame);
    if (!r.isOk()) return Result<bool>::fail(r.error());
    return Result<bool>::ok(true);
}

Result<std::vector<uint8_t>> Framing::readFrame(Pipe& pipe) {
    auto header = pipe.read(4);
    if (!header.isOk()) return Result<std::vector<uint8_t>>::fail(header.error());
    if (header.value().size() != 4)
        return Result<std::vector<uint8_t>>::fail(Error::internal("short header"));

    uint32_t len = static_cast<uint32_t>(header.value()[0])
                 | (static_cast<uint32_t>(header.value()[1]) << 8)
                 | (static_cast<uint32_t>(header.value()[2]) << 16)
                 | (static_cast<uint32_t>(header.value()[3]) << 24);
    if (len == 0 || len > 16 * 1024 * 1024)
        return Result<std::vector<uint8_t>>::fail(Error::internal("bad frame length"));

    auto body = pipe.read(len);
    if (!body.isOk()) return Result<std::vector<uint8_t>>::fail(body.error());
    if (body.value().size() != len)
        return Result<std::vector<uint8_t>>::fail(Error::internal("short body"));
    return Result<std::vector<uint8_t>>::ok(std::move(body.value()));
}

} // namespace dm::ipc