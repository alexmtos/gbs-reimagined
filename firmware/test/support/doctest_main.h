#pragma once

// Entry point shared by every test program. PlatformIO's doctest runner needs a custom main()
// that reports successful cases and leaves the exit code to the runner; include this header from
// exactly one file per test directory, before any other doctest include.
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest.h>

int main(int argc, char** argv) {
    doctest::Context context;
    context.setOption("success", true);
    context.setOption("no-exitcode", true);
    context.applyCommandLine(argc, argv);
    return context.run();
}
