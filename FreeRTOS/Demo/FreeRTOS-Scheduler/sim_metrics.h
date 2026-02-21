#pragma once
#include "FreeRTOS.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "task.h"

// ----------------- Global metrics -----------------
typedef struct {
    Tick sim_ticks;

    Tick cpu_busy_ticks;
    Tick cpu_idle_ticks;

    uint64_t energy_run;
    uint64_t energy_idle;
    uint64_t energy_sleep;

    uint32_t total_preemptions;
    uint32_t total_dispatches;

    uint64_t net_tx_bytes;
    uint64_t net_rx_bytes;
} SimMetrics;