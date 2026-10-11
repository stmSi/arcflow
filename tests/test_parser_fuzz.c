#include "af/core/allocator.h"
#include "af/core/hash.h"
#include "assets/package.h"
#include "project/project.h"
#include "project/project_templates.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AF_FUZZ_PROJECT_CASES_PER_TEMPLATE 128U
#define AF_FUZZ_PACKAGE_CASES_PER_TEMPLATE 512U
#define AF_FUZZ_MUTATION_SLACK 32U
#define AF_FUZZ_SEED UINT64_C(0x415243464c4f5731)

typedef struct AfFuzzStats {
    uint64_t case_count;
    uint64_t accepted_count;
    uint64_t rejected_count;
    uint64_t decoded_count;
    uint64_t checksum_repaired_count;
} AfFuzzStats;

static uint64_t af_fuzz_next(uint64_t *state) {
    uint64_t value = *state;
    value ^= value >> 12U;
    value ^= value << 25U;
    value ^= value >> 27U;
    *state = value;
    return value * UINT64_C(2685821657736338717);
}

static size_t af_fuzz_min_size(size_t left, size_t right) { return left < right ? left : right; }

static void af_fuzz_mutate(const uint8_t *input, size_t size, uint8_t *output, size_t capacity,
                           uint32_t case_index, uint64_t *random_state, size_t *out_size) {
    const uint32_t mode = case_index % 8U;
    size_t position = 0U;
    size_t length = 0U;
    memcpy(output, input, size);
    *out_size = size;
    if (size == 0U) {
        return;
    }
    position = (size_t)(af_fuzz_next(random_state) % (uint64_t)size);
    switch (mode) {
    case 0U:
        output[position] ^= (uint8_t)(1U << (af_fuzz_next(random_state) & 7U));
        break;
    case 1U:
        output[position] = (uint8_t)af_fuzz_next(random_state);
        break;
    case 2U:
        *out_size = (size_t)(af_fuzz_next(random_state) % ((uint64_t)size + 1U));
        break;
    case 3U:
        length = 1U + (size_t)(af_fuzz_next(random_state) % 16U);
        length = af_fuzz_min_size(length, size - position);
        length = af_fuzz_min_size(length, capacity - size);
        memcpy(output + size, output + position, length);
        *out_size = size + length;
        break;
    case 4U:
        length = 1U + (size_t)(af_fuzz_next(random_state) % 16U);
        length = af_fuzz_min_size(length, size - position);
        memset(output + position, 0, length);
        break;
    case 5U:
        length = 1U + (size_t)(af_fuzz_next(random_state) % 16U);
        length = af_fuzz_min_size(length, size - position);
        memmove(output + position, output + position + length, size - position - length);
        *out_size = size - length;
        break;
    case 6U:
        length = af_fuzz_min_size(8U, size - position);
        memset(output + position, (case_index & 8U) == 0U ? 0xff : 0x80, length);
        break;
    case 7U:
        output[position] = (uint8_t)((case_index & 8U) == 0U ? '\0' : '\n');
        break;
    default:
        break;
    }
}

static uint32_t af_fuzz_package_checksum(const uint8_t *bytes, size_t size) {
    uint32_t hash = 2166136261U;
    for (size_t index = 0U; index < size; ++index) {
        hash ^= bytes[index];
        hash *= 16777619U;
    }
    return hash;
}

static void af_fuzz_write_u32(uint8_t *bytes, uint32_t value) {
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8U);
    bytes[2] = (uint8_t)(value >> 16U);
    bytes[3] = (uint8_t)(value >> 24U);
}

static void af_fuzz_write_u64(uint8_t *bytes, uint64_t value) {
    af_fuzz_write_u32(bytes, (uint32_t)value);
    af_fuzz_write_u32(bytes + 4U, (uint32_t)(value >> 32U));
}

static bool af_fuzz_result_expected(AfResult result) {
    return result == AF_RESULT_SUCCESS || result == AF_RESULT_INVALID_ARGUMENT ||
           result == AF_RESULT_LIMIT_EXCEEDED || result == AF_RESULT_INVALID_PACKAGE ||
           result == AF_RESULT_UNSUPPORTED_VERSION || result == AF_RESULT_UNSUPPORTED_FEATURE ||
           result == AF_RESULT_NOT_FOUND;
}

static bool af_fuzz_projects(const AfPackageBlob *text, AfAllocator *allocator, uint8_t *mutation,
                             size_t mutation_capacity, uint64_t *random_state, AfFuzzStats *stats) {
    for (uint32_t case_index = 0U; case_index < AF_FUZZ_PROJECT_CASES_PER_TEMPLATE; ++case_index) {
        AfProject parsed = {0};
        AfPackageBlob normalized = {0};
        size_t mutation_size = 0U;
        AfResult result = AF_RESULT_SUCCESS;
        af_fuzz_mutate(text->data, text->size, mutation, mutation_capacity, case_index,
                       random_state, &mutation_size);
        result = af_project_deserialize(mutation, mutation_size, allocator, &parsed);
        ++stats->case_count;
        if (!af_fuzz_result_expected(result)) {
            af_project_destroy(&parsed);
            return false;
        }
        if (result == AF_RESULT_SUCCESS) {
            ++stats->accepted_count;
            if (af_project_validate(&parsed) != AF_RESULT_SUCCESS ||
                af_project_serialize(&parsed, allocator, &normalized) != AF_RESULT_SUCCESS) {
                af_package_blob_destroy(allocator, &normalized);
                af_project_destroy(&parsed);
                return false;
            }
            ++stats->decoded_count;
        } else {
            ++stats->rejected_count;
        }
        af_package_blob_destroy(allocator, &normalized);
        af_project_destroy(&parsed);
    }
    return true;
}

static bool af_fuzz_packages(const AfPackageBlob *package, AfAllocator *allocator,
                             uint8_t *mutation, size_t mutation_capacity, uint64_t *random_state,
                             AfFuzzStats *stats) {
    for (uint32_t case_index = 0U; case_index < AF_FUZZ_PACKAGE_CASES_PER_TEMPLATE; ++case_index) {
        AfLoadedPackage decoded = {0};
        size_t mutation_size = 0U;
        AfResult result = AF_RESULT_SUCCESS;
        af_fuzz_mutate(package->data, package->size, mutation, mutation_capacity, case_index,
                       random_state, &mutation_size);
        if ((case_index & 1U) != 0U && mutation_size >= AF_PACKAGE_HEADER_SIZE) {
            af_fuzz_write_u64(mutation + 24U, (uint64_t)mutation_size);
            af_fuzz_write_u32(mutation + 48U,
                              af_fuzz_package_checksum(mutation + AF_PACKAGE_HEADER_SIZE,
                                                       mutation_size - AF_PACKAGE_HEADER_SIZE));
            ++stats->checksum_repaired_count;
        }
        result = af_package_validate(mutation, mutation_size);
        ++stats->case_count;
        if (!af_fuzz_result_expected(result)) {
            return false;
        }
        if (result == AF_RESULT_SUCCESS) {
            ++stats->accepted_count;
            result = af_package_decode(mutation, mutation_size, allocator, &decoded);
            if (result != AF_RESULT_SUCCESS) {
                af_loaded_package_destroy(allocator, &decoded);
                return false;
            }
            ++stats->decoded_count;
        } else {
            ++stats->rejected_count;
        }
        af_loaded_package_destroy(allocator, &decoded);
    }
    return true;
}

int main(void) {
    AfAllocator allocator = {0};
    AfFuzzStats project_stats = {0};
    AfFuzzStats package_stats = {0};
    uint64_t random_state = AF_FUZZ_SEED;
    uint64_t workload_hash = 0U;
    bool passed = true;
    af_allocator_init_default(&allocator);

    for (uint32_t template_index = 0U; passed && template_index < af_project_template_count();
         ++template_index) {
        AfProject project = {0};
        AfPackageBlob text = {0};
        AfPackageBlob package = {0};
        uint8_t *project_mutation = NULL;
        uint8_t *package_mutation = NULL;
        AfResult result = af_project_template_build((AfProjectTemplateId)template_index, &allocator,
                                                    64U, &project);
        if (result == AF_RESULT_SUCCESS) {
            result = af_project_serialize(&project, &allocator, &text);
        }
        if (result == AF_RESULT_SUCCESS) {
            result = af_project_compile_release(&project, &allocator, &package);
        }
        if (result != AF_RESULT_SUCCESS ||
            af_package_validate(package.data, package.size) != AF_RESULT_SUCCESS) {
            passed = false;
        }
        if (passed) {
            project_mutation = malloc(text.size + AF_FUZZ_MUTATION_SLACK);
            package_mutation = malloc(package.size + AF_FUZZ_MUTATION_SLACK);
            passed = project_mutation != NULL && package_mutation != NULL;
        }
        if (passed) {
            workload_hash ^= af_hash_bytes(text.data, text.size) + UINT64_C(0x9e3779b97f4a7c15) +
                             (workload_hash << 6U) + (workload_hash >> 2U);
            workload_hash ^= af_hash_bytes(package.data, package.size) +
                             UINT64_C(0x9e3779b97f4a7c15) + (workload_hash << 6U) +
                             (workload_hash >> 2U);
            passed = af_fuzz_projects(&text, &allocator, project_mutation,
                                      text.size + AF_FUZZ_MUTATION_SLACK, &random_state,
                                      &project_stats) &&
                     af_fuzz_packages(&package, &allocator, package_mutation,
                                      package.size + AF_FUZZ_MUTATION_SLACK, &random_state,
                                      &package_stats);
        }
        free(package_mutation);
        free(project_mutation);
        af_package_blob_destroy(&allocator, &package);
        af_package_blob_destroy(&allocator, &text);
        af_project_destroy(&project);
    }

    {
        const uint64_t required_project_cases =
            (uint64_t)af_project_template_count() * AF_FUZZ_PROJECT_CASES_PER_TEMPLATE;
        const uint64_t required_package_cases =
            (uint64_t)af_project_template_count() * AF_FUZZ_PACKAGE_CASES_PER_TEMPLATE;
        passed = passed && af_project_template_count() == AF_PROJECT_TEMPLATE_COUNT &&
                 project_stats.case_count >= required_project_cases &&
                 package_stats.case_count >= required_package_cases &&
                 project_stats.rejected_count != 0U && package_stats.rejected_count != 0U &&
                 package_stats.checksum_repaired_count >= required_package_cases / 3U;
        (void)fprintf(stdout,
                      "{\"seed\":\"%016llx\",\"workload_hash\":\"%016llx\","
                      "\"templates\":%u,\"project_cases\":%llu,\"required_project_cases\":%llu,"
                      "\"project_accepted\":%llu,\"project_rejected\":%llu,"
                      "\"package_cases\":%llu,\"required_package_cases\":%llu,"
                      "\"package_accepted\":%llu,\"package_rejected\":%llu,"
                      "\"package_checksum_repaired\":%llu,\"passed\":%s}\n",
                      (unsigned long long)AF_FUZZ_SEED, (unsigned long long)workload_hash,
                      af_project_template_count(), (unsigned long long)project_stats.case_count,
                      (unsigned long long)required_project_cases,
                      (unsigned long long)project_stats.accepted_count,
                      (unsigned long long)project_stats.rejected_count,
                      (unsigned long long)package_stats.case_count,
                      (unsigned long long)required_package_cases,
                      (unsigned long long)package_stats.accepted_count,
                      (unsigned long long)package_stats.rejected_count,
                      (unsigned long long)package_stats.checksum_repaired_count,
                      passed ? "true" : "false");
    }
    return passed ? 0 : 1;
}
