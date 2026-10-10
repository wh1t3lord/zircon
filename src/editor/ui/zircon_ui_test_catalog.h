#pragma once

// task Z17: the UI-test catalog — the constexpr step tables + their
// probe functions (see zircon_ui_test_harness.h for the harness design).
// Declared here so the harness resolves names without knowing the
// tables; everything lives in the .cpp (the probes touch the world,
// the history, the config, the renderer — engine internals)

#include "zircon_ui_test_harness.h"
