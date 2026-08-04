#pragma once

#include "RawIron/Ui/UiFlowSession.h"

#include <cctype>
#include <cstddef>
#include <string>
#include <string_view>

namespace ri::ui {

/// Ren'Py-style `${variable}` interpolation against the flow session store.
/// Unknown ids expand to empty; unterminated or malformed `${` runs are copied verbatim so that
/// authoring mistakes stay visible instead of eating the rest of the line.
[[nodiscard]] inline std::string ResolveStoreText(const UiFlowSession& session, std::string_view input) {
    std::string out;
    out.reserve(input.size() + 24U);
    for (std::size_t i = 0; i < input.size();) {
        if (input[i] == '$' && i + 1U < input.size() && input[i + 1U] == '{') {
            std::size_t j = i + 2U;
            while (j < input.size()) {
                const unsigned char ch = static_cast<unsigned char>(input[j]);
                if (std::isalnum(ch) != 0 || input[j] == '_') {
                    ++j;
                    continue;
                }
                break;
            }
            if (j < input.size() && input[j] == '}') {
                out.append(session.GetVariableValueView(input.substr(i + 2U, j - (i + 2U))));
                i = j + 1U;
                continue;
            }
        }
        out.push_back(input[i]);
        ++i;
    }
    return out;
}

} // namespace ri::ui
