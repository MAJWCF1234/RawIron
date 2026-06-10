#include "RawIron/Logic/LogicKitPortBridge.h"

#include <cctype>
#include <string>
#include <unordered_map>

namespace ri::logic {
namespace {

[[nodiscard]] std::string NormalizePortKey(std::string_view portName) {
    std::string out;
    out.reserve(portName.size());
    for (char ch : portName) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return out;
}

[[nodiscard]] const std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& InputBridgeTable() {
    static const std::unordered_map<std::string, std::unordered_map<std::string, std::string>> kTable{
        {"mem_flipflop",
         {
             {"clock", "Clock"},
             {"data", "Data"},
         }},
        {"mem_register",
         {
             {"data", "Sig"},
             {"write", "Cap"},
             {"read", "Hold"},
         }},
        {"mem_variable",
         {
             {"set", "SetValue"},
             {"get", "Trigger"},
         }},
        {"mem_ram_array",
         {
             {"data", "Sig"},
             {"write", "Cap"},
             {"read", "Hold"},
         }},
    };
    return kTable;
}

[[nodiscard]] const std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& OutputBridgeTable() {
    static const std::unordered_map<std::string, std::unordered_map<std::string, std::string>> kTable{
        {"mem_flipflop",
         {
             {"q", "OnTrue"},
             {"not_q", "OnFalse"},
         }},
        {"mem_register",
         {
             {"value", "Out"},
         }},
        {"mem_variable",
         {
             {"value", "OnChanged"},
         }},
        {"mem_ram_array",
         {
             {"value", "Out"},
         }},
    };
    return kTable;
}

[[nodiscard]] std::optional<std::string> LookupBridge(
    const std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& table,
    const std::string_view kitId,
    const std::string_view kitPortName) {
    const auto kitIt = table.find(NormalizePortKey(kitId));
    if (kitIt == table.end()) {
        return std::nullopt;
    }
    const auto portIt = kitIt->second.find(NormalizePortKey(kitPortName));
    if (portIt == kitIt->second.end()) {
        return std::nullopt;
    }
    return portIt->second;
}

} // namespace

std::optional<std::string> MapKitLogicInputToRuntime(const std::string_view kitId,
                                                     const std::string_view kitPortName) {
    return LookupBridge(InputBridgeTable(), kitId, kitPortName);
}

std::optional<std::string> MapKitLogicOutputToRuntime(const std::string_view kitId,
                                                      const std::string_view kitPortName) {
    return LookupBridge(OutputBridgeTable(), kitId, kitPortName);
}

} // namespace ri::logic
