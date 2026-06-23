# Lightweight test workspace: native `RawIron.Core.Tests` / `RawIron.EngineImport.Tests` trees were removed.
# Use game `*.riscript` flows and engine-level checks instead.
#
# When `RAWIRON_BUILD_TESTS` is ON, individual apps register small CTest entries (e.g.
# `RawIron.UiMenu.HeadlessParse` in `Apps/RawIron.UiMenu/CMakeLists.txt`).

add_executable(CommandLineSmoke CommandLineSmoke.cpp)
target_link_libraries(CommandLineSmoke PRIVATE RawIron::Core)
add_test(NAME RawIron.Core.CommandLineSmoke COMMAND CommandLineSmoke)
