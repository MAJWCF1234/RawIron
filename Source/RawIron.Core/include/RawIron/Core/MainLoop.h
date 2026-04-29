#pragma once

namespace ri::core {

class CommandLine;
class Host;

struct MainLoopOptions {
    int maxFrames = 8;
    double fixedDeltaSeconds = 1.0 / 60.0;
    bool verboseFrames = false;
    bool paceToFixedDelta = true;
};

int RunMainLoop(Host& host, const CommandLine& commandLine, const MainLoopOptions& options);

} // namespace ri::core
