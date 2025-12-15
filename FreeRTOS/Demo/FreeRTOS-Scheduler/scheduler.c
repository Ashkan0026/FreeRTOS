#include "customScheduler/scheduler.h"
#include "customScheduler/scheduling-algorithms.h"

void ReleaseTasks(TaskState *tasksState, uint8_t numTasks, Tick now)
{
    for (uint8_t i = 0; i < numTasks; i++)
    {
        TaskState *t = &tasksState[i];

        while ((now >= t->next_release) && (t->remaining == 0))
        {
            t->ready        = 1;
            t->last_release = t->next_release;
            t->abs_deadline = t->last_release + t->taskCfg->relDeadline;
            t->remaining    = t->taskCfg->wCet;
            t->jobs_released++;

            t->next_release += t->taskCfg->period;
        }
    }
}

int SelectJobs(const TaskState *taskStates, uint8_t numTasks, Tick now, SchedulingAlgoType algo)
{
    switch (algo)
    {
    case SCHED_ALGO_RM:
        return rmImplementation(taskStates, numTasks);
    case SCHED_ALGO_DM:
        return dmImplementation(taskStates, numTasks);
    case SCHED_ALGO_EDF:
        return edfImplementation(taskStates, numTasks);
    case SCHED_ALGO_LLF:
        return llfImplementation(taskStates, numTasks, now);
    case SCHED_ALGO_SRTF:
        return srtImplementation(taskStates, numTasks);
    default:
        return -1;
    }
}