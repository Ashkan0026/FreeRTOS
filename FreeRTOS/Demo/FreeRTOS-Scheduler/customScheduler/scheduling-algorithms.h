#ifndef SCHED_ALGOS_H
#define SCHED_ALGOS_H

#include "task.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief This function implements the Rate monotonic scheduler algorithm, and return the task with highest priority
 * 
 * @param taskStates This pointer contains an array of jobs
 * @param numTasks Number of tasks
 * @return int Returns index of highest priority task, If there is none, returns -1
 */
int rmImplementation(const TaskState *taskStates, uint8_t numTasks);

/**
 * @brief This function implements the deadline monotonic algorithm, which select task with lowest relative deadline
 * 
 * @param taskStates An pointer to array of jobs
 * @param numTasks Number of tasks
 * @return int Returns the index of task with highest priority
 */
int dmImplementation(const TaskState *taskStates, uint8_t numTasks);

/**
 * @brief This function implements the deadline monotonic algorithm, which select task with lowest absolute deadline
 * 
 * @param taskStates An pointer to array of jobs
 * @param numTasks Number of tasks
 * @return int Returns the index of task with highest priority
 */
int edfImplementation(const TaskState *taskStates, uint8_t numTasks);

/**
 * @brief This function implements the least laxity scheduling, laxity = deadline - (now + remaining)
 * 
 * @param taskStates An pointer to array of jobs
 * @param numTasks Number of tasks
 * @param now Current Time
 * @return int Selects the task with highest priority
 */
int llfImplementation(const TaskState *taskStates, uint8_t numTasks, Tick now);

/**
 * @brief This function implement the shortest remaining time first algorithm, which chooses the task with the shortest remaining time
 * 
 * @param taskStates 
 * @param numTasks 
 * @return int 
 */
int srtImplementation(const TaskState *taskStates, uint8_t numTasks);

/**
 * @brief This function implements the an EDF variant that biases towards cheaper tasks when CPU utilization is high
 * 
 * @param taskStates 
 * @param numTasks 
 * @param now 
 * @return int 
 */
// int edfEnergyAwareImplementation(const TaskState *taskStates, uint8_t numTasks, Tick now);


#ifdef __cplusplus
}
#endif

#endif