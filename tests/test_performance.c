#include "af/performance/metrics.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(expression)                                                                          \
    do {                                                                                           \
        if (!(expression)) {                                                                       \
            (void)fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #expression);   \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

static bool af_test_performance_frames(void) {
    AfPerformanceMetrics metrics;
    AfPerformanceFrameMetrics frame;
    AfPerformanceTimer timer = {0};
    const AfPerformanceSubsystemAggregate *animation = NULL;
    af_performance_metrics_init(&metrics);
    CHECK(af_performance_end_frame(&metrics, NULL) == AF_RESULT_INVALID_ARGUMENT);
    CHECK(af_performance_begin_frame(&metrics, 7U) == AF_RESULT_SUCCESS);
    CHECK(af_performance_begin_frame(&metrics, 8U) == AF_RESULT_INVALID_ARGUMENT);
    CHECK(af_performance_timer_begin(&metrics, AF_PERFORMANCE_ANIMATION, 100U, &timer) ==
          AF_RESULT_SUCCESS);
    CHECK(af_performance_timer_end(&metrics, 160U, 7U, 32U, &timer) == AF_RESULT_SUCCESS);
    CHECK(af_performance_subsystem_add(&metrics, AF_PERFORMANCE_LAYOUT, 40U, 2U, 3U, 10U) ==
          AF_RESULT_SUCCESS);
    CHECK(af_performance_counter_add(&metrics, AF_PERFORMANCE_DRAW_COUNT, 4U) == AF_RESULT_SUCCESS);
    CHECK(af_performance_counter_add(&metrics, AF_PERFORMANCE_UPLOADED_BYTES, 1024U) ==
          AF_RESULT_SUCCESS);
    CHECK(af_performance_gauge_set(&metrics, AF_PERFORMANCE_GPU_MEMORY_BYTES, 4096U) ==
          AF_RESULT_SUCCESS);
    CHECK(af_performance_end_frame(&metrics, &frame) == AF_RESULT_SUCCESS);
    CHECK(frame.frame_index == 7U);
    CHECK(frame.subsystems[AF_PERFORMANCE_ANIMATION].elapsed_nanoseconds == 60U);
    CHECK(frame.subsystems[AF_PERFORMANCE_LAYOUT].invocation_count == 2U);
    CHECK(frame.counters[AF_PERFORMANCE_DRAW_COUNT] == 4U);

    CHECK(af_performance_begin_frame(&metrics, 8U) == AF_RESULT_SUCCESS);
    CHECK(af_performance_timer_begin(&metrics, AF_PERFORMANCE_ANIMATION, 200U, &timer) ==
          AF_RESULT_SUCCESS);
    CHECK(af_performance_timer_end(&metrics, 230U, 3U, 16U, &timer) == AF_RESULT_SUCCESS);
    CHECK(af_performance_counter_add(&metrics, AF_PERFORMANCE_DRAW_COUNT, 2U) == AF_RESULT_SUCCESS);
    CHECK(af_performance_gauge_set(&metrics, AF_PERFORMANCE_GPU_MEMORY_BYTES, 2048U) ==
          AF_RESULT_SUCCESS);
    CHECK(af_performance_end_frame(&metrics, NULL) == AF_RESULT_SUCCESS);
    animation = &metrics.aggregate.subsystems[AF_PERFORMANCE_ANIMATION];
    CHECK(metrics.aggregate.frame_count == 2U);
    CHECK(animation->sample_count == 2U && animation->total_nanoseconds == 90U);
    CHECK(animation->minimum_nanoseconds == 30U && animation->maximum_nanoseconds == 60U);
    CHECK(animation->invocation_count == 2U && animation->work_item_count == 10U);
    CHECK(animation->processed_byte_count == 48U);
    CHECK(metrics.aggregate.counter_totals[AF_PERFORMANCE_DRAW_COUNT] == 6U);
    CHECK(metrics.aggregate.gauge_peaks[AF_PERFORMANCE_GPU_MEMORY_BYTES] == 4096U);
    CHECK(strcmp(af_performance_subsystem_name(AF_PERFORMANCE_TEXT_SHAPING), "Text shaping") == 0);
    CHECK(strcmp(af_performance_subsystem_name((AfPerformanceSubsystem)99), "Unknown") == 0);

    CHECK(af_performance_begin_frame(&metrics, 9U) == AF_RESULT_SUCCESS);
    CHECK(af_performance_timer_begin(&metrics, AF_PERFORMANCE_HIT_TESTING, 300U, &timer) ==
          AF_RESULT_SUCCESS);
    CHECK(af_performance_timer_end(&metrics, 299U, 0U, 0U, &timer) == AF_RESULT_INVALID_ARGUMENT);
    af_performance_timer_cancel(&timer);
    CHECK(af_performance_end_frame(&metrics, NULL) == AF_RESULT_SUCCESS);
    return true;
}

static bool af_test_memory_metrics(void) {
    AfMemoryMetrics metrics;
    af_memory_metrics_init(&metrics);
    CHECK(af_memory_metrics_balanced(&metrics));
    CHECK(af_memory_metrics_record_allocation(&metrics, 64U) == AF_RESULT_SUCCESS);
    CHECK(af_memory_metrics_record_allocation(&metrics, 32U) == AF_RESULT_SUCCESS);
    CHECK(metrics.live_bytes == 96U && metrics.peak_bytes == 96U);
    CHECK(metrics.live_allocation_count == 2U && metrics.peak_allocation_count == 2U);
    CHECK(af_memory_metrics_record_deallocation(&metrics, 97U) == AF_RESULT_INVALID_ARGUMENT);
    CHECK(metrics.live_bytes == 96U && metrics.deallocation_count == 0U);
    CHECK(af_memory_metrics_record_deallocation(&metrics, 64U) == AF_RESULT_SUCCESS);
    CHECK(af_memory_metrics_record_deallocation(&metrics, 32U) == AF_RESULT_SUCCESS);
    CHECK(af_memory_metrics_record_failed_allocation(&metrics) == AF_RESULT_SUCCESS);
    CHECK(metrics.failed_allocation_count == 1U);
    CHECK(af_memory_metrics_balanced(&metrics));

    af_memory_metrics_init(&metrics);
    CHECK(af_memory_metrics_record_allocation(&metrics, UINT64_MAX) == AF_RESULT_SUCCESS);
    CHECK(af_memory_metrics_record_allocation(&metrics, 1U) == AF_RESULT_LIMIT_EXCEEDED);
    CHECK(metrics.live_bytes == UINT64_MAX && metrics.allocation_count == 1U);
    CHECK(af_memory_metrics_record_deallocation(&metrics, UINT64_MAX) == AF_RESULT_SUCCESS);
    CHECK(af_memory_metrics_balanced(&metrics));
    return true;
}

static bool af_test_benchmark_stats(void) {
    static const uint8_t input[] = {3U, 1U, 4U, 1U, 5U, 9U};
    AfBenchmarkStats first;
    AfBenchmarkStats second;
    AfBenchmarkStats different;
    af_benchmark_stats_init(&first, 42U);
    af_benchmark_stats_init(&second, 42U);
    af_benchmark_stats_init(&different, 42U);
    CHECK(af_benchmark_hash_bytes(&first, input, sizeof(input)) == AF_RESULT_SUCCESS);
    CHECK(af_benchmark_hash_u64(&first, UINT64_C(0x123456789abcdef0)) == AF_RESULT_SUCCESS);
    CHECK(af_benchmark_record_sample(&first, 10U, 100U, 400U) == AF_RESULT_SUCCESS);
    CHECK(af_benchmark_record_sample(&first, 30U, 200U, 800U) == AF_RESULT_SUCCESS);

    CHECK(af_benchmark_hash_bytes(&second, input, sizeof(input)) == AF_RESULT_SUCCESS);
    CHECK(af_benchmark_hash_u64(&second, UINT64_C(0x123456789abcdef0)) == AF_RESULT_SUCCESS);
    CHECK(af_benchmark_record_sample(&second, 10U, 100U, 400U) == AF_RESULT_SUCCESS);
    CHECK(af_benchmark_record_sample(&second, 30U, 200U, 800U) == AF_RESULT_SUCCESS);
    CHECK(af_benchmark_workload_equal(&first, &second));
    CHECK(first.sample_minimum == 10U && first.sample_maximum == 30U);
    CHECK(first.sample_total == 40U && af_benchmark_sample_mean(&first) == 20.0);
    CHECK(first.work_item_count == 300U && first.processed_byte_count == 1200U);

    CHECK(af_benchmark_hash_bytes(&different, input, sizeof(input)) == AF_RESULT_SUCCESS);
    CHECK(af_benchmark_hash_u64(&different, UINT64_C(0x123456789abcdef0)) == AF_RESULT_SUCCESS);
    CHECK(af_benchmark_record_sample(&different, 11U, 100U, 400U) == AF_RESULT_SUCCESS);
    CHECK(af_benchmark_record_sample(&different, 30U, 200U, 800U) == AF_RESULT_SUCCESS);
    CHECK(!af_benchmark_workload_equal(&first, &different));

    first.sample_count = UINT64_MAX;
    CHECK(af_benchmark_record_sample(&first, 1U, 1U, 1U) == AF_RESULT_LIMIT_EXCEEDED);
    CHECK(first.sample_count == UINT64_MAX);
    return true;
}

int main(void) {
    static const struct {
        const char *name;
        bool (*function)(void);
    } tests[] = {{"performance frames", af_test_performance_frames},
                 {"memory metrics", af_test_memory_metrics},
                 {"benchmark statistics", af_test_benchmark_stats}};
    size_t passed = 0U;
    for (size_t index = 0U; index < sizeof(tests) / sizeof(tests[0]); ++index) {
        if (tests[index].function()) {
            ++passed;
            (void)printf("[PASS] %s\n", tests[index].name);
        } else {
            (void)printf("[FAIL] %s\n", tests[index].name);
        }
    }
    (void)printf("%zu/%zu tests passed\n", passed, sizeof(tests) / sizeof(tests[0]));
    return passed == sizeof(tests) / sizeof(tests[0]) ? 0 : 1;
}
