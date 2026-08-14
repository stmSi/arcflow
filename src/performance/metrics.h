#ifndef AF_PERFORMANCE_METRICS_H
#define AF_PERFORMANCE_METRICS_H

#include "arcflow/arcflow_result.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum AfPerformanceSubsystem {
    AF_PERFORMANCE_ANIMATION = 0,
    AF_PERFORMANCE_LAYOUT,
    AF_PERFORMANCE_HIT_TESTING,
    AF_PERFORMANCE_SCENE_TRAVERSAL,
    AF_PERFORMANCE_TESSELLATION,
    AF_PERFORMANCE_GEOMETRY_CACHE,
    AF_PERFORMANCE_TEXT_SHAPING,
    AF_PERFORMANCE_GLYPH_UPLOAD,
    AF_PERFORMANCE_RENDER_COMMAND_GENERATION,
    AF_PERFORMANCE_DRAW_SUBMISSION,
    AF_PERFORMANCE_PARTICLE_SIMULATION,
    AF_PERFORMANCE_GPU_COMPOSITION,
    AF_PERFORMANCE_PACKAGE_LOADING,
    AF_PERFORMANCE_EDITOR_UNDO,
    AF_PERFORMANCE_SUBSYSTEM_COUNT
} AfPerformanceSubsystem;

typedef enum AfPerformanceCounter {
    AF_PERFORMANCE_DRAW_COUNT = 0,
    AF_PERFORMANCE_PIPELINE_CHANGES,
    AF_PERFORMANCE_DESCRIPTOR_CHANGES,
    AF_PERFORMANCE_INSTANCE_COUNT,
    AF_PERFORMANCE_TESSELLATED_PATH_COUNT,
    AF_PERFORMANCE_UPLOADED_BYTES,
    AF_PERFORMANCE_GENERAL_ALLOCATIONS,
    AF_PERFORMANCE_COUNTER_COUNT
} AfPerformanceCounter;

typedef enum AfPerformanceGauge {
    AF_PERFORMANCE_GPU_MEMORY_BYTES = 0,
    AF_PERFORMANCE_GAUGE_COUNT
} AfPerformanceGauge;

typedef struct AfPerformanceSubsystemMetrics {
    uint64_t elapsed_nanoseconds;
    uint64_t invocation_count;
    uint64_t work_item_count;
    uint64_t processed_byte_count;
} AfPerformanceSubsystemMetrics;

typedef struct AfPerformanceFrameMetrics {
    uint64_t frame_index;
    AfPerformanceSubsystemMetrics subsystems[AF_PERFORMANCE_SUBSYSTEM_COUNT];
    uint64_t counters[AF_PERFORMANCE_COUNTER_COUNT];
    uint64_t gauges[AF_PERFORMANCE_GAUGE_COUNT];
} AfPerformanceFrameMetrics;

typedef struct AfPerformanceSubsystemAggregate {
    uint64_t sample_count;
    uint64_t total_nanoseconds;
    uint64_t minimum_nanoseconds;
    uint64_t maximum_nanoseconds;
    uint64_t invocation_count;
    uint64_t work_item_count;
    uint64_t processed_byte_count;
} AfPerformanceSubsystemAggregate;

typedef struct AfPerformanceAggregate {
    uint64_t frame_count;
    AfPerformanceSubsystemAggregate subsystems[AF_PERFORMANCE_SUBSYSTEM_COUNT];
    uint64_t counter_totals[AF_PERFORMANCE_COUNTER_COUNT];
    uint64_t gauge_peaks[AF_PERFORMANCE_GAUGE_COUNT];
} AfPerformanceAggregate;

typedef struct AfPerformanceMetrics {
    AfPerformanceFrameMetrics frame;
    AfPerformanceAggregate aggregate;
    uint64_t frame_generation;
    bool frame_active;
} AfPerformanceMetrics;

typedef struct AfPerformanceTimer {
    uint64_t start_nanoseconds;
    uint64_t frame_generation;
    AfPerformanceSubsystem subsystem;
    bool active;
} AfPerformanceTimer;

typedef struct AfMemoryMetrics {
    uint64_t live_bytes;
    uint64_t peak_bytes;
    uint64_t live_allocation_count;
    uint64_t peak_allocation_count;
    uint64_t allocation_count;
    uint64_t deallocation_count;
    uint64_t failed_allocation_count;
} AfMemoryMetrics;

typedef struct AfBenchmarkStats {
    uint64_t workload_hash;
    uint64_t hashed_byte_count;
    uint64_t sample_count;
    uint64_t sample_total;
    uint64_t sample_minimum;
    uint64_t sample_maximum;
    uint64_t work_item_count;
    uint64_t processed_byte_count;
    bool has_samples;
} AfBenchmarkStats;

void af_performance_metrics_init(AfPerformanceMetrics *metrics);
AfResult af_performance_begin_frame(AfPerformanceMetrics *metrics, uint64_t frame_index);
AfResult af_performance_end_frame(AfPerformanceMetrics *metrics,
                                  AfPerformanceFrameMetrics *out_frame);
AfResult af_performance_subsystem_add(AfPerformanceMetrics *metrics,
                                      AfPerformanceSubsystem subsystem,
                                      uint64_t elapsed_nanoseconds, uint64_t invocation_count,
                                      uint64_t work_item_count, uint64_t processed_byte_count);
AfResult af_performance_counter_add(AfPerformanceMetrics *metrics, AfPerformanceCounter counter,
                                    uint64_t amount);
AfResult af_performance_gauge_set(AfPerformanceMetrics *metrics, AfPerformanceGauge gauge,
                                  uint64_t value);
AfResult af_performance_timer_begin(AfPerformanceMetrics *metrics, AfPerformanceSubsystem subsystem,
                                    uint64_t timestamp_nanoseconds, AfPerformanceTimer *timer);
AfResult af_performance_timer_end(AfPerformanceMetrics *metrics, uint64_t timestamp_nanoseconds,
                                  uint64_t work_item_count, uint64_t processed_byte_count,
                                  AfPerformanceTimer *timer);
void af_performance_timer_cancel(AfPerformanceTimer *timer);
const char *af_performance_subsystem_name(AfPerformanceSubsystem subsystem);

void af_memory_metrics_init(AfMemoryMetrics *metrics);
AfResult af_memory_metrics_record_allocation(AfMemoryMetrics *metrics, uint64_t size);
AfResult af_memory_metrics_record_deallocation(AfMemoryMetrics *metrics, uint64_t size);
AfResult af_memory_metrics_record_failed_allocation(AfMemoryMetrics *metrics);
bool af_memory_metrics_balanced(const AfMemoryMetrics *metrics);

void af_benchmark_stats_init(AfBenchmarkStats *stats, uint64_t seed);
AfResult af_benchmark_hash_bytes(AfBenchmarkStats *stats, const void *data, size_t size);
AfResult af_benchmark_hash_u64(AfBenchmarkStats *stats, uint64_t value);
AfResult af_benchmark_record_sample(AfBenchmarkStats *stats, uint64_t sample,
                                    uint64_t work_item_count, uint64_t processed_byte_count);
double af_benchmark_sample_mean(const AfBenchmarkStats *stats);
bool af_benchmark_workload_equal(const AfBenchmarkStats *left, const AfBenchmarkStats *right);

#endif
