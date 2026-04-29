#include "RawIron/Runtime/EntityIoTelemetry.h"

#include <string>
#include <utility>

namespace ri::runtime::entity_io {

void EmitEntityIo(RuntimeEventBus& bus, RuntimeEventFields fields) {
    RuntimeEvent event{};
    event.fields = std::move(fields);
    bus.Emit(std::string(kEventType), std::move(event));
}

} // namespace ri::runtime::entity_io
