#include "update/update.h"

#include "af/core/file_io.h"
#include "af/core/hash.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool af_update_token_valid(const char *text, size_t capacity) {
    size_t length = 0U;
    if (text == NULL || text[0] == '\0') {
        return false;
    }
    while (length < capacity && text[length] != '\0') {
        const unsigned char byte = (unsigned char)text[length];
        if (byte <= 32U || byte >= 127U) {
            return false;
        }
        ++length;
    }
    return length != capacity;
}

static bool af_update_hash_valid(const char *text) {
    if (text == NULL || strlen(text) != 16U) {
        return false;
    }
    for (size_t index = 0U; index < 16U; ++index) {
        const unsigned char byte = (unsigned char)text[index];
        const bool decimal = byte >= '0' && byte <= '9';
        const bool lower = byte >= 'a' && byte <= 'f';
        const bool upper = byte >= 'A' && byte <= 'F';
        if (!(decimal || lower || upper)) {
            return false;
        }
    }
    return true;
}

AfResult af_semantic_version_parse(const char *text, AfSemanticVersion *out_version) {
    unsigned long values[3] = {0UL, 0UL, 0UL};
    const char *cursor = text;
    char *end = NULL;
    if (text == NULL || out_version == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < 3U; ++index) {
        if (*cursor < '0' || *cursor > '9' ||
            (*cursor == '0' && cursor[1] >= '0' && cursor[1] <= '9')) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        errno = 0;
        values[index] = strtoul(cursor, &end, 10);
        if (errno != 0 || end == cursor || values[index] > UINT32_MAX ||
            (index < 2U && *end != '.') || (index == 2U && *end != '\0')) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        cursor = end + 1;
    }
    *out_version =
        (AfSemanticVersion){(uint32_t)values[0], (uint32_t)values[1], (uint32_t)values[2]};
    return AF_RESULT_SUCCESS;
}

int af_semantic_version_compare(AfSemanticVersion left, AfSemanticVersion right) {
    if (left.major != right.major) {
        return left.major < right.major ? -1 : 1;
    }
    if (left.minor != right.minor) {
        return left.minor < right.minor ? -1 : 1;
    }
    if (left.patch != right.patch) {
        return left.patch < right.patch ? -1 : 1;
    }
    return 0;
}

const char *af_release_channel_name(AfReleaseChannel channel) {
    static const char *const names[] = {"Development", "Preview", "Beta", "Stable"};
    return (uint32_t)channel <= (uint32_t)AF_RELEASE_CHANNEL_STABLE ? names[channel] : "Unknown";
}

AfResult af_release_channel_parse(const char *text, AfReleaseChannel *out_channel) {
    if (text == NULL || out_channel == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index <= (uint32_t)AF_RELEASE_CHANNEL_STABLE; ++index) {
        const char *name = af_release_channel_name((AfReleaseChannel)index);
        if (strcmp(text, name) == 0) {
            *out_channel = (AfReleaseChannel)index;
            return AF_RESULT_SUCCESS;
        }
    }
    return AF_RESULT_INVALID_ARGUMENT;
}

AfResult af_update_manifest_parse(const void *data, size_t size, AfUpdateManifest *out_manifest) {
    char text[AF_UPDATE_MANIFEST_MAX_SIZE + 1U];
    char version[64];
    char channel[32];
    char hash[32];
    char trailing = '\0';
    unsigned int runtime_version = 0U;
    unsigned int package_version = 0U;
    unsigned long long artifact_size = 0ULL;
    unsigned long long parsed_hash = 0ULL;
    AfUpdateManifest manifest = {0};
    int matched = 0;
    if (data == NULL || size == 0U || size > AF_UPDATE_MANIFEST_MAX_SIZE || out_manifest == NULL ||
        memchr(data, '\0', size) != NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    memcpy(text, data, size);
    text[size] = '\0';
    matched = sscanf(text,
                     "arcflow_update 1\nversion %63s\nchannel %31s\nruntime_min %u\n"
                     "package_min %u\nsize %llu\nhash %31s\nurl %511s\nsignature %255s\n%c",
                     version, channel, &runtime_version, &package_version, &artifact_size, hash,
                     manifest.url, manifest.signature, &trailing);
    if (matched != 8 ||
        af_semantic_version_parse(version, &manifest.version) != AF_RESULT_SUCCESS ||
        af_release_channel_parse(channel, &manifest.channel) != AF_RESULT_SUCCESS ||
        !af_update_hash_valid(hash) || sscanf(hash, "%llx%c", &parsed_hash, &trailing) != 1 ||
        !af_update_token_valid(manifest.url, sizeof(manifest.url)) ||
        !af_update_token_valid(manifest.signature, sizeof(manifest.signature))) {
        return AF_RESULT_INVALID_PACKAGE;
    }
    manifest.minimum_runtime_version = runtime_version;
    manifest.minimum_package_version = package_version;
    manifest.artifact_size = (uint64_t)artifact_size;
    manifest.content_hash = (uint64_t)parsed_hash;
    if (manifest.artifact_size == 0U || manifest.content_hash == 0U) {
        return AF_RESULT_INVALID_PACKAGE;
    }
    *out_manifest = manifest;
    return AF_RESULT_SUCCESS;
}

AfUpdateStatus af_update_check(const AfUpdateManifest *manifest, AfSemanticVersion current,
                               AfReleaseChannel subscribed_channel, uint32_t runtime_version,
                               uint32_t package_version) {
    if (manifest == NULL || (uint32_t)subscribed_channel > (uint32_t)AF_RELEASE_CHANNEL_STABLE ||
        manifest->minimum_runtime_version > runtime_version ||
        manifest->minimum_package_version > package_version) {
        return AF_UPDATE_STATUS_INCOMPATIBLE;
    }
    if ((uint32_t)manifest->channel < (uint32_t)subscribed_channel) {
        return AF_UPDATE_STATUS_CHANNEL_BLOCKED;
    }
    return af_semantic_version_compare(current, manifest->version) < 0 ? AF_UPDATE_STATUS_AVAILABLE
                                                                       : AF_UPDATE_STATUS_CURRENT;
}

AfResult af_update_stage_artifact(const AfUpdateManifest *manifest, const void *data, size_t size,
                                  AfUpdateSignatureVerifier verifier, void *verifier_user,
                                  const char *staging_path) {
    if (manifest == NULL || data == NULL || size == 0U || staging_path == NULL ||
        staging_path[0] == '\0' || verifier == NULL || manifest->artifact_size != (uint64_t)size ||
        manifest->content_hash != af_hash_bytes(data, size)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (!verifier(data, size, manifest->signature, verifier_user)) {
        return AF_RESULT_INVALID_PACKAGE;
    }
    return af_file_write_atomic(staging_path, data, size, 1U);
}
