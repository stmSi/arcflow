#ifndef AF_UPDATE_H
#define AF_UPDATE_H

#include "arcflow/arcflow_result.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AF_UPDATE_URL_CAPACITY 512U
#define AF_UPDATE_SIGNATURE_CAPACITY 256U
#define AF_UPDATE_MANIFEST_MAX_SIZE 8192U

typedef enum AfReleaseChannel {
    AF_RELEASE_CHANNEL_DEVELOPMENT = 0,
    AF_RELEASE_CHANNEL_PREVIEW,
    AF_RELEASE_CHANNEL_BETA,
    AF_RELEASE_CHANNEL_STABLE
} AfReleaseChannel;

typedef struct AfSemanticVersion {
    uint32_t major;
    uint32_t minor;
    uint32_t patch;
} AfSemanticVersion;

typedef struct AfUpdateManifest {
    AfSemanticVersion version;
    AfReleaseChannel channel;
    uint32_t minimum_runtime_version;
    uint32_t minimum_package_version;
    uint64_t artifact_size;
    uint64_t content_hash;
    char url[AF_UPDATE_URL_CAPACITY];
    char signature[AF_UPDATE_SIGNATURE_CAPACITY];
} AfUpdateManifest;

typedef enum AfUpdateStatus {
    AF_UPDATE_STATUS_CURRENT = 0,
    AF_UPDATE_STATUS_AVAILABLE,
    AF_UPDATE_STATUS_CHANNEL_BLOCKED,
    AF_UPDATE_STATUS_INCOMPATIBLE
} AfUpdateStatus;

typedef bool (*AfUpdateSignatureVerifier)(const void *data, size_t size, const char *signature,
                                          void *user);

AfResult af_semantic_version_parse(const char *text, AfSemanticVersion *out_version);
int af_semantic_version_compare(AfSemanticVersion left, AfSemanticVersion right);
const char *af_release_channel_name(AfReleaseChannel channel);
AfResult af_release_channel_parse(const char *text, AfReleaseChannel *out_channel);
AfResult af_update_manifest_parse(const void *data, size_t size, AfUpdateManifest *out_manifest);
AfUpdateStatus af_update_check(const AfUpdateManifest *manifest, AfSemanticVersion current,
                               AfReleaseChannel subscribed_channel, uint32_t runtime_version,
                               uint32_t package_version);
AfResult af_update_stage_artifact(const AfUpdateManifest *manifest, const void *data, size_t size,
                                  AfUpdateSignatureVerifier verifier, void *verifier_user,
                                  const char *staging_path);

#endif
