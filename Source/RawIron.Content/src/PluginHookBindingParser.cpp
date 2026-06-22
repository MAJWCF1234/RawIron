#include "RawIron/Content/PluginHookBindingParser.h"

#include <fstream>
#include <string>

namespace ri::content {
namespace {

std::string TrimText(const std::string& text) {
    const std::string whitespace = " \t\r\n";
    const std::size_t begin = text.find_first_not_of(whitespace);
    if (begin == std::string::npos) {
        return {};
    }
    const std::size_t end = text.find_last_not_of(whitespace);
    return text.substr(begin, end - begin + 1U);
}

int ParsePriorityText(const std::string& text) {
    try {
        return std::stoi(TrimText(text));
    } catch (...) {
        return 0;
    }
}

void AddAliasBinding(std::vector<PluginHookBinding>& bindings, const std::string& key, const std::string& pluginId) {
    if (key == "startup" || key == "on_startup") {
        bindings.push_back(PluginHookBinding{.hookPhase = "startup", .pluginId = pluginId, .eventName = "bootstrap"});
        return;
    }
    if (key == "runtime" || key == "on_runtime") {
        bindings.push_back(PluginHookBinding{.hookPhase = "runtime", .pluginId = pluginId, .eventName = "frame_sample"});
        return;
    }
    bindings.push_back(PluginHookBinding{.hookPhase = key, .pluginId = pluginId, .eventName = "default"});
}

} // namespace

std::vector<PluginHookBinding> LoadPluginHookBindings(const std::filesystem::path& path) {
    std::vector<PluginHookBinding> bindings{};
    std::ifstream stream(path);
    if (!stream.is_open()) {
        return bindings;
    }

    std::string line{};
    while (std::getline(stream, line)) {
        line = TrimText(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }

        const std::size_t equals = line.find('=');
        if (equals != std::string::npos) {
            const std::string key = TrimText(line.substr(0, equals));
            const std::string pluginId = TrimText(line.substr(equals + 1U));
            if (!key.empty() && !pluginId.empty()) {
                AddAliasBinding(bindings, key, pluginId);
            }
            continue;
        }

        const std::size_t first = line.find(',');
        const std::size_t second = first == std::string::npos ? std::string::npos : line.find(',', first + 1U);
        const std::size_t third = second == std::string::npos ? std::string::npos : line.find(',', second + 1U);
        if (first == std::string::npos || second == std::string::npos || third == std::string::npos) {
            continue;
        }

        const std::string phase = TrimText(line.substr(0, first));
        if (phase == "hook" || phase == "phase") {
            continue;
        }
        bindings.push_back(PluginHookBinding{
            .hookPhase = phase,
            .pluginId = TrimText(line.substr(first + 1U, second - first - 1U)),
            .eventName = TrimText(line.substr(second + 1U, third - second - 1U)),
            .priority = ParsePriorityText(line.substr(third + 1U)),
        });
    }
    return bindings;
}

} // namespace ri::content
