#pragma once

#include "FreeRTOS.h"
#include "task.h"

// Simulation length (ticks)
#define SIM_DURATION_TICKS   ((Tick)100)

// Log each tick (noisy)
#define SIM_LOG_EACH_TICK    1

// ---- Energy model (units: microjoules per tick, or arbitrary "energy units") ----
// Use simple constants. Replace with real measured numbers later.
#define E_RUN_PER_TICK       50u
#define E_IDLE_PER_TICK      10u
#define E_SLEEP_PER_TICK     1u   // only used if you enable tickless + sleep hooks

// ---- Network model ----
// If you don't have real stack counters, simulate bytes per CPU tick for network tasks.
#define NET_TX_BYTES_PER_EXEC_TICK   120u
#define NET_RX_BYTES_PER_EXEC_TICK   80u