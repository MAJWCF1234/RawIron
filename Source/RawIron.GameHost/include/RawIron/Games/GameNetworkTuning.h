#pragma once

#include "RawIron/Content/ScriptScalars.h"
#include "RawIron/Core/CommandLine.h"
#include "RawIron/Runtime/RuntimeNetcode.h"

namespace ri::games {

/// Script defaults, then explicit CLI overrides. serverTickRate is the existing
/// net module's snapshot cadence; it does not change the host simulation clock.
void ApplyGameNetworkTuning(ri::runtime::AuthoritativeNetConfig& config,
    const ri::content::ScriptScalarMap& network, const ri::core::CommandLine& commandLine);

} // namespace ri::games
