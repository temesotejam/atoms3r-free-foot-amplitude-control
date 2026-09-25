#pragma once
#include "experiment_status.h"
#include "roller485_manager.h"

// Encode one synchronous row from the controller's current status and the
// single Roller snapshot captured before now_us. No clocks, I/O, allocation,
// mutable global state, or delayed work here. RWLOG stays 258 bytes per row.
void encodeLogSample(LogSample& row, const ExperimentStatus& status,
    const RollerTelemetry& roller, uint32_t now_us, uint32_t t_test_ms,
    uint32_t run_start_us, const float* beta_ceilings);
