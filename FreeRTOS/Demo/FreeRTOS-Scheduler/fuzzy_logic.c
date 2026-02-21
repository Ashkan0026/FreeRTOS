#include "fuzzy_logic.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdint.h>

// ==================== Internal Structures ====================

// Membership function types
typedef enum {
    MF_TRIANGULAR,
    MF_TRAPEZOIDAL,
    MF_GAUSSIAN
} MembershipType;

// Membership function structure
typedef struct {
    MembershipType type;
    float a, b, c, d;  // Parameters based on type
} MembershipFunc;

// Input membership definitions
typedef struct {
    char name[20];
    MembershipFunc low;
    MembershipFunc medium;
    MembershipFunc high;
} InputMembership;

// Fuzzy rule structure
typedef struct {
    uint8_t cpu_util_idx;        // Index for CPU utilization membership (0=low,1=med,2=high)
    uint8_t deadline_idx;         // Index for deadline urgency membership
    uint8_t periodicity_idx;      // Index for periodicity membership
    uint8_t variability_idx;      // Index for variability membership
    uint8_t criticality_idx;      // Index for criticality membership
    uint8_t energy_idx;           // Index for energy efficiency membership
    SchedulingAlgoType output;    // Recommended algorithm
    float weight;                 // Rule weight (0-1)
} FuzzyRule;

// Fuzzy input variables (internal)
typedef struct {
    float cpu_utilization;      // 0 to 1
    float deadline_urgency;      // 0 to 1
    float task_periodicity;      // 0 to 1
    float workload_variability;  // 0 to 1
    float criticality_mix;       // 0 to 1
    float energy_efficiency;     // 0 to 1
} FuzzyInputs;

// Main fuzzy controller structure (opaque)
struct FuzzyScheduler {
    InputMembership cpu_util_mf;
    InputMembership deadline_mf;
    InputMembership periodicity_mf;
    InputMembership variability_mf;
    InputMembership criticality_mf;
    InputMembership energy_mf;

    FuzzyRule rules[50];  // Rule base
    int rule_count;

    SimMetrics* metrics;   // Pointer to system metrics
    SchedulingAlgoType current_algo;
    Tick last_selection_tick;
    uint32_t selection_interval;

    // Cache for last computed values (for debugging)
    FuzzyInputs last_inputs;
    float last_strengths[SCHED_ALGO_COUNT];
};

// ==================== Static Function Prototypes ====================

static void init_membership_functions(FuzzyScheduler* fs);
static void init_fuzzy_rules(FuzzyScheduler* fs);
static float calculate_membership(float x, MembershipFunc mf);
static void fuzzify_inputs(FuzzyScheduler* fs, FuzzyInputs* inputs,
    float cpu_mem[3], float deadline_mem[3],
    float periodicity_mem[3], float variability_mem[3],
    float criticality_mem[3], float energy_mem[3]);
static void evaluate_rules(FuzzyScheduler* fs,
    float cpu_mem[3], float deadline_mem[3],
    float periodicity_mem[3], float variability_mem[3],
    float criticality_mem[3], float energy_mem[3],
    float rule_strengths[SCHED_ALGO_COUNT]);
static void extract_fuzzy_inputs(SimMetrics* metrics, FuzzyInputs* inputs);
static SchedulingAlgoType defuzzify(float* rule_strengths);

// ==================== Initialization Functions ====================

static void init_membership_functions(FuzzyScheduler* fs) {
    // CPU Utilization membership functions
    strcpy(fs->cpu_util_mf.name, "CPU Utilization");
    fs->cpu_util_mf.low = (MembershipFunc){ MF_TRAPEZOIDAL, 0.0, 0.0, 0.3, 0.4 };
    fs->cpu_util_mf.medium = (MembershipFunc){ MF_TRIANGULAR, 0.3, 0.5, 0.7, 0.0 };
    fs->cpu_util_mf.high = (MembershipFunc){ MF_TRAPEZOIDAL, 0.6, 0.7, 1.0, 1.0 };

    // Deadline Urgency membership functions
    strcpy(fs->deadline_mf.name, "Deadline Urgency");
    fs->deadline_mf.low = (MembershipFunc){ MF_TRAPEZOIDAL, 0.0, 0.0, 0.2, 0.3 };
    fs->deadline_mf.medium = (MembershipFunc){ MF_TRIANGULAR, 0.2, 0.4, 0.6, 0.0 };
    fs->deadline_mf.high = (MembershipFunc){ MF_TRAPEZOIDAL, 0.5, 0.7, 1.0, 1.0 };

    // Task Periodicity membership functions
    strcpy(fs->periodicity_mf.name, "Task Periodicity");
    fs->periodicity_mf.low = (MembershipFunc){ MF_TRAPEZOIDAL, 0.0, 0.0, 0.25, 0.35 };
    fs->periodicity_mf.medium = (MembershipFunc){ MF_TRIANGULAR, 0.25, 0.5, 0.75, 0.0 };
    fs->periodicity_mf.high = (MembershipFunc){ MF_TRAPEZOIDAL, 0.65, 0.75, 1.0, 1.0 };

    // Workload Variability membership functions
    strcpy(fs->variability_mf.name, "Workload Variability");
    fs->variability_mf.low = (MembershipFunc){ MF_TRAPEZOIDAL, 0.0, 0.0, 0.2, 0.3 };
    fs->variability_mf.medium = (MembershipFunc){ MF_TRIANGULAR, 0.2, 0.4, 0.6, 0.0 };
    fs->variability_mf.high = (MembershipFunc){ MF_TRAPEZOIDAL, 0.5, 0.7, 1.0, 1.0 };

    // Criticality Mix membership functions
    strcpy(fs->criticality_mf.name, "Criticality Mix");
    fs->criticality_mf.low = (MembershipFunc){ MF_TRAPEZOIDAL, 0.0, 0.0, 0.2, 0.3 };
    fs->criticality_mf.medium = (MembershipFunc){ MF_TRIANGULAR, 0.2, 0.4, 0.6, 0.0 };
    fs->criticality_mf.high = (MembershipFunc){ MF_TRAPEZOIDAL, 0.5, 0.7, 1.0, 1.0 };

    // Energy Efficiency membership functions
    strcpy(fs->energy_mf.name, "Energy Efficiency");
    fs->energy_mf.low = (MembershipFunc){ MF_TRAPEZOIDAL, 0.0, 0.0, 0.2, 0.3 };
    fs->energy_mf.medium = (MembershipFunc){ MF_TRIANGULAR, 0.2, 0.4, 0.6, 0.0 };
    fs->energy_mf.high = (MembershipFunc){ MF_TRAPEZOIDAL, 0.5, 0.7, 1.0, 1.0 };
}

static void init_fuzzy_rules(FuzzyScheduler* fs) {
    int rule_idx = 0;

    // RULES FOR RATE MONOTONIC (SCHED_ALGO_RM)
    // Rule 1: Low CPU, periodic tasks, low variability -> RM
    fs->rules[rule_idx++] = (FuzzyRule){ 0, 0, 2, 0, 0, 0, SCHED_ALGO_RM, 1.0 };

    // Rule 2: Low-medium CPU, highly periodic, low deadlines -> RM
    fs->rules[rule_idx++] = (FuzzyRule){ 1, 0, 2, 0, 0, 1, SCHED_ALGO_RM, 0.9 };

    // RULES FOR DEADLINE MONOTONIC (SCHED_ALGO_DM)
    // Rule 3: Mixed deadlines, periodic tasks -> DM
    fs->rules[rule_idx++] = (FuzzyRule){ 1, 2, 2, 0, 1, 0, SCHED_ALGO_DM, 1.0 };

    // Rule 4: Medium CPU, tight deadlines, periodic -> DM
    fs->rules[rule_idx++] = (FuzzyRule){ 1, 2, 2, 0, 0, 1, SCHED_ALGO_DM, 0.9 };

    // RULES FOR EARLIEST DEADLINE FIRST (SCHED_ALGO_EDF)
    // Rule 5: High CPU, tight deadlines -> EDF
    fs->rules[rule_idx++] = (FuzzyRule){ 2, 2, 1, 1, 1, 0, SCHED_ALGO_EDF, 1.0 };

    // Rule 6: Very high CPU, any deadlines -> EDF
    fs->rules[rule_idx++] = (FuzzyRule){ 2, 1, 1, 1, 0, 1, SCHED_ALGO_EDF, 0.9 };

    // Rule 7: High CPU, mixed criticality -> EDF
    fs->rules[rule_idx++] = (FuzzyRule){ 2, 1, 1, 1, 2, 0, SCHED_ALGO_EDF, 0.8 };

    // RULES FOR LEAST LAXITY TIME (SCHED_ALGO_LLF)
    // Rule 8: High variability, mixed criticality, tight deadlines -> LLF
    fs->rules[rule_idx++] = (FuzzyRule){ 1, 2, 0, 2, 2, 0, SCHED_ALGO_LLF, 1.0 };

    // Rule 9: High variability, any CPU, mixed criticality -> LLF
    fs->rules[rule_idx++] = (FuzzyRule){ 1, 1, 1, 2, 2, 1, SCHED_ALGO_LLF, 0.9 };

    // RULES FOR SHORTEST REMAINING TIME FIRST (SCHED_ALGO_SRTF)
    // Rule 10: High variability, low criticality, low energy concern -> SRTF
    fs->rules[rule_idx++] = (FuzzyRule){ 1, 0, 0, 2, 0, 0, SCHED_ALGO_SRTF, 1.0 };

    // Rule 11: Bursty workload, low criticality -> SRTF
    fs->rules[rule_idx++] = (FuzzyRule){ 1, 0, 0, 2, 0, 1, SCHED_ALGO_SRTF, 0.9 };

    // Rule 12: High CPU, high variability, low criticality -> SRTF
    fs->rules[rule_idx++] = (FuzzyRule){ 2, 0, 0, 2, 0, 0, SCHED_ALGO_SRTF, 0.8 };

    // ADDITIONAL MIXED RULES FOR ROBUSTNESS
    // Rule 13: Low energy, periodic -> RM (energy saving)
    fs->rules[rule_idx++] = (FuzzyRule){ 0, 0, 2, 0, 0, 2, SCHED_ALGO_RM, 0.7 };

    // Rule 14: High energy efficiency needed, low CPU -> RM
    fs->rules[rule_idx++] = (FuzzyRule){ 0, 0, 1, 0, 0, 2, SCHED_ALGO_RM, 0.6 };

    // Rule 15: Medium all around -> EDF (good general purpose)
    fs->rules[rule_idx++] = (FuzzyRule){ 1, 1, 1, 1, 1, 1, SCHED_ALGO_EDF, 0.5 };

    fs->rule_count = rule_idx;
}

// ==================== Fuzzy Logic Core Functions ====================

static float calculate_membership(float x, MembershipFunc mf) {
    switch (mf.type) {
    case MF_TRIANGULAR:
        if (x <= mf.a) return 0;
        if (x <= mf.b) return (x - mf.a) / (mf.b - mf.a);
        if (x <= mf.c) return (mf.c - x) / (mf.c - mf.b);
        return 0;

    case MF_TRAPEZOIDAL:
        if (x <= mf.a) return 0;
        if (x <= mf.b) return (x - mf.a) / (mf.b - mf.a);
        if (x <= mf.c) return 1;
        if (x <= mf.d) return (mf.d - x) / (mf.d - mf.c);
        return 0;

    case MF_GAUSSIAN:
        return expf(-((x - mf.b) * (x - mf.b)) / (2 * mf.c * mf.c));

    default:
        return 0;
    }
}

static void fuzzify_inputs(FuzzyScheduler* fs, FuzzyInputs* inputs,
    float cpu_mem[3], float deadline_mem[3],
    float periodicity_mem[3], float variability_mem[3],
    float criticality_mem[3], float energy_mem[3]) {

    cpu_mem[0] = calculate_membership(inputs->cpu_utilization, fs->cpu_util_mf.low);
    cpu_mem[1] = calculate_membership(inputs->cpu_utilization, fs->cpu_util_mf.medium);
    cpu_mem[2] = calculate_membership(inputs->cpu_utilization, fs->cpu_util_mf.high);

    deadline_mem[0] = calculate_membership(inputs->deadline_urgency, fs->deadline_mf.low);
    deadline_mem[1] = calculate_membership(inputs->deadline_urgency, fs->deadline_mf.medium);
    deadline_mem[2] = calculate_membership(inputs->deadline_urgency, fs->deadline_mf.high);

    periodicity_mem[0] = calculate_membership(inputs->task_periodicity, fs->periodicity_mf.low);
    periodicity_mem[1] = calculate_membership(inputs->task_periodicity, fs->periodicity_mf.medium);
    periodicity_mem[2] = calculate_membership(inputs->task_periodicity, fs->periodicity_mf.high);

    variability_mem[0] = calculate_membership(inputs->workload_variability, fs->variability_mf.low);
    variability_mem[1] = calculate_membership(inputs->workload_variability, fs->variability_mf.medium);
    variability_mem[2] = calculate_membership(inputs->workload_variability, fs->variability_mf.high);

    criticality_mem[0] = calculate_membership(inputs->criticality_mix, fs->criticality_mf.low);
    criticality_mem[1] = calculate_membership(inputs->criticality_mix, fs->criticality_mf.medium);
    criticality_mem[2] = calculate_membership(inputs->criticality_mix, fs->criticality_mf.high);

    energy_mem[0] = calculate_membership(inputs->energy_efficiency, fs->energy_mf.low);
    energy_mem[1] = calculate_membership(inputs->energy_efficiency, fs->energy_mf.medium);
    energy_mem[2] = calculate_membership(inputs->energy_efficiency, fs->energy_mf.high);
}

static void evaluate_rules(FuzzyScheduler* fs,
    float cpu_mem[3], float deadline_mem[3],
    float periodicity_mem[3], float variability_mem[3],
    float criticality_mem[3], float energy_mem[3],
    float rule_strengths[SCHED_ALGO_COUNT]) {

    // Initialize rule strengths to 0
    for (int i = 0; i < SCHED_ALGO_COUNT; i++) {
        rule_strengths[i] = 0;
    }

    // Evaluate each rule using MIN for AND operation
    for (int i = 0; i < fs->rule_count; i++) {
        FuzzyRule* rule = &fs->rules[i];

        float strength = 1.0f;

        // Apply AND operation (MIN) for all conditions
        strength = fminf(strength, cpu_mem[rule->cpu_util_idx]);
        strength = fminf(strength, deadline_mem[rule->deadline_idx]);
        strength = fminf(strength, periodicity_mem[rule->periodicity_idx]);
        strength = fminf(strength, variability_mem[rule->variability_idx]);
        strength = fminf(strength, criticality_mem[rule->criticality_idx]);
        strength = fminf(strength, energy_mem[rule->energy_idx]);

        // Apply rule weight
        strength *= rule->weight;

        // Take maximum for each output (OR operation for same conclusions)
        if (strength > rule_strengths[rule->output]) {
            rule_strengths[rule->output] = strength;
        }
    }
}

static void extract_fuzzy_inputs(SimMetrics* metrics, FuzzyInputs* inputs) {
    Tick total_ticks;
    uint64_t total_energy;

    if (!metrics || !inputs) return;

    // Calculate CPU utilization
    total_ticks = metrics->cpu_busy_ticks + metrics->cpu_idle_ticks;
    if (total_ticks > 0) {
        inputs->cpu_utilization = (float)metrics->cpu_busy_ticks / total_ticks;
    }
    else {
        inputs->cpu_utilization = 0;
    }

    // Deadline urgency (based on preemptions and dispatches)
    if (metrics->total_dispatches > 0) {
        // More preemptions might indicate tight deadlines
        inputs->deadline_urgency = fminf(1.0f,
            (float)metrics->total_preemptions / metrics->total_dispatches * 1.5f);
    }
    else {
        inputs->deadline_urgency = 0;
    }

    // Task periodicity (inverse of preemption rate)
    if (metrics->total_dispatches > 0) {
        float preemption_ratio = (float)metrics->total_preemptions / metrics->total_dispatches;
        inputs->task_periodicity = 1.0f - fminf(1.0f, preemption_ratio);
    }
    else {
        inputs->task_periodicity = 0.5f;
    }

    // Workload variability (based on energy consumption patterns)
    total_energy = metrics->energy_run + metrics->energy_idle + metrics->energy_sleep;
    if (total_energy > 0) {
        // Higher variability when energy states change frequently
        float run_ratio = (float)metrics->energy_run / total_energy;
        float idle_ratio = (float)metrics->energy_idle / total_energy;
        float sleep_ratio = (float)metrics->energy_sleep / total_energy;

        // Simple variability metric (0 = constant, 1 = highly variable)
        float variability = 1.0f - fmaxf(fmaxf(run_ratio, idle_ratio), sleep_ratio);
        inputs->workload_variability = fminf(1.0f, variability * 1.5f);
    }
    else {
        inputs->workload_variability = 0.5f;
    }

    // Criticality mix (based on network activity relative to processing)
    uint64_t total_network = metrics->net_tx_bytes + metrics->net_rx_bytes;
    if (total_network > 0 && metrics->energy_run > 0) {
        // More network activity might indicate mixed criticality
        float network_intensity = (float)total_network / (metrics->energy_run / 1000);
        inputs->criticality_mix = fminf(1.0f, network_intensity / 1000.0f);
    }
    else {
        inputs->criticality_mix = 0.3f;
    }

    // Energy efficiency importance
    if (total_energy > 0) {
        // Higher energy efficiency needed when energy_run is high relative to total
        inputs->energy_efficiency = 1.0f - fminf(1.0f,
            (float)metrics->energy_run / total_energy);
    }
    else {
        inputs->energy_efficiency = 0.5f;
    }

    // Clamp all values to [0,1]
    inputs->cpu_utilization = fminf(1.0f, fmaxf(0.0f, inputs->cpu_utilization));
    inputs->deadline_urgency = fminf(1.0f, fmaxf(0.0f, inputs->deadline_urgency));
    inputs->task_periodicity = fminf(1.0f, fmaxf(0.0f, inputs->task_periodicity));
    inputs->workload_variability = fminf(1.0f, fmaxf(0.0f, inputs->workload_variability));
    inputs->criticality_mix = fminf(1.0f, fmaxf(0.0f, inputs->criticality_mix));
    inputs->energy_efficiency = fminf(1.0f, fmaxf(0.0f, inputs->energy_efficiency));
}

static SchedulingAlgoType defuzzify(float* rule_strengths) {
    int best_algorithm = SCHED_ALGO_RM;
    float max_strength = rule_strengths[SCHED_ALGO_RM];

    for (int i = SCHED_ALGO_RM + 1; i < SCHED_ALGO_COUNT; i++) {
        if (rule_strengths[i] > max_strength) {
            max_strength = rule_strengths[i];
            best_algorithm = i;
        }
    }

    // If all strengths are very low, default to RM
    if (max_strength < 0.1f) {
        return SCHED_ALGO_RM;
    }

    return (SchedulingAlgoType)best_algorithm;
}

// ==================== Public API Implementation ====================

FuzzyScheduler* FuzzyScheduler_Init(SimMetrics* metrics, uint32_t selection_interval) {
    FuzzyScheduler* fs;

    if (!metrics) {
        return NULL;
    }

    fs = (FuzzyScheduler*)malloc(sizeof(FuzzyScheduler));
    if (!fs) {
        return NULL;
    }

    memset(fs, 0, sizeof(FuzzyScheduler));
    fs->metrics = metrics;
    fs->selection_interval = selection_interval;
    fs->current_algo = SCHED_ALGO_RM;
    fs->last_selection_tick = 0;

    init_membership_functions(fs);
    init_fuzzy_rules(fs);

    return fs;
}

void FuzzyScheduler_Destroy(FuzzyScheduler* fs) {
    if (fs) {
        free(fs);
    }
}

SchedulingAlgoType FuzzyScheduler_Update(FuzzyScheduler* fs) {
    SchedulingAlgoType new_algo;

    if (!fs || !fs->metrics) {
        return SCHED_ALGO_RM;
    }

    // Check if it's time to reevaluate
    if (fs->metrics->sim_ticks - fs->last_selection_tick >= fs->selection_interval) {
        new_algo = FuzzyScheduler_SelectAlgorithm(fs);

        if (new_algo != fs->current_algo) {
            fs->current_algo = new_algo;
        }

        fs->last_selection_tick = fs->metrics->sim_ticks;
    }

    return fs->current_algo;
}

SchedulingAlgoType FuzzyScheduler_SelectAlgorithm(FuzzyScheduler* fs) {
    float cpu_mem[3], deadline_mem[3], periodicity_mem[3];
    float variability_mem[3], criticality_mem[3], energy_mem[3];

    if (!fs || !fs->metrics) {
        return SCHED_ALGO_RM;
    }

    // Extract inputs from metrics
    extract_fuzzy_inputs(fs->metrics, &fs->last_inputs);

    // Fuzzify inputs
    fuzzify_inputs(fs, &fs->last_inputs, cpu_mem, deadline_mem, periodicity_mem,
        variability_mem, criticality_mem, energy_mem);

    // Evaluate rules
    evaluate_rules(fs, cpu_mem, deadline_mem, periodicity_mem,
        variability_mem, criticality_mem, energy_mem, fs->last_strengths);

    // Defuzzify and return result
    return defuzzify(fs->last_strengths);
}

const char* FuzzyScheduler_GetAlgorithmName(SchedulingAlgoType algo) {
    switch (algo) {
    case SCHED_ALGO_RM:   return "Rate Monotonic";
    case SCHED_ALGO_DM:   return "Deadline Monotonic";
    case SCHED_ALGO_EDF:  return "Earliest Deadline First";
    case SCHED_ALGO_LLF:  return "Least Laxity First";
    case SCHED_ALGO_SRTF: return "Shortest Remaining Time First";
    default:              return "Unknown";
    }
}

void FuzzyScheduler_Reset(FuzzyScheduler* fs) {
    if (fs) {
        fs->current_algo = SCHED_ALGO_RM;
        fs->last_selection_tick = 0;
        memset(&fs->last_inputs, 0, sizeof(FuzzyInputs));
        memset(fs->last_strengths, 0, sizeof(fs->last_strengths));
    }
}

void FuzzyScheduler_GetDetailedOutputs(FuzzyScheduler* fs, float strengths[SCHED_ALGO_COUNT]) {
    if (!fs || !strengths) return;

    for (int i = 0; i < SCHED_ALGO_COUNT; i++) {
        strengths[i] = fs->last_strengths[i];
    }
}

void FuzzyScheduler_GetInputValues(FuzzyScheduler* fs,
    float* cpu_util,
    float* deadline_urgency,
    float* periodicity,
    float* variability,
    float* criticality,
    float* energy_eff) {
    if (!fs) return;

    if (cpu_util) *cpu_util = fs->last_inputs.cpu_utilization;
    if (deadline_urgency) *deadline_urgency = fs->last_inputs.deadline_urgency;
    if (periodicity) *periodicity = fs->last_inputs.task_periodicity;
    if (variability) *variability = fs->last_inputs.workload_variability;
    if (criticality) *criticality = fs->last_inputs.criticality_mix;
    if (energy_eff) *energy_eff = fs->last_inputs.energy_efficiency;
}

int FuzzyScheduler_SetRuleWeight(FuzzyScheduler* fs, int rule_index, float weight) {
    if (!fs || rule_index < 0 || rule_index >= fs->rule_count) {
        return -1;
    }

    if (weight < 0.0f || weight > 1.0f) {
        return -1;
    }

    fs->rules[rule_index].weight = weight;
    return 0;
}