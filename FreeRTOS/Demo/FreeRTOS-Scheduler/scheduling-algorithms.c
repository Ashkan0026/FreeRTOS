#include "customScheduler/scheduling-algorithms.h"

int rmImplementation(const TaskState *taskStates, uint8_t numTasks)
{
    int bestIndex = -1;
    Tick bestKey = UINT32_MAX;

    for (uint8_t i = 0; i < numTasks; i++)
    {
        const TaskState *t = &taskStates[i];
        if (!t->ready || t->remaining == 0)
            continue;
        
        Tick key = t->taskCfg->period;
        if (
            (bestIndex < 0) || 
            (key < bestKey) || 
            (key == bestKey && t->taskCfg->id < taskStates[bestIndex].taskCfg->id)
        )
        {
            bestIndex = (int) i;
            bestKey = key;
        }
    }
    
    return bestIndex;
}

int dmImplementation(const TaskState *taskStates, uint8_t numTasks)
{
    int      best_idx = -1;
    Tick best_key = UINT32_MAX;

    for (uint8_t i = 0; i < numTasks; ++i)
    {
        const TaskState *t = &taskStates[i];
        if (!t->ready || t->remaining == 0)
            continue;

        Tick key = t->taskCfg->relDeadline;
        if ((best_idx < 0) || (key < best_key) ||
            (key == best_key && t->taskCfg->id < taskStates[best_idx].taskCfg->id))
        {
            best_idx = (int)i;
            best_key = key;
        }
    }
    return best_idx;
}

int edfImplementation(const TaskState *taskStates, uint8_t numTasks)
{
    int      best_idx = -1;
    Tick best_key = UINT32_MAX;

    for (uint8_t i = 0; i < numTasks; ++i)
    {
        const TaskState *t = &taskStates[i];
        if (!t->ready || t->remaining == 0)
            continue;

        Tick key = t->abs_deadline;
        if ((best_idx < 0) || (key < best_key) ||
            (key == best_key && t->taskCfg->id < taskStates[best_idx].taskCfg->id))
        {
            best_idx = (int)i;
            best_key = key;
        }
    }
    return best_idx;
}

int llfImplementation(const TaskState *taskStates, uint8_t numTasks, Tick now)
{
    int      best_idx = -1;
    int32_t  best_lax = INT32_MAX;

    for (uint8_t i = 0; i < numTasks; ++i)
    {
        const TaskState *t = &taskStates[i];
        if (!t->ready || t->remaining == 0)
            continue;

        /* laxity = deadline - (now + remaining) */
        int32_t lax = (int32_t)t->abs_deadline -
                      (int32_t)(now + t->remaining);

        if ((best_idx < 0) || (lax < best_lax) ||
            (lax == best_lax && t->taskCfg->id < taskStates[best_idx].taskCfg->id))
        {
            best_idx = (int)i;
            best_lax = lax;
        }
    }
    return best_idx;
}

int srtImplementation(const TaskState *taskStates, uint8_t numTasks)
{
    int best = -1;
    Tick bestRem = (Tick)-1;

    for (uint8_t i = 0; i < numTasks; ++i)
    {
        const TaskState *ts = &taskStates[i];
        if (!ts->ready || ts->remaining == 0)
            continue;

        if (best < 0 || ts->remaining < bestRem)
        {
            best = i;
            bestRem = ts->remaining;
        }
    }
    return best;
}

