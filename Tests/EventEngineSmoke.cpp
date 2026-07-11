#include "RawIron/Events/EventEngine.h"

#include <cstdlib>
#include <limits>
#include <vector>

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

    ri::events::EventConditions invalidCondition;
    invalidCondition.valuesAtLeast["score"] = std::numeric_limits<double>::quiet_NaN();
    if (engine.EvaluateConditions(invalidCondition)) {
        return EXIT_FAILURE;
    }

    ri::events::EventAction delayedAction;
    delayedAction.type = "delay";
    delayedAction.delayMs = std::numeric_limits<double>::infinity();
    ri::events::EventAction setValueAction;
    setValueAction.type = "set_value";
    setValueAction.key = "fired";
    setValueAction.value = 1.0;
    delayedAction.actions.push_back(setValueAction);
    engine.RunActions({delayedAction}, {}, {}, std::numeric_limits<double>::quiet_NaN());
    if (engine.ScheduledTimerCount() != 1) {
        return EXIT_FAILURE;
    }
    engine.Tick(0.0, {});
    if (engine.GetWorldValue("fired") != 1.0) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
