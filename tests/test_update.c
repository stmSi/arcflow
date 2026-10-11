#include "af/core/hash.h"
#include "arcflow/arcflow_build_info.h"
#include "arcflow/arcflow_version.h"
#include "update/update.h"

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

static bool af_test_verify(const void *data, size_t size, const char *signature, void *user) {
    (void)data;
    (void)size;
    (void)user;
    return strcmp(signature, "test-signature") == 0;
}

int main(void) {
    static const char artifact[] = "signed release artifact";
    AfSemanticVersion current = {0};
    AfSemanticVersion newer = {0};
    AfUpdateManifest manifest = {0};
    char text[2048];
    char staging_path[256];
    const uint64_t hash = af_hash_bytes(artifact, sizeof(artifact) - 1U);
#if defined(_WIN32)
    const unsigned long process_id = (unsigned long)_getpid();
#else
    const unsigned long process_id = (unsigned long)getpid();
#endif
    const int written =
        snprintf(text, sizeof(text),
                 "arcflow_update 1\nversion 1.1.0\nchannel Beta\nruntime_min %u\npackage_min "
                 "%u\nsize %zu\nhash %016llx\nurl https://updates.example/arcflow.zip\n"
                 "signature test-signature\n",
                 AF_RUNTIME_VERSION, AF_PACKAGE_FORMAT_VERSION, sizeof(artifact) - 1U,
                 (unsigned long long)hash);
    CHECK(strcmp(AF_VERSION_STRING, "1.0.0") == 0 && AF_BUILD_ID[0] != '\0');
    CHECK(af_semantic_version_parse(AF_VERSION_STRING, &current) == AF_RESULT_SUCCESS);
    CHECK(af_semantic_version_parse("01.1.0", &current) == AF_RESULT_INVALID_ARGUMENT);
    CHECK(af_semantic_version_parse("+1.1.0", &current) == AF_RESULT_INVALID_ARGUMENT);
    CHECK(af_semantic_version_parse("1.1.0", &newer) == AF_RESULT_SUCCESS);
    CHECK(af_semantic_version_compare(current, newer) < 0);
    CHECK(written > 0 && (size_t)written < sizeof(text));
    CHECK(af_update_manifest_parse(text, (size_t)written, &manifest) == AF_RESULT_SUCCESS);
    {
        char malformed[2048];
        char *hash_field = NULL;
        (void)snprintf(malformed, sizeof(malformed), "%s", text);
        hash_field = strstr(malformed, "hash ");
        CHECK(hash_field != NULL);
        hash_field[5] = 'g';
        CHECK(af_update_manifest_parse(malformed, strlen(malformed), &manifest) ==
              AF_RESULT_INVALID_PACKAGE);
    }
    CHECK(af_update_check(&manifest, current, AF_RELEASE_CHANNEL_BETA, AF_RUNTIME_VERSION,
                          AF_PACKAGE_FORMAT_VERSION) == AF_UPDATE_STATUS_AVAILABLE);
    CHECK(af_update_check(&manifest, newer, AF_RELEASE_CHANNEL_STABLE, AF_RUNTIME_VERSION,
                          AF_PACKAGE_FORMAT_VERSION) == AF_UPDATE_STATUS_CHANNEL_BLOCKED);
    CHECK(af_update_check(&manifest, current, AF_RELEASE_CHANNEL_BETA, AF_RUNTIME_VERSION - 1U,
                          AF_PACKAGE_FORMAT_VERSION) == AF_UPDATE_STATUS_INCOMPATIBLE);
    (void)snprintf(staging_path, sizeof(staging_path), "arcflow_update_test_%lu.stage", process_id);
    (void)remove(staging_path);
    CHECK(af_update_stage_artifact(&manifest, artifact, sizeof(artifact) - 1U, af_test_verify, NULL,
                                   staging_path) == AF_RESULT_SUCCESS);
    CHECK(remove(staging_path) == 0);
    (void)snprintf(manifest.signature, sizeof(manifest.signature), "invalid-signature");
    CHECK(af_update_stage_artifact(&manifest, artifact, sizeof(artifact) - 1U, af_test_verify, NULL,
                                   staging_path) == AF_RESULT_INVALID_PACKAGE);
    (void)snprintf(manifest.signature, sizeof(manifest.signature), "test-signature");
    manifest.content_hash ^= 1U;
    CHECK(af_update_stage_artifact(&manifest, artifact, sizeof(artifact) - 1U, af_test_verify, NULL,
                                   staging_path) == AF_RESULT_INVALID_ARGUMENT);
    return 0;
}
