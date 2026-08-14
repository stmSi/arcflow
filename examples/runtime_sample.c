#include "arcflow/arcflow_runtime.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define ARCFLOW_SAMPLE_MAX_PACKAGE_SIZE (512U * 1024U * 1024U)

static unsigned char *arcflow_sample_read(const char *path, size_t *out_size) {
    FILE *file = NULL;
    unsigned char *data = NULL;
    long length = 0L;
    int failed = 0;
    if (path == NULL || out_size == NULL) {
        return NULL;
    }
    *out_size = 0U;
    file = fopen(path, "rb");
    if (file == NULL || fseek(file, 0L, SEEK_END) != 0) {
        if (file != NULL) {
            (void)fclose(file);
        }
        return NULL;
    }
    length = ftell(file);
    if (length <= 0L || (unsigned long)length > ARCFLOW_SAMPLE_MAX_PACKAGE_SIZE ||
        fseek(file, 0L, SEEK_SET) != 0) {
        (void)fclose(file);
        return NULL;
    }
    data = malloc((size_t)length);
    failed = data == NULL || fread(data, 1U, (size_t)length, file) != (size_t)length;
    if (fclose(file) != 0) {
        failed = 1;
    }
    if (failed != 0) {
        free(data);
        return NULL;
    }
    *out_size = (size_t)length;
    return data;
}

int main(int argument_count, char **arguments) {
    AfRuntimeCreateInfo create_info = {.frames_in_flight = 2U};
    AfRuntime *runtime = NULL;
    AfPackageHandle package = 0U;
    AfInstanceHandle instance = 0U;
    AfRuntimeEvent event = {0};
    unsigned char *data = NULL;
    size_t size = 0U;
    uint32_t event_count = 0U;
    AfResult result = AF_RESULT_SUCCESS;
    if (argument_count != 2) {
        (void)fprintf(stderr, "usage: arcflow_runtime_sample INPUT.afp\n");
        return 2;
    }
    data = arcflow_sample_read(arguments[1], &size);
    if (data == NULL) {
        (void)fprintf(stderr, "cannot read package: %s\n", arguments[1]);
        return 1;
    }
    result = af_runtime_create(&create_info, &runtime);
    if (result == AF_RESULT_SUCCESS) {
        result = af_package_load(runtime, data, size, &package);
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_instance_create(runtime, package, &instance);
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_instance_play(runtime, instance);
    }
    for (uint32_t frame = 0U; result == AF_RESULT_SUCCESS && frame < 120U; ++frame) {
        result = af_instance_update(runtime, instance, 1.0F / 60.0F);
    }
    while (result == AF_RESULT_SUCCESS &&
           af_instance_poll_event(runtime, instance, &event) == AF_RESULT_SUCCESS) {
        ++event_count;
    }
    if (instance != 0U) {
        const AfResult destroy_result = af_instance_destroy(runtime, instance);
        result = result == AF_RESULT_SUCCESS ? destroy_result : result;
    }
    if (package != 0U) {
        const AfResult destroy_result = af_package_destroy(runtime, package);
        result = result == AF_RESULT_SUCCESS ? destroy_result : result;
    }
    af_runtime_destroy(runtime);
    free(data);
    if (result != AF_RESULT_SUCCESS) {
        (void)fprintf(stderr, "runtime sample failed with result %d\n", (int)result);
        return 1;
    }
    (void)printf("runtime sample completed: 120 frames, %u events\n", event_count);
    return 0;
}
