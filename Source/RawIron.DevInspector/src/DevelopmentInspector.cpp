#include "RawIron/DevInspector/DevelopmentInspector.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <exception>
#include <sstream>
#include <utility>

namespace ri::dev {
namespace {

std::string JsonEscape(std::string_view in) {
    std::string out;
    out.reserve(in.size() + 8);
    for (char c : in) {
        switch (c) {
        case '\\':
            out += "\\\\";
            break;
        case '\"':
            out += "\\\"";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                char buf[12];
                std::snprintf(buf,
                              sizeof(buf),
                              "\\u%04x",
                              static_cast<unsigned int>(static_cast<unsigned char>(c)));
                out += buf;
            } else {
                out += c;
            }
        }
    }
    return out;
}

void TrimInPlace(std::string_view& s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
        s.remove_prefix(1);
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.remove_suffix(1);
    }
}

InspectorConfig NormalizeConfig(InspectorConfig config) {
    if (config.maxBufferedDiagnostics == 0) {
        config.maxBufferedDiagnostics = 8192;
    }
    return config;
}

std::string QuoteArg(std::string_view value) {
    std::string out = "\"";
    out.reserve(value.size() + 4);
    for (const char c : value) {
        if (c == '"') {
            out += "\\\"";
        } else {
            out += c;
        }
    }
    out += "\"";
    return out;
}

} // namespace

std::optional<std::string> BuildInspectorBrowserLaunchCommand(const InspectorBrowserLaunchOptions& options) {
    if (options.url.empty()) {
        return std::nullopt;
    }
#if defined(_WIN32)
    if (!options.browserPath.empty()) {
        const std::string browser = QuoteArg(options.browserPath);
        const std::string url = QuoteArg(options.url);
        if (options.kioskMode) {
            return browser + " --app=" + url + " --kiosk";
        }
        return browser + " " + url;
    }
    return std::string("start \"\" ") + QuoteArg(options.url);
#elif defined(__APPLE__)
    if (!options.browserPath.empty()) {
        const std::string app = QuoteArg(options.browserPath);
        const std::string url = QuoteArg(options.url);
        return std::string("open -a ") + app + " " + url;
    }
    return std::string("open ") + QuoteArg(options.url);
#else
    if (!options.browserPath.empty()) {
        return QuoteArg(options.browserPath) + " " + QuoteArg(options.url);
    }
    return std::string("xdg-open ") + QuoteArg(options.url);
#endif
}

bool LaunchInspectorBrowser(const InspectorBrowserLaunchOptions& options,
                            std::string* error,
                            std::string* commandUsed) {
    const std::optional<std::string> command = BuildInspectorBrowserLaunchCommand(options);
    if (!command.has_value()) {
        if (error != nullptr) {
            *error = "browser launch URL is empty";
        }
        return false;
    }
    if (commandUsed != nullptr) {
        *commandUsed = *command;
    }
    if (options.dryRun) {
        return true;
    }
    const int code = std::system(command->c_str());
    if (code != 0) {
        if (error != nullptr) {
            *error = "browser launch command failed with exit code " + std::to_string(code);
        }
        return false;
    }
    return true;
}

DevelopmentInspector::DevelopmentInspector(InspectorConfig config)
    : config_(NormalizeConfig(std::move(config)))
    , transport_(std::make_shared<NullInspectorTransport>()) {}

DevelopmentInspector::DevelopmentInspector(DevelopmentInspector&& other) noexcept {
    std::scoped_lock lock(other.mutex_);
    config_ = other.config_;
    transport_ = std::move(other.transport_);
    sources_ = std::move(other.sources_);
    pending_ = std::move(other.pending_);
    commandHandlers_ = std::move(other.commandHandlers_);
    snapshotSeq_ = other.snapshotSeq_;
    diagnosticSeq_ = other.diagnosticSeq_;
    diagnosticsDropped_ = other.diagnosticsDropped_;
    other.transport_ = std::make_shared<NullInspectorTransport>();
}

DevelopmentInspector& DevelopmentInspector::operator=(DevelopmentInspector&& other) noexcept {
    if (this == &other) {
        return *this;
    }
    std::scoped_lock selfLock(mutex_);
    std::scoped_lock otherLock(other.mutex_);
    config_ = other.config_;
    transport_ = std::move(other.transport_);
    sources_ = std::move(other.sources_);
    pending_ = std::move(other.pending_);
    commandHandlers_ = std::move(other.commandHandlers_);
    snapshotSeq_ = other.snapshotSeq_;
    diagnosticSeq_ = other.diagnosticSeq_;
    diagnosticsDropped_ = other.diagnosticsDropped_;
    other.transport_ = std::make_shared<NullInspectorTransport>();
    return *this;
}

void DevelopmentInspector::SetConfig(InspectorConfig config) {
    std::scoped_lock lock(mutex_);
    config_ = NormalizeConfig(std::move(config));
}

InspectorConfig DevelopmentInspector::Config() const {
    std::scoped_lock lock(mutex_);
    return config_;
}

bool DevelopmentInspector::IsEnabled() const {
    std::scoped_lock lock(mutex_);
    return config_.enabled;
}

void DevelopmentInspector::RegisterSnapshotSource(std::string id, SnapshotSource source) {
    std::scoped_lock lock(mutex_);
    auto it = std::find_if(sources_.begin(), sources_.end(), [&](const SnapshotEntry& e) { return e.id == id; });
    if (it != sources_.end()) {
        it->source = std::move(source);
    } else {
        sources_.push_back(SnapshotEntry{std::move(id), std::move(source)});
    }
}

void DevelopmentInspector::UnregisterSnapshotSource(std::string_view id) {
    std::scoped_lock lock(mutex_);
    sources_.erase(
        std::remove_if(sources_.begin(), sources_.end(), [&](const SnapshotEntry& e) { return e.id == id; }),
        sources_.end());
}

std::vector<std::string> DevelopmentInspector::SnapshotSourceIds() const {
    std::scoped_lock lock(mutex_);
    std::vector<std::string> out;
    out.reserve(sources_.size());
    for (const SnapshotEntry& e : sources_) {
        out.push_back(e.id);
    }
    return out;
}

std::string DevelopmentInspector::BuildSnapshotJson() const {
    std::scoped_lock lock(mutex_);
    ++snapshotSeq_;
    if (!config_.enabled) {
        std::ostringstream empty;
        empty << "{\"version\":2,\"seq\":" << snapshotSeq_ << ",\"sources\":{}}";
        return empty.str();
    }
    std::ostringstream out;
    out << "{\"version\":2,\"seq\":" << snapshotSeq_ << ",\"sources\":{";
    bool first = true;
    for (const SnapshotEntry& entry : sources_) {
        if (!entry.source) {
            continue;
        }
        std::string body;
        try {
            body = entry.source();
        } catch (const std::exception& ex) {
            body = std::string("{\"error\":\"") + JsonEscape(ex.what()) + "\"}";
        } catch (...) {
            body = "{\"error\":\"unknown exception in snapshot source\"}";
        }
        if (!first) {
            out << ',';
        }
        first = false;
        out << '\"' << JsonEscape(entry.id) << "\":\"" << JsonEscape(body) << '\"';
    }
    out << "}}";
    return out.str();
}

void DevelopmentInspector::PostDiagnostic(InspectorChannel channel, std::string message) {
    std::scoped_lock lock(mutex_);
    if (!config_.enabled) {
        return;
    }
    const std::size_t cap = config_.maxBufferedDiagnostics;
    while (!pending_.empty() && pending_.size() >= cap) {
        pending_.pop_front();
        ++diagnosticsDropped_;
    }
    pending_.push_back(PendingDiagnostic{channel, std::move(message)});
}

void DevelopmentInspector::SetTransport(std::shared_ptr<IInspectorTransport> transport) {
    std::scoped_lock lock(mutex_);
    transport_ = transport ? std::move(transport) : std::make_shared<NullInspectorTransport>();
}

std::shared_ptr<IInspectorTransport> DevelopmentInspector::Transport() const {
    std::scoped_lock lock(mutex_);
    return transport_;
}

void DevelopmentInspector::Pump() {
    std::deque<PendingDiagnostic> batch;
    std::shared_ptr<IInspectorTransport> sink;
    std::uint64_t firstSeq = 1;
    {
        std::scoped_lock lock(mutex_);
        if (!config_.enabled || pending_.empty()) {
            return;
        }
        batch.swap(pending_);
        sink = transport_;
        diagnosticSeq_ += batch.size();
        firstSeq = diagnosticSeq_ - batch.size() + 1;
    }
    for (std::size_t i = 0; i < batch.size(); ++i) {
        const PendingDiagnostic& d = batch[i];
        const std::uint64_t seq = firstSeq + i;
        std::ostringstream line;
        line << "{\"version\":2,\"channel\":" << static_cast<int>(d.channel) << ",\"seq\":" << seq << ",\"msg\":\""
             << JsonEscape(d.message) << "\"}";
        sink->Send(line.str());
    }
}

void DevelopmentInspector::ClearDiagnosticQueue() {
    std::scoped_lock lock(mutex_);
    pending_.clear();
}

std::uint64_t DevelopmentInspector::DiagnosticsDroppedCount() const {
    std::scoped_lock lock(mutex_);
    return diagnosticsDropped_;
}

void DevelopmentInspector::RegisterCommandHandler(std::string prefix, CommandHandler handler) {
    std::scoped_lock lock(mutex_);
    commandHandlers_.push_back(CommandEntry{std::move(prefix), std::move(handler)});
}

void DevelopmentInspector::UnregisterCommandHandlersWithPrefix(std::string_view prefix) {
    std::scoped_lock lock(mutex_);
    commandHandlers_.erase(
        std::remove_if(commandHandlers_.begin(),
                       commandHandlers_.end(),
                       [&](const CommandEntry& e) { return e.prefix == prefix; }),
        commandHandlers_.end());
}

std::string DevelopmentInspector::TryHandleCommand(std::string_view line) {
    std::scoped_lock lock(mutex_);
    if (!config_.enabled || !config_.allowDevelopmentCommands) {
        return {};
    }
    std::string_view rest = line;
    TrimInPlace(rest);
    if (rest.empty()) {
        return {};
    }

    const std::size_t space = rest.find(' ');
    const std::string_view name = space == std::string_view::npos ? rest : rest.substr(0, space);
    std::string_view tail = space == std::string_view::npos ? std::string_view{} : rest.substr(space + 1);
    TrimInPlace(tail);

    std::string_view argsJson = tail;
    const std::size_t brace = tail.find('{');
    if (brace != std::string_view::npos) {
        argsJson = tail.substr(brace);
        TrimInPlace(argsJson);
    }

    for (const CommandEntry& entry : commandHandlers_) {
        if (!entry.prefix.empty() && name.starts_with(std::string_view(entry.prefix))) {
            if (!entry.handler) {
                return {};
            }
            return entry.handler(name, argsJson);
        }
    }
    return {};
}

} // namespace ri::dev
