#ifndef TASK_H
#define TASK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t Tick;

/**
 * @brief 
 * \enum SchedulingAlgorithmType
 * \brief Enum for different type of scheduling algorithms
 */
typedef enum
{
    SCHED_ALGO_RM = 0,  /**< Rate Monotonic algorithm */
    SCHED_ALGO_DM,      /**< Deadline Monotonic algorithm */
    SCHED_ALGO_EDF,     /**< Earliest Deadline First algorithm*/
    SCHED_ALGO_LLF,     /**< Least Laxity First algorithm*/
    SCHED_ALGO_SRTF,
    SCHED_ALGO_COUNT    /**< Number of scheduling algorithms */
} SchedulingAlgoType;

/**
 * @brief 
 * \struct TaskConfig
 * \brief Structure representing static parameters of a task
 * 
 * This structure contains information about task static information
 * like its relative deadline, execution time, period and others.
 */
typedef struct
{
    uint8_t id;                 /**< \brief ID of task*/
    const char* name;           /**< \brief Name of task (used for debugging mostly) */
    Tick period;                /**< \brief Period of task */
    Tick relDeadline;           /**< \brief Task Relaive deadline */
    Tick wCet;                  /**< \brief Task execution time */
    Tick deadlineTolerance;     /**< \brief The amount of time that soft deadlien task can tolerate */
    uint8_t isNetwork;          /**< \brief Is task network related or not */
    uint8_t isHard;             /**< Is task hard deadline or soft */
} TaskConfig;

/**
 * @brief 
 * \struct TaskState
 * \brief Dynamic state of each task running in a period
 * 
 * This structure represents the dynamic information about a task running on current period
 */

typedef struct
{
    TaskConfig* taskCfg;            /**< \brief Pointer to task static data */
    void *osHandle;                 /**< \brief OS specific handle for the task */
    
    uint8_t   ready;                /**< \brief Is task ready to be run */
    Tick next_release;              /**< \brief Task next release time*/
    Tick abs_deadline;              /**< \brief Task absolute deadline */
    Tick remaining;                 /**< \brief Task remaining running time */

    Tick last_release;              /**< \brief Task last release time */
    Tick last_start;                /**< \brief Task last start time */
    Tick last_finish;               /**< \brief Task last finish time */

    uint32_t  jobs_released;        /**< \brief number of jobs released from this task*/
    uint32_t  jobs_completed;       /**< \brief number of jobs completed from this task*/
    uint32_t  jobs_missed;          /**< \brief number of jobs missed from this task*/
    double    sum_response_time;    /**< \brief sum of response times from this task*/
} TaskState;

/**
 * @brief Initialize the task states from tasks static data
 * 
 * @param tasks         The static tasks array
 * @param cfgs          The tasks state array, each for a static task in previous parameter
 * @param numTasks      Number of tasks to initialize
 * @param startTime     The start time
 */
void InitializeTasksStates(const TaskConfig *cfgs,
                           TaskState        *states,
                           uint8_t           numTasks,
                           Tick              startTime);


/**
 * @brief Mark that we started executing this job
 * 
 * @param taskState the task that we are going to run
 * @param now       Current time
 */
void OnJobStart(TaskState *taskState, Tick now);

/**
 * @brief Update the remaining execution time, mark completion and other dynamic stats
 * 
 * @param taskState The task state that has just been running
 * @param now       Current time
 * @param delta     Running duration in ticks
 */
void OnJobExecute(TaskState *taskState, Tick now, Tick delta);

#ifdef __cplusplus
}
#endif

#endif /* TASK_H */