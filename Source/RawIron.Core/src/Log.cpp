#include "RawIron/Core/Log.h"

#include <iostream>
#include <string>

namespace ri::core {

void LogInfo(std::string_view message) {
    std::cout << message << '\n';
}

void LogSection(std::string_view title) {
    std::cout << "\n[" << title << "]\n";
}

} // namespace ri::core
