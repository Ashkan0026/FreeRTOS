#pragma once
#include "sim_metrics.h"
#include "task"

typedef struct FuzzyScheduler FuzzyScheduler;

#ifdef __cplusplus
extern "C" {
#endif

    /**
     * @brief Initialize the fuzzy scheduler
     *
     * @param metrics Pointer to system metrics structure
     * @param selection_interval Ticks between algorithm reevaluation
     * @return Pointer to initialized fuzzy scheduler context (NULL on failure)
     */
    FuzzyScheduler* FuzzyScheduler_Init(SimMetrics* metrics, uint32_t selection_interval);

    /**
     * @brief Destroy fuzzy scheduler and free resources
     *
     * @param fs Pointer to fuzzy scheduler context
     */
    void FuzzyScheduler_Destroy(FuzzyScheduler* fs);

    /**
     * @brief Update scheduler state and get current recommended algorithm
     * Call this periodically (e.g., every tick)
     *
     * @param fs Pointer to fuzzy scheduler context
     * @return Current recommended scheduling algorithm
     */
    SchedulingAlgoType FuzzyScheduler_Update(FuzzyScheduler* fs);

    /**
     * @brief Force algorithm selection based on current metrics
     * Useful for immediate reevaluation
     *
     * @param fs Pointer to fuzzy scheduler context
     * @return Selected scheduling algorithm
     */
    SchedulingAlgoType FuzzyScheduler_SelectAlgorithm(FuzzyScheduler* fs);

    /**
     * @brief Get human-readable algorithm name
     *
     * @param algo Scheduling algorithm type
     * @return String representation of algorithm name
     */
    const char* FuzzyScheduler_GetAlgorithmName(SchedulingAlgoType algo);

    /**
     * @brief Reset fuzzy scheduler state
     *
     * @param fs Pointer to fuzzy scheduler context
     */
    void FuzzyScheduler_Reset(FuzzyScheduler* fs);

    /**
     * @brief Get detailed fuzzy output strengths for debugging
     *
     * @param fs Pointer to fuzzy scheduler context
     * @param strengths Array of size SCHED_ALGO_COUNT to store output strengths
     */
    void FuzzyScheduler_GetDetailedOutputs(FuzzyScheduler* fs, float strengths[SCHED_ALGO_COUNT]);

    /**
     * @brief Get current fuzzy input values for debugging
     *
     * @param fs Pointer to fuzzy scheduler context
     * @param cpu_util CPU utilization output (0-1)
     * @param deadline_urgency Deadline urgency output (0-1)
     * @param periodicity Task periodicity output (0-1)
     * @param variability Workload variability output (0-1)
     * @param criticality Criticality mix output (0-1)
     * @param energy_eff Energy efficiency output (0-1)
     */
    void FuzzyScheduler_GetInputValues(FuzzyScheduler* fs,
        float* cpu_util,
        float* deadline_urgency,
        float* periodicity,
        float* variability,
        float* criticality,
        float* energy_eff);

    /**
     * @brief Set custom rule weight (for runtime tuning)
     *
     * @param fs Pointer to fuzzy scheduler context
     * @param rule_index Index of rule to modify
     * @param weight New weight value (0.0 to 1.0)
     * @return int 0 on success, -1 on error
     */
    int FuzzyScheduler_SetRuleWeight(FuzzyScheduler* fs, int rule_index, float weight);

#ifdef __cplusplus
}
#endif
