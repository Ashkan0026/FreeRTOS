#include "customScheduler/task.h"
#include <string.h>
#include <float.h>
#include <limits.h>

void InitializeTasksStates(const TaskConfig *cfgs,
                           TaskState        *states,
                           uint8_t           numTasks,
                           Tick              startTime)
{
    memset(states, 0, sizeof(TaskState) * numTasks);

    for (uint8_t i = 0; i < numTasks; ++i)
    {
        states[i].taskCfg      = (TaskConfig *)&cfgs[i];  // or cast away const
        states[i].osHandle     = NULL;
        states[i].ready        = 0;
        states[i].next_release = startTime;
        states[i].abs_deadline = 0;
        states[i].remaining    = 0;
    }
}

void OnJobStart(TaskState *taskState, Tick now)
{
    taskState->last_start = now;
}

void OnJobExecute(TaskState *taskState, Tick now, Tick delta)
{
    if (!taskState->ready || taskState->remaining == 0 || delta == 0)
    {
        return;
    }

    if (delta >= taskState->remaining)
    {
        delta = taskState->remaining;
    }

    taskState->remaining -= delta;

    if (taskState->remaining != 0)
    {
        return;
    }

    Tick finish = now + delta;
    taskState->last_finish = finish;
    taskState->jobs_completed++;
    Tick response = finish - taskState->last_release;
    taskState->sum_response_time += (double) response;

    if (finish > taskState->abs_deadline)
    {
        if (!taskState->taskCfg->isHard)
        {
            Tick amountOfDeadlineMiss = finish - taskState->abs_deadline;
            if (amountOfDeadlineMiss > taskState->taskCfg->deadlineTolerance)
            {
                taskState->jobs_missed++;    
            }
        }
        else
        {
            taskState->jobs_missed++;
        }
    }

    taskState->ready = 0;
}