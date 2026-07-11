#include "RawIron/Events/EventEngine.h"

#include <cstdlib>
#include <limits>

int main() {
    ri::events::EventEngine engine{};
    if (!engine.SetWorldValue("score", 10.0) || !engine.AddWorldValue("score", 5.0)
        || engine.GetWorldValue("score") != 15.0) {
        return EXIT_FAILURE;
    }

    const double maximum = std::numeric_limits<double>::max();
    if (!engine.SetWorldValue("energy", maximum)) {
        return EXIT_FAILURE;
    }
    if (engine.AddWorldValue("energy", maximum) || engine.GetWorldValue("energy") != maximum) {
        return EXIT_FAILURE;
    }
    if (engine.SetWorldValue("invalid", std::numeric_limits<double>::infinity())) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
