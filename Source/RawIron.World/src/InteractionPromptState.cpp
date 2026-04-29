#include "RawIron/World/InteractionPromptState.h"

#include <algorithm>
#include <cctype>

namespace ri::world {

namespace {

constexpr std::uint32_t ToMask(const InteractionPromptSuppression suppression) noexcept {
    return static_cast<std::uint32_t>(suppression);
}

void TrimWhitespace(std::string& value) {
    const auto notSpace = [](unsigned char ch) {
        return std::isspace(ch) == 0;
    };

    const auto begin = std::find_if(value.begin(), value.end(), notSpace);
    const auto end = std::find_if(value.rbegin(), value.rend(), notSpace).base();
    if (begin >= end) {
        value.clear();
        return;
    }
    value.assign(begin, end);
}

} // namespace

void InteractionPromptState::Show(InteractionPromptRequest request) {
    request.actionLabel = NormalizeLabel(std::move(request.actionLabel), false);
    request.verb = NormalizeLabel(std::move(request.verb), true);
    request.targetLabel = NormalizeLabel(std::move(request.targetLabel), true);
    request.fallbackLabel = NormalizeLabel(std::move(request.fallbackLabel), true);
    request_ = std::move(request);
    hasPrompt_ = true;
}

void InteractionPromptState::Clear() noexcept {
    request_ = {};
    hasPrompt_ = false;
}

void InteractionPromptState::SetSuppressed(const InteractionPromptSuppression suppression, const bool enabled) noexcept {
    const std::uint32_t mask = ToMask(suppression);
    if (enabled) {
        suppressionMask_ |= mask;
    } else {
        suppressionMask_ &= ~mask;
    }
}

bool InteractionPromptState::Suppressed() const noexcept {
    return suppressionMask_ != 0U;
}

std::uint32_t InteractionPromptState::SuppressionMask() const noexcept {
    return suppressionMask_;
}

InteractionPromptView InteractionPromptState::BuildView() const {
    InteractionPromptView view{};
    if (!hasPrompt_ || Suppressed()) {
        return view;
    }

    view.actionLabel = request_.actionLabel;
    view.verb = request_.verb.empty() ? std::string("INTERACT") : request_.verb;
    view.targetLabel = request_.targetLabel;

    std::string body;
    if (!view.targetLabel.empty()) {
        if (!view.verb.empty() && view.verb != "INTERACT") {
            body = view.verb + " " + view.targetLabel;
        } else {
            body = view.targetLabel;
        }
    } else if (!request_.fallbackLabel.empty()) {
        body = request_.fallbackLabel;
    } else {
        body = view.verb;
    }

    if (!view.actionLabel.empty()) {
        view.text = "[" + view.actionLabel + "] " + body;
    } else {
        view.text = body;
    }
    view.visible = !view.text.empty();
    return view;
}

std::string InteractionPromptState::NormalizeLabel(std::string text, const bool uppercase) {
    TrimWhitespace(text);
    bool pendingSpace = false;
    std::string normalized;
    normalized.reserve(text.size());

    for (const unsigned char raw : text) {
        if (std::isspace(raw) != 0) {
            pendingSpace = !normalized.empty();
            continue;
        }
        if (pendingSpace && !normalized.empty()) {
            normalized.push_back(' ');
        }
        pendingSpace = false;
        normalized.push_back(uppercase
                                 ? static_cast<char>(std::toupper(raw))
                                 : static_cast<char>(raw));
    }

    return normalized;
}

} // namespace ri::world
