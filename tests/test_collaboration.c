#include "core/file_io.h"
#include "project/project_collaboration.h"

#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            (void)fprintf(stderr, "check failed: %s:%d: %s\n", __FILE__, __LINE__, #condition);    \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static unsigned long af_test_process_id(void) {
#if defined(_WIN32)
    return (unsigned long)_getpid();
#else
    return (unsigned long)getpid();
#endif
}

int main(void) {
    static const char conflict_text[] =
        "arcflow_project 11\n<<<<<<< ours\ncanvas 1 1\n=======\ncanvas 2 2\n>>>>>>> theirs\n";
    static const char first_data[] = "first";
    static const char second_data[] = "second";
    static const char cr_conflict_text[] = "header\r<<<<<<< ours\r=======\r>>>>>>> theirs\r";
    AfProjectMetadata metadata = {0};
    AfProjectConflictReport conflicts = {0};
    AfProjectFingerprint first = {0};
    AfProjectFingerprint second = {0};
    AfProjectLock lock = {0};
    AfProjectLock duplicate = {0};
    char path[256];
    char lock_path[288];
    char owner[AF_PROJECT_LOCK_OWNER_CAPACITY];
    char existing_owner[AF_PROJECT_LOCK_OWNER_CAPACITY];
    (void)snprintf(path, sizeof(path), "arcflow_collaboration_test_%lu.afproj",
                   af_test_process_id());
    (void)snprintf(lock_path, sizeof(lock_path), "%s.aflock", path);
    (void)snprintf(owner, sizeof(owner), "test-%lu", af_test_process_id());
    (void)remove(path);
    (void)remove(lock_path);

    af_project_metadata_default(&metadata);
    CHECK(af_project_metadata_validate(&metadata) == AF_RESULT_SUCCESS);
    CHECK(af_project_metadata_record(&metadata, "alice", "shape-update") == AF_RESULT_SUCCESS);
    CHECK(metadata.revision == 1U && metadata.change_count == 1U);
    CHECK(af_project_metadata_record(&metadata, "invalid author", "update") ==
          AF_RESULT_INVALID_ARGUMENT);

    CHECK(af_project_conflicts_scan(conflict_text, sizeof(conflict_text) - 1U, &conflicts) ==
          AF_RESULT_INVALID_PACKAGE);
    CHECK(conflicts.count == 3U && conflicts.entries[0].line == 2U &&
          conflicts.entries[1].line == 4U && conflicts.entries[2].line == 6U);
    CHECK(af_project_conflicts_scan(first_data, sizeof(first_data) - 1U, &conflicts) ==
          AF_RESULT_SUCCESS);
    CHECK(af_project_conflicts_scan(cr_conflict_text, sizeof(cr_conflict_text) - 1U, &conflicts) ==
          AF_RESULT_INVALID_PACKAGE);
    CHECK(conflicts.count == 3U && conflicts.entries[0].line == 2U &&
          conflicts.entries[1].line == 3U && conflicts.entries[2].line == 4U);

    CHECK(af_file_write_atomic(path, first_data, sizeof(first_data) - 1U, 0U) == AF_RESULT_SUCCESS);
    CHECK(af_project_fingerprint_file(path, 128U, &first) == AF_RESULT_SUCCESS && first.exists);
    CHECK(af_file_write_atomic(path, second_data, sizeof(second_data) - 1U, 0U) ==
          AF_RESULT_SUCCESS);
    CHECK(af_project_fingerprint_file(path, 128U, &second) == AF_RESULT_SUCCESS &&
          af_project_fingerprint_changed(&first, &second));

    CHECK(af_project_lock_acquire(path, owner, &lock, existing_owner, sizeof(existing_owner)) ==
          AF_RESULT_SUCCESS);
    CHECK(af_project_lock_query(path, existing_owner, sizeof(existing_owner)) ==
              AF_RESULT_SUCCESS &&
          strcmp(existing_owner, owner) == 0);
    CHECK(af_project_lock_query(path, existing_owner, 2U) == AF_RESULT_INVALID_PACKAGE &&
          existing_owner[0] == '\0');
    CHECK(af_project_lock_acquire(path, "competitor", &duplicate, existing_owner,
                                  sizeof(existing_owner)) == AF_RESULT_STALE_HANDLE);
    CHECK(strcmp(existing_owner, owner) == 0);
    CHECK(af_project_lock_release(&lock) == AF_RESULT_SUCCESS);
    CHECK(af_project_lock_query(path, existing_owner, sizeof(existing_owner)) ==
          AF_RESULT_NOT_FOUND);
    duplicate = (AfProjectLock){.token = 7U, .acquired = true};
    CHECK(af_project_lock_acquire("", owner, &duplicate, existing_owner, sizeof(existing_owner)) ==
              AF_RESULT_INVALID_ARGUMENT &&
          !duplicate.acquired && duplicate.path[0] == '\0');

    CHECK(remove(path) == 0);
    return 0;
}
