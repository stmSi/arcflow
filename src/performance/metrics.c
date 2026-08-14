#include "performance/metrics.h"

#include "core/safe_math.h"

#include <string.h>

#define AF_BENCHMARK_HASH_OFFSET UINT64_C(14695981039346656037)
#define AF_BENCHMARK_HASH_PRIME UINT64_C(1099511628211)

static bool af_performance_subsystem_valid(AfPerformanceSubsystem subsystem) {
    return subsystem >= AF_PERFORMANCE_ANIMATION && subsystem < AF_PERFORMANCE_SUBSYSTEM_COUNT;
}

static bool af_performance_counter_valid(AfPerformanceCounter counter) {
    return counter >= AF_PERFORMANCE_DRAW_COUNT && counter < AF_PERFORMANCE_COUNTER_COUNT;
}

static bool af_performance_gauge_valid(AfPerformanceGauge gauge) {
    return gauge >= AF_PERFORMANCE_GPU_MEMORY_BYTES && gauge < AF_PERFORMANCE_GAUGE_COUNT;
}

static bool af_performance_add_u64(uint64_t left, uint64_t right, uint64_t *out_value) {
    return !af_u64_add_overflow(left, right, out_value);
}

void af_performance_metrics_init(AfPerformanceMetrics *metrics) {
    if (metrics != NULL) {
        *metrics = (AfPerformanceMetrics){0};
    }
}

AfResult af_performance_begin_frame(AfPerformanceMetrics *metrics, uint64_t frame_index) {
    uint64_t generation = 0U;
    if (metrics == NULL || metrics->frame_active ||
        !af_performance_add_u64(metrics->frame_generation, 1U, &generation)) {
        return metrics != NULL && metrics->frame_generation == UINT64_MAX
                   ? AF_RESULT_LIMIT_EXCEEDED
                   : AF_RESULT_INVALID_ARGUMENT;
    }
    metrics->frame = (AfPerformanceFrameMetrics){.frame_index = frame_index};
    metrics->frame_generation = generation;
    metrics->frame_active = true;
    return AF_RESULT_SUCCESS;
}

static AfResult af_performance_aggregate_frame(const AfPerformanceMetrics *metrics,
                                               AfPerformanceAggregate *out_aggregate) {
    AfPerformanceAggregate aggregate = metrics->aggregate;
    if (!af_performance_add_u64(aggregate.frame_count, 1U, &aggregate.frame_count)) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    for (uint32_t index = 0U; index < (uint32_t)AF_PERFORMANCE_SUBSYSTEM_COUNT; ++index) {
        const AfPerformanceSubsystemMetrics *frame = &metrics->frame.subsystems[index];
        AfPerformanceSubsystemAggregate *total = &aggregate.subsystems[index];
        uint64_t sample_count = 0U;
        if (frame->invocation_count == 0U) {
            continue;
        }
        if (!af_performance_add_u64(total->sample_count, 1U, &sample_count) ||
            !af_performance_add_u64(total->total_nanoseconds, frame->elapsed_nanoseconds,
                                    &total->total_nanoseconds) ||
            !af_performance_add_u64(total->invocation_count, frame->invocation_count,
                                    &total->invocation_count) ||
            !af_performance_add_u64(total->work_item_count, frame->work_item_count,
                                    &total->work_item_count) ||
            !af_performance_add_u64(total->processed_byte_count, frame->processed_byte_count,
                                    &total->processed_byte_count)) {
            return AF_RESULT_LIMIT_EXCEEDED;
        }
        if (total->sample_count == 0U || frame->elapsed_nanoseconds < total->minimum_nanoseconds) {
            total->minimum_nanoseconds = frame->elapsed_nanoseconds;
        }
        if (total->sample_count == 0U || frame->elapsed_nanoseconds > total->maximum_nanoseconds) {
            total->maximum_nanoseconds = frame->elapsed_nanoseconds;
        }
        total->sample_count = sample_count;
    }
    for (uint32_t index = 0U; index < (uint32_t)AF_PERFORMANCE_COUNTER_COUNT; ++index) {
        if (!af_performance_add_u64(aggregate.counter_totals[index], metrics->frame.counters[index],
                                    &aggregate.counter_totals[index])) {
            return AF_RESULT_LIMIT_EXCEEDED;
        }
    }
    for (uint32_t index = 0U; index < (uint32_t)AF_PERFORMANCE_GAUGE_COUNT; ++index) {
        if (metrics->frame.gauges[index] > aggregate.gauge_peaks[index]) {
            aggregate.gauge_peaks[index] = metrics->frame.gauges[index];
        }
    }
    *out_aggregate = aggregate;
    return AF_RESULT_SUCCESS;
}

AfResult af_performance_end_frame(AfPerformanceMetrics *metrics,
                                  AfPerformanceFrameMetrics *out_frame) {
    AfPerformanceAggregate aggregate;
    AfResult result = AF_RESULT_SUCCESS;
    if (metrics == NULL || !metrics->frame_active) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    result = af_performance_aggregate_frame(metrics, &aggregate);
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    metrics->aggregate = aggregate;
    metrics->frame_active = false;
    if (out_frame != NULL) {
        *out_frame = metrics->frame;
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_performance_subsystem_add(AfPerformanceMetrics *metrics,
                                      AfPerformanceSubsystem subsystem,
                                      uint64_t elapsed_nanoseconds, uint64_t invocation_count,
                                      uint64_t work_item_count, uint64_t processed_byte_count) {
    AfPerformanceSubsystemMetrics updated;
    if (metrics == NULL || !metrics->frame_active || !af_performance_subsystem_valid(subsystem) ||
        invocation_count == 0U) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    updated = metrics->frame.subsystems[(uint32_t)subsystem];
    if (!af_performance_add_u64(updated.elapsed_nanoseconds, elapsed_nanoseconds,
                                &updated.elapsed_nanoseconds) ||
        !af_performance_add_u64(updated.invocation_count, invocation_count,
                                &updated.invocation_count) ||
        !af_performance_add_u64(updated.work_item_count, work_item_count,
                                &updated.work_item_count) ||
        !af_performance_add_u64(updated.processed_byte_count, processed_byte_count,
                                &updated.processed_byte_count)) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    metrics->frame.subsystems[(uint32_t)subsystem] = updated;
    return AF_RESULT_SUCCESS;
}

AfResult af_performance_counter_add(AfPerformanceMetrics *metrics, AfPerformanceCounter counter,
                                    uint64_t amount) {
    uint64_t updated = 0U;
    if (metrics == NULL || !metrics->frame_active || !af_performance_counter_valid(counter)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (!af_performance_add_u64(metrics->frame.counters[(uint32_t)counter], amount, &updated)) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    metrics->frame.counters[(uint32_t)counter] = updated;
    return AF_RESULT_SUCCESS;
}

AfResult af_performance_gauge_set(AfPerformanceMetrics *metrics, AfPerformanceGauge gauge,
                                  uint64_t value) {
    if (metrics == NULL || !metrics->frame_active || !af_performance_gauge_valid(gauge)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    metrics->frame.gauges[(uint32_t)gauge] = value;
    return AF_RESULT_SUCCESS;
}

AfResult af_performance_timer_begin(AfPerformanceMetrics *metrics, AfPerformanceSubsystem subsystem,
                                    uint64_t timestamp_nanoseconds, AfPerformanceTimer *timer) {
    if (metrics == NULL || timer == NULL || !metrics->frame_active || timer->active ||
        !af_performance_subsystem_valid(subsystem)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *timer = (AfPerformanceTimer){.start_nanoseconds = timestamp_nanoseconds,
                                  .frame_generation = metrics->frame_generation,
                                  .subsystem = subsystem,
                                  .active = true};
    return AF_RESULT_SUCCESS;
}

AfResult af_performance_timer_end(AfPerformanceMetrics *metrics, uint64_t timestamp_nanoseconds,
                                  uint64_t work_item_count, uint64_t processed_byte_count,
                                  AfPerformanceTimer *timer) {
    AfResult result = AF_RESULT_SUCCESS;
    if (metrics == NULL || timer == NULL || !metrics->frame_active || !timer->active ||
        timer->frame_generation != metrics->frame_generation ||
        timestamp_nanoseconds < timer->start_nanoseconds) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    result = af_performance_subsystem_add(metrics, timer->subsystem,
                                          timestamp_nanoseconds - timer->start_nanoseconds, 1U,
                                          work_item_count, processed_byte_count);
    if (result == AF_RESULT_SUCCESS) {
        timer->active = false;
    }
    return result;
}

void af_performance_timer_cancel(AfPerformanceTimer *timer) {
    if (timer != NULL) {
        timer->active = false;
    }
}

const char *af_performance_subsystem_name(AfPerformanceSubsystem subsystem) {
    static const char *const names[AF_PERFORMANCE_SUBSYSTEM_COUNT] = {
        "Animation",       "Layout",          "Hit testing",         "Scene traversal",
        "Tessellation",    "Geometry cache",  "Text shaping",        "Glyph upload",
        "Render commands", "Draw submission", "Particle simulation", "GPU composition",
        "Package loading", "Editor undo"};
    return af_performance_subsystem_valid(subsystem) ? names[(uint32_t)subsystem] : "Unknown";
}

void af_memory_metrics_init(AfMemoryMetrics *metrics) {
    if (metrics != NULL) {
        *metrics = (AfMemoryMetrics){0};
    }
}

AfResult af_memory_metrics_record_allocation(AfMemoryMetrics *metrics, uint64_t size) {
    AfMemoryMetrics updated;
    if (metrics == NULL || size == 0U) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    updated = *metrics;
    if (!af_performance_add_u64(updated.live_bytes, size, &updated.live_bytes) ||
        !af_performance_add_u64(updated.live_allocation_count, 1U,
                                &updated.live_allocation_count) ||
        !af_performance_add_u64(updated.allocation_count, 1U, &updated.allocation_count)) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    if (updated.live_bytes > updated.peak_bytes) {
        updated.peak_bytes = updated.live_bytes;
    }
    if (updated.live_allocation_count > updated.peak_allocation_count) {
        updated.peak_allocation_count = updated.live_allocation_count;
    }
    *metrics = updated;
    return AF_RESULT_SUCCESS;
}

AfResult af_memory_metrics_record_deallocation(AfMemoryMetrics *metrics, uint64_t size) {
    AfMemoryMetrics updated;
    if (metrics == NULL || size == 0U || size > metrics->live_bytes ||
        metrics->live_allocation_count == 0U) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    updated = *metrics;
    if (!af_performance_add_u64(updated.deallocation_count, 1U, &updated.deallocation_count)) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    updated.live_bytes -= size;
    --updated.live_allocation_count;
    *metrics = updated;
    return AF_RESULT_SUCCESS;
}

AfResult af_memory_metrics_record_failed_allocation(AfMemoryMetrics *metrics) {
    uint64_t count = 0U;
    if (metrics == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (!af_performance_add_u64(metrics->failed_allocation_count, 1U, &count)) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    metrics->failed_allocation_count = count;
    return AF_RESULT_SUCCESS;
}

bool af_memory_metrics_balanced(const AfMemoryMetrics *metrics) {
    return metrics != NULL && metrics->live_bytes == 0U && metrics->live_allocation_count == 0U &&
           metrics->allocation_count == metrics->deallocation_count;
}

static uint64_t af_benchmark_hash_byte(uint64_t hash, uint8_t value) {
    return (hash ^ value) * AF_BENCHMARK_HASH_PRIME;
}

static uint64_t af_benchmark_hash_raw_u64(uint64_t hash, uint64_t value) {
    for (uint32_t shift = 0U; shift < 64U; shift += 8U) {
        hash = af_benchmark_hash_byte(hash, (uint8_t)(value >> shift));
    }
    return hash;
}

void af_benchmark_stats_init(AfBenchmarkStats *stats, uint64_t seed) {
    if (stats != NULL) {
        *stats = (AfBenchmarkStats){.workload_hash =
                                        af_benchmark_hash_raw_u64(AF_BENCHMARK_HASH_OFFSET, seed)};
    }
}

AfResult af_benchmark_hash_bytes(AfBenchmarkStats *stats, const void *data, size_t size) {
    const uint8_t *bytes = data;
    uint64_t hash = 0U;
    uint64_t byte_count = 0U;
    if (stats == NULL || (data == NULL && size != 0U)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (!af_performance_add_u64(stats->hashed_byte_count, (uint64_t)size, &byte_count)) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    hash = af_benchmark_hash_byte(stats->workload_hash, 0x42U);
    hash = af_benchmark_hash_raw_u64(hash, (uint64_t)size);
    for (size_t index = 0U; index < size; ++index) {
        hash = af_benchmark_hash_byte(hash, bytes[index]);
    }
    stats->workload_hash = hash;
    stats->hashed_byte_count = byte_count;
    return AF_RESULT_SUCCESS;
}

AfResult af_benchmark_hash_u64(AfBenchmarkStats *stats, uint64_t value) {
    uint64_t byte_count = 0U;
    uint64_t hash = 0U;
    if (stats == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (!af_performance_add_u64(stats->hashed_byte_count, sizeof(value), &byte_count)) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    hash = af_benchmark_hash_byte(stats->workload_hash, 0x55U);
    stats->workload_hash = af_benchmark_hash_raw_u64(hash, value);
    stats->hashed_byte_count = byte_count;
    return AF_RESULT_SUCCESS;
}

AfResult af_benchmark_record_sample(AfBenchmarkStats *stats, uint64_t sample,
                                    uint64_t work_item_count, uint64_t processed_byte_count) {
    AfBenchmarkStats updated;
    uint64_t sample_count = 0U;
    if (stats == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    updated = *stats;
    if (!af_performance_add_u64(updated.sample_count, 1U, &sample_count) ||
        !af_performance_add_u64(updated.sample_total, sample, &updated.sample_total) ||
        !af_performance_add_u64(updated.work_item_count, work_item_count,
                                &updated.work_item_count) ||
        !af_performance_add_u64(updated.processed_byte_count, processed_byte_count,
                                &updated.processed_byte_count)) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    if (!updated.has_samples || sample < updated.sample_minimum) {
        updated.sample_minimum = sample;
    }
    if (!updated.has_samples || sample > updated.sample_maximum) {
        updated.sample_maximum = sample;
    }
    updated.sample_count = sample_count;
    updated.has_samples = true;
    updated.workload_hash = af_benchmark_hash_byte(updated.workload_hash, 0x53U);
    updated.workload_hash = af_benchmark_hash_raw_u64(updated.workload_hash, sample);
    updated.workload_hash = af_benchmark_hash_raw_u64(updated.workload_hash, work_item_count);
    updated.workload_hash = af_benchmark_hash_raw_u64(updated.workload_hash, processed_byte_count);
    *stats = updated;
    return AF_RESULT_SUCCESS;
}

double af_benchmark_sample_mean(const AfBenchmarkStats *stats) {
    return stats != NULL && stats->sample_count != 0U
               ? (double)stats->sample_total / (double)stats->sample_count
               : 0.0;
}

bool af_benchmark_workload_equal(const AfBenchmarkStats *left, const AfBenchmarkStats *right) {
    return left != NULL && right != NULL && left->workload_hash == right->workload_hash &&
           left->hashed_byte_count == right->hashed_byte_count &&
           left->sample_count == right->sample_count && left->sample_total == right->sample_total &&
           left->sample_minimum == right->sample_minimum &&
           left->sample_maximum == right->sample_maximum &&
           left->work_item_count == right->work_item_count &&
           left->processed_byte_count == right->processed_byte_count &&
           left->has_samples == right->has_samples;
}
