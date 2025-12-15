#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "task.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Release new jobs for all periodic tasks whose release time has come.
 * 
 * @param taskState Task state array to investigate
 * @param numTasks  Number of tasks
 * @param now       Current time
 */
void ReleaseTasks(TaskState *taskState, uint8_t numTasks, Tick now);

/**
 * @brief Select index of ready task to run at time 'now' under algroithm alg
 * 
 * @param tasks     A list of task to investigate for any ready task
 * @param numTasks  Number of tasks
 * @param now       Current time
 * @param algo      The Algroithms type
 * @return int 
 */
int SelectJobs(const TaskState *taskStates, uint8_t numTasks, Tick now, SchedulingAlgoType algo);



int select_rm(const TaskState *tasks, uint8_t num_tasks);

#ifdef __cplusplus
}
#endif

#endif