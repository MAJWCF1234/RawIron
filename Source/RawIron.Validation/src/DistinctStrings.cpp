#include "RawIron/DataSchema/DistinctStrings.h"

#include <string>
#include <unordered_map>

namespace ri::validate {

ValidationReport ValidateDistinctStrings(std::string_view pathPrefix, const std::vector<std::string_view>& values) {
    ValidationReport report;
    std::unordered_map<std::string, std::size_t> firstIndex;
    firstIndex.reserve(values.size());
    for (std::size_t i = 0; i < values.size(); ++i) {
        const std::string key(values[i]);
        const auto it = firstIndex.find(key);
        if (it != firstIndex.end()) {
            std::string path;
            if (pathPrefix.empty()) {
                path = '/' + std::to_string(i);
            } else {
                path.assign(pathPrefix);
                if (path.back() != '/') {
                    path.push_back('/');
                }
                path += std::to_string(i);
            }
            report.Add(IssueCode::ConstraintViolation,
                       std::move(path),
                       "duplicate string (first seen at index " + std::to_string(it->second) + ')');
            return report;
        }
        firstIndex.emplace(std::move(key), i);
    }
    return report;
}

} // namespace ri::validate
