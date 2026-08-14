#if defined(_WIN32)
#include <direct.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

static int af_test_create_directory(const char *path) {
#if defined(_WIN32)
    return _mkdir(path);
#else
    return mkdir(path, 0700);
#endif
}

static int af_test_remove_directory(const char *path) {
#if defined(_WIN32)
    return _rmdir(path);
#else
    return rmdir(path);
#endif
}

static bool af_test_file_equals(const char *path, const void *expected, size_t expected_size) {
    AfAllocator allocator;
    AfFileData file = {0};
    bool equal = false;
    af_allocator_init_default(&allocator);
    if (af_file_read_limited(path, 1024U, &allocator, &file) == AF_RESULT_SUCCESS) {
        equal = file.size == expected_size &&
                (expected_size == 0U || memcmp(file.data, expected, expected_size) == 0);
    }
    af_file_data_destroy(&allocator, &file);
    return equal;
}

static bool af_test_file_missing(const char *path) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return true;
    }
    (void)fclose(file);
    return false;
}

static bool af_test_atomic_file_io(void) {
    static const char path[] = "arcflow_test_atomic_file.bin";
    static const char direct_path[] = "arcflow_test_direct_file.bin";
    static const char failure_path[] = "arcflow_test_atomic_failure.bin";
    static const char failure_backup[] = "arcflow_test_atomic_failure.bin.bak1";
    static const char failure_guard[] = "arcflow_test_atomic_failure.bin.bak1/guard";
    static const char first[] = "first";
    static const char second[] = "second";
    static const char third[] = "third";
    static const char fourth[] = "fourth";
    (void)remove(path);
    (void)remove("arcflow_test_atomic_file.bin.bak1");
    (void)remove("arcflow_test_atomic_file.bin.bak2");
    (void)remove(direct_path);
    (void)remove(failure_guard);
    (void)af_test_remove_directory(failure_backup);
    (void)remove(failure_path);

    CHECK(af_file_write_atomic(path, first, sizeof(first) - 1U, 2U) == AF_RESULT_SUCCESS);
    CHECK(af_test_file_equals(path, first, sizeof(first) - 1U));
    CHECK(af_test_file_missing("arcflow_test_atomic_file.bin.bak1"));
    CHECK(af_file_write_atomic(path, second, sizeof(second) - 1U, 2U) == AF_RESULT_SUCCESS);
    CHECK(af_test_file_equals(path, second, sizeof(second) - 1U));
    CHECK(af_test_file_equals("arcflow_test_atomic_file.bin.bak1", first, sizeof(first) - 1U));
    CHECK(af_file_write_atomic(path, third, sizeof(third) - 1U, 2U) == AF_RESULT_SUCCESS);
    CHECK(af_test_file_equals(path, third, sizeof(third) - 1U));
    CHECK(af_test_file_equals("arcflow_test_atomic_file.bin.bak1", second, sizeof(second) - 1U));
    CHECK(af_test_file_equals("arcflow_test_atomic_file.bin.bak2", first, sizeof(first) - 1U));
    CHECK(af_file_write_atomic(path, fourth, sizeof(fourth) - 1U, 2U) == AF_RESULT_SUCCESS);
    CHECK(af_test_file_equals(path, fourth, sizeof(fourth) - 1U));
    CHECK(af_test_file_equals("arcflow_test_atomic_file.bin.bak1", third, sizeof(third) - 1U));
    CHECK(af_test_file_equals("arcflow_test_atomic_file.bin.bak2", second, sizeof(second) - 1U));
    CHECK(af_file_write_atomic(path, first, sizeof(first) - 1U, AF_FILE_MAX_BACKUPS + 1U) ==
          AF_RESULT_INVALID_ARGUMENT);
    CHECK(af_test_file_equals(path, fourth, sizeof(fourth) - 1U));
    CHECK(af_file_write_atomic(path, NULL, 1U, 0U) == AF_RESULT_INVALID_ARGUMENT);

    CHECK(af_file_write(direct_path, second, sizeof(second) - 1U) == AF_RESULT_SUCCESS);
    CHECK(af_test_file_equals(direct_path, second, sizeof(second) - 1U));

    CHECK(af_file_write(failure_path, first, sizeof(first) - 1U) == AF_RESULT_SUCCESS);
    CHECK(af_test_create_directory(failure_backup) == 0);
    CHECK(af_file_write(failure_guard, second, sizeof(second) - 1U) == AF_RESULT_SUCCESS);
    CHECK(af_file_write_atomic(failure_path, fourth, sizeof(fourth) - 1U, 1U) ==
          AF_RESULT_IO_ERROR);
    CHECK(af_test_file_equals(failure_path, first, sizeof(first) - 1U));
    CHECK(af_test_file_equals(failure_guard, second, sizeof(second) - 1U));

    CHECK(remove(failure_guard) == 0);
    CHECK(af_test_remove_directory(failure_backup) == 0);
    CHECK(remove(failure_path) == 0);
    CHECK(remove(direct_path) == 0);
    CHECK(remove("arcflow_test_atomic_file.bin.bak2") == 0);
    CHECK(remove("arcflow_test_atomic_file.bin.bak1") == 0);
    CHECK(remove(path) == 0);
    return true;
}
