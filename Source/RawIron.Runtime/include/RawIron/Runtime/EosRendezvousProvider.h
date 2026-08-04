#pragma once

#include "RawIron/Runtime/RendezvousProvider.h"

#include <memory>

namespace ri::runtime {

/// Creates the EOS-backed rendezvous provider. When RAWIRON_HAS_EOS is off the returned provider
/// fails `Startup()`, which is the caller's signal to fall back to the DirectToken provider.
[[nodiscard]] std::unique_ptr<IRendezvousProvider> CreateEosRendezvousProvider();

} // namespace ri::runtime
