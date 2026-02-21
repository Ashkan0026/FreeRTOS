/**
 * FreeRTOS “simulation-style” dispatcher + worker model
 * FIXED for common FreeRTOS assert/crash causes:
 *  - checks xTaskCreate() return values + NULL handles
 *  - guards invalid scheduler selection (sel < 0 or sel >= NUM_TASKS)
 *  - guards gDispatcherHandle before notifying
 *  - optional: reduces per-tick logging pressure (stack/heap)
 *
 * Drop-in replacement for your pasted file.
 */

#include "../FreeRTOS-Scheduler/customScheduler/sim_config.h"
#include "FreeRTOS.h"
#include "task.h"
#include "../FreeRTOS-Scheduler/customScheduler/task.h"
#include "../FreeRTOS-Scheduler/sim_metrics.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

 // ----------------- Task set (edit this) -----------------
static TaskConfig gTaskCfg[] = {
    {.id = 0, .name = "T0", .period = 10, .relDeadline = 10, .wCet = 2, .deadlineTolerance = 0, .isNetwork = 0, .isHard = 1 },
    {.id = 1, .name = "T1", .period = 15, .relDeadline = 15, .wCet = 3, .deadlineTolerance = 0, .isNetwork = 1, .isHard = 0 },
    {.id = 2, .name = "T2", .period = 20, .relDeadline = 12, .wCet = 4, .deadlineTolerance = 0, .isNetwork = 0, .isHard = 1 },
    // {.id = 3, .name = "T3", .period = 15, .relDeadline = 10, .wCet = 5, .deadlineTolerance = 0, .isNetwork = 0, .isHard = 1}
};
#define NUM_TASKS   ((uint8_t)(sizeof(gTaskCfg)/sizeof(gTaskCfg[0])))

// ----------------- Runtime state -----------------
static TaskState    gState[NUM_TASKS];
static TaskHandle_t gWorkerHandle[NUM_TASKS];
static TaskHandle_t gDispatcherHandle = NULL;
static SchedulingAlgoType gPolicy = SCHED_ALGO_EDF;

static SimMetrics gM = { 0 };

// -------- Scheduling algorithms (you already implemented) --------
extern int rmImplementation(const TaskState* taskStates, uint8_t numTasks);
extern int dmImplementation(const TaskState* taskStates, uint8_t numTasks);
extern int edfImplementation(const TaskState* taskStates, uint8_t numTasks);
extern int llfImplementation(const TaskState* taskStates, uint8_t numTasks, Tick now);
extern int srtImplementation(const TaskState* taskStates, uint8_t numTasks);

static int pickNext(const TaskState* ts, uint8_t n, Tick now)
{
    switch (gPolicy) {
    case SCHED_ALGO_RM:   return rmImplementation(ts, n);
    case SCHED_ALGO_DM:   return dmImplementation(ts, n);
    case SCHED_ALGO_EDF:  return edfImplementation(ts, n);
    case SCHED_ALGO_LLF:  return llfImplementation(ts, n, now);
    case SCHED_ALGO_SRTF: return srtImplementation(ts, n);
    default:              return -1;
    }
}

static void releaseJobsIfNeeded(Tick now)
{
    for (uint8_t i = 0; i < NUM_TASKS; i++) {
        TaskState* s = &gState[i];
        TaskConfig* c = s->taskCfg;

        if (now >= s->next_release) {
            // If previous job not finished by next release => miss + overwrite
            if (s->ready && s->remaining > 0) {
                s->jobs_missed++;
                s->deadline_violations++;
            }

            s->ready = 1;
            s->remaining = c->wCet;
            s->last_release = now;
            s->abs_deadline = now + c->relDeadline;
            s->next_release = now + c->period;
            s->jobs_released++;
        }
    }
}

static void checkDeadlineMisses(Tick now)
{
    for (uint8_t i = 0; i < NUM_TASKS; i++) {
        TaskState* s = &gState[i];
        TaskConfig* c = s->taskCfg;

        if (!s->ready || s->remaining == 0) continue;

        Tick missTime = c->isHard ? s->abs_deadline : (s->abs_deadline + c->deadlineTolerance);

        if (now > missTime) {
            s->jobs_missed++;
            s->deadline_violations++;

            if (c->isHard) {
                // Drop hard job once it misses
                s->ready = 0;
                s->remaining = 0;
            }
        }
    }
}

// ----------------- Worker task -----------------
static void workerTask(void* arg)
{
    uint32_t idx = (uint32_t)(uintptr_t)arg;
    configASSERT(idx < NUM_TASKS);

    TaskState* s = &gState[idx];

    for (;;) {
        // Wait until dispatcher gives us one quantum
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // Dispatcher handle must be valid before we respond
        configASSERT(gDispatcherHandle != NULL);

        Tick now = xTaskGetTickCount();

        // Consume one tick if runnable
        if (s->ready && s->remaining > 0) {
            if (s->remaining == s->taskCfg->wCet) {
                s->last_start = now;
            }

            s->dispatches++;
            s->exec_ticks++;

            if (s->taskCfg->isNetwork) {
                s->net_tx_bytes += NET_TX_BYTES_PER_EXEC_TICK;
                s->net_rx_bytes += NET_RX_BYTES_PER_EXEC_TICK;
            }

            s->remaining--;

            if (s->remaining == 0) {
                s->ready = 0;
                s->last_finish = now + 1;

                s->jobs_completed++;
                Tick resp = s->last_finish - s->last_release;
                s->sum_response_time += (double)resp;
            }
        }

        // Notify dispatcher we finished the quantum
        xTaskNotifyGive(gDispatcherHandle);
    }
}

// ----------------- Summary printing -----------------
static void printSummary(void)
{
    printf("\n=== Simulation Summary ===\n");
    printf("Policy=%d  Duration=%lu ticks\n", (int)gPolicy, (unsigned long)gM.sim_ticks);

    Tick total = gM.cpu_busy_ticks + gM.cpu_idle_ticks;
    double cpuLoad = (total > 0) ? ((double)gM.cpu_busy_ticks / (double)total) * 100.0 : 0.0;

    printf("\n--- CPU ---\n");
    printf("Busy=%lu  Idle=%lu  CPU Load=%.2f%%\n",
        (unsigned long)gM.cpu_busy_ticks,
        (unsigned long)gM.cpu_idle_ticks,
        cpuLoad);

    printf("\n--- Energy model ---\n");
    printf("E_run=%llu  E_idle=%llu  E_sleep=%llu  E_total=%llu\n",
        (unsigned long long)gM.energy_run,
        (unsigned long long)gM.energy_idle,
        (unsigned long long)gM.energy_sleep,
        (unsigned long long)(gM.energy_run + gM.energy_idle + gM.energy_sleep));

    printf("\n--- Dispatching / preemptions ---\n");
    printf("Total dispatches=%lu  Total preemptions=%lu\n",
        (unsigned long)gM.total_dispatches,
        (unsigned long)gM.total_preemptions);

    printf("\n--- Network ---\n");
    printf("TX=%llu bytes  RX=%llu bytes\n",
        (unsigned long long)gM.net_tx_bytes,
        (unsigned long long)gM.net_rx_bytes);

    printf("\n--- Per-task ---\n");
    for (uint8_t i = 0; i < NUM_TASKS; i++) {
        TaskState* s = &gState[i];
        TaskConfig* c = s->taskCfg;

        double avgResp = (s->jobs_completed > 0) ? (s->sum_response_time / (double)s->jobs_completed) : 0.0;
        double taskCpu = (gM.sim_ticks > 0) ? ((double)s->exec_ticks / (double)gM.sim_ticks) * 100.0 : 0.0;

        printf("%s: rel=%lu comp=%lu miss=%lu viol=%lu exec=%lu (%.2f%% CPU) disp=%lu preempt=%lu avgResp=%.2f  TX=%lu RX=%lu\n",
            c->name,
            (unsigned long)s->jobs_released,
            (unsigned long)s->jobs_completed,
            (unsigned long)s->jobs_missed,
            (unsigned long)s->deadline_violations,
            (unsigned long)s->exec_ticks,
            taskCpu,
            (unsigned long)s->dispatches,
            (unsigned long)s->preemptions,
            avgResp,
            (unsigned long)s->net_tx_bytes,
            (unsigned long)s->net_rx_bytes);
    }

#if (INCLUDE_uxTaskGetStackHighWaterMark == 1)
    printf("\n--- Stack high-water marks (words) ---\n");
    for (uint8_t i = 0; i < NUM_TASKS; i++) {
        if (gWorkerHandle[i]) {
            UBaseType_t hw = uxTaskGetStackHighWaterMark(gWorkerHandle[i]);
            printf("W%u: %lu\n", (unsigned)i, (unsigned long)hw);
        }
    }
    if (gDispatcherHandle) {
        UBaseType_t hw = uxTaskGetStackHighWaterMark(gDispatcherHandle);
        printf("DISP: %lu\n", (unsigned long)hw);
    }
#endif
}

// ----------------- Dispatcher -----------------
static void dispatcherTask(void* arg)
{
    (void)arg;
    gDispatcherHandle = xTaskGetCurrentTaskHandle();
    configASSERT(gDispatcherHandle != NULL);

    memset(&gM, 0, sizeof(gM));

    // All tasks release at "now"
    Tick now = xTaskGetTickCount();
    for (uint8_t i = 0; i < NUM_TASKS; i++) {
        gState[i].next_release = now;
    }

    int lastSelected = -1;

    for (Tick step = 0; step < SIM_DURATION_TICKS; step++) {
        now = xTaskGetTickCount();
        gM.sim_ticks++;

        releaseJobsIfNeeded(now);
        checkDeadlineMisses(now);

        int sel = pickNext(gState, NUM_TASKS, now);

        // Preemption accounting
        if (lastSelected >= 0) {
            TaskState* prev = &gState[lastSelected];
            if (prev->remaining > 0 && prev->ready) {
                if (sel != lastSelected) {
                    prev->preemptions++;
                    gM.total_preemptions++;
                }
            }
        }

        if (sel >= 0) {
            // Guard selection validity (prevents out-of-bounds and NULL handle notify)
            configASSERT(sel < (int)NUM_TASKS);
            configASSERT(gWorkerHandle[sel] != NULL);

            gM.cpu_busy_ticks++;
            gM.energy_run += E_RUN_PER_TICK;
            gM.total_dispatches++;

            // Dispatch one quantum
            xTaskNotifyGive(gWorkerHandle[sel]);
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        }
        else {
            gM.cpu_idle_ticks++;
            gM.energy_idle += E_IDLE_PER_TICK;
        }

        // Update global net totals (cheap for small NUM_TASKS)
        uint64_t tx = 0, rx = 0;
        for (uint8_t i = 0; i < NUM_TASKS; i++) {
            tx += gState[i].net_tx_bytes;
            rx += gState[i].net_rx_bytes;
        }
        gM.net_tx_bytes = tx;
        gM.net_rx_bytes = rx;

#if SIM_LOG_EACH_TICK
        // Warning: per-tick printf can blow stack/heap on embedded targets.
        if (sel >= 0) {
            printf("[t=%lu] RUN %s rem=%lu ddl=%lu  CPUload=%.0f%%  TX=%llu RX=%llu\n",
                (unsigned long)now,
                gState[sel].taskCfg->name,
                (unsigned long)gState[sel].remaining,
                (unsigned long)gState[sel].abs_deadline,
                (gM.sim_ticks > 0) ? (100.0 * (double)gM.cpu_busy_ticks / (double)gM.sim_ticks) : 0.0,
                (unsigned long long)gM.net_tx_bytes,
                (unsigned long long)gM.net_rx_bytes);
        }
        else {
            printf("[t=%lu] IDLE  CPUload=%.0f%%  TX=%llu RX=%llu\n",
                (unsigned long)now,
                (gM.sim_ticks > 0) ? (100.0 * (double)gM.cpu_busy_ticks / (double)gM.sim_ticks) : 0.0,
                (unsigned long long)gM.net_tx_bytes,
                (unsigned long long)gM.net_rx_bytes);
        }
#endif

        lastSelected = sel;

        // Advance one tick
        vTaskDelay(1);
    }

    printSummary();
    for (int i = 0; i < NUM_TASKS; i++)
    {
        printf("Task %s missed deadlines are: %d\n", gState[i].taskCfg->name, gState[i].jobs_missed);
    }
    vTaskSuspend(NULL);
}

// ----------------- Public init -----------------
void simInit(SchedulingAlgoType policy)
{
    gPolicy = policy;

    memset(gState, 0, sizeof(gState));
    memset(gWorkerHandle, 0, sizeof(gWorkerHandle));

    for (uint8_t i = 0; i < NUM_TASKS; i++) {
        gState[i].taskCfg = &gTaskCfg[i];
        gState[i].ready = 0;
        gState[i].remaining = 0;
    }

    // Create workers (CHECK RETURN VALUES!)
    for (uint8_t i = 0; i < NUM_TASKS; i++) {
        char name[8];
        (void)snprintf(name, sizeof(name), "W%u", (unsigned)i);

        BaseType_t ok = xTaskCreate(
            workerTask,
            name,
            configMINIMAL_STACK_SIZE + 256,     // increase if you still print a lot
            (void*)(uintptr_t)i,
            tskIDLE_PRIORITY + 1,
            &gWorkerHandle[i]);

        configASSERT(ok == pdPASS);
        configASSERT(gWorkerHandle[i] != NULL);

        gState[i].osHandle = (void*)gWorkerHandle[i];
    }

    // Create dispatcher (CHECK RETURN VALUE!)
    {
        BaseType_t ok = xTaskCreate(
            dispatcherTask,
            "DISP",
            configMINIMAL_STACK_SIZE + 512,     // increase if you print summary via heavy printf
            NULL,
            tskIDLE_PRIORITY + 3,
            NULL);

        configASSERT(ok == pdPASS);
    }
}
