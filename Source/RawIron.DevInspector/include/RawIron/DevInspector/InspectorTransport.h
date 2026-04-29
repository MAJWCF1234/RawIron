#pragma once

#include <functional>
#include <memory>
#include <string>

namespace ri::dev {

/// Sends inspector payloads (typically one JSON object per message) to an external sink.
/// Implementations may stream over TCP/WebSocket/stdin-out, append to ring buffers, etc.
class IInspectorTransport {
public:
    virtual ~IInspectorTransport() = default;

    virtual void Send(std::string message) = 0;
};

/// No-op sink; default when no external tooling is attached.
class NullInspectorTransport final : public IInspectorTransport {
public:
    void Send(std::string /*message*/) override {}
};

/// For tests and embedding: invoked on \ref DevelopmentInspector::Pump thread.
class CallbackInspectorTransport final : public IInspectorTransport {
public:
    explicit CallbackInspectorTransport(std::function<void(const std::string&)> cb) : cb_(std::move(cb)) {}

    void Send(std::string message) override {
        if (cb_) {
            cb_(message);
        }
    }

private:
    std::function<void(const std::string&)> cb_;
};

} // namespace ri::dev
