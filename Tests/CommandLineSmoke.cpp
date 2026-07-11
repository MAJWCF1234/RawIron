#include "RawIron/Core/CommandLine.h"

#include <cstdlib>
#include <string>

namespace {

bool Expect(bool condition) {
    return condition;
}

} // namespace

int main() {
    char app[] = "rawiron-test";
    char output[] = "--output";
    char verbose[] = "--verbose";
    char width[] = "--width=1280";
    char depth[] = "--depth";
    char negative[] = "-1";
    char* argv[] = {app, output, verbose, width, depth, negative};

    ri::core::CommandLine commandLine(6, argv);

    if (!Expect(commandLine.HasFlag("--verbose"))) {
        return EXIT_FAILURE;
    }
    if (!Expect(!commandLine.GetValue("--output").has_value())) {
        return EXIT_FAILURE;
    }
    if (!Expect(commandLine.TryGetInt("--width").value_or(0) == 1280)) {
        return EXIT_FAILURE;
    }
    if (!Expect(commandLine.TryGetInt("--depth").value_or(0) == -1)) {
        return EXIT_FAILURE;
    }

    char separator[] = "--";
    char hiddenFlag[] = "--hidden";
    char hiddenValue[] = "--width=640";
    char* terminatedArgv[] = {app, verbose, separator, hiddenFlag, hiddenValue};
    ri::core::CommandLine terminated(5, terminatedArgv);
    if (!Expect(terminated.HasFlag("--verbose"))
        || !Expect(!terminated.HasFlag("--hidden"))
        || !Expect(!terminated.GetValue("--width").has_value())) {
        return EXIT_FAILURE;
    }

    char missing[] = "--output";
    char* separatorValueArgv[] = {app, missing, separator, hiddenFlag};
    ri::core::CommandLine separatorValue(4, separatorValueArgv);
    if (!Expect(!separatorValue.GetValue("--output").has_value())) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
