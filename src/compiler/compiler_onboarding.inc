static AfResult af_compiler_directory_create(const char *path) {
    int result = 0;
    if (path == NULL || path[0] == '\0') {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    errno = 0;
#if defined(_WIN32)
    result = _mkdir(path);
#else
    result = mkdir(path, 0777);
#endif
    return result == 0 || errno == EEXIST ? AF_RESULT_SUCCESS : AF_RESULT_IO_ERROR;
}

static AfResult af_compiler_template_write(const AfProjectTemplateInfo *info, const char *path) {
    AfAllocator allocator = {0};
    AfProject project = {0};
    AfResult result = AF_RESULT_INVALID_ARGUMENT;
    if (info == NULL || path == NULL || path[0] == '\0') {
        return result;
    }
    af_allocator_init_default(&allocator);
    result = af_project_template_build(info->id, &allocator, 64U, &project);
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_save_file(&project, path, &allocator);
    }
    af_project_destroy(&project);
    return result;
}

static AfResult af_compiler_examples_generate(const char *directory) {
    AfResult result = af_compiler_directory_create(directory);
    for (uint32_t index = 0U; result == AF_RESULT_SUCCESS && index < af_project_template_count();
         ++index) {
        const AfProjectTemplateInfo *info = af_project_template_info((AfProjectTemplateId)index);
        AfAllocator allocator = {0};
        AfProject project = {0};
        AfPackageBlob package = {0};
        char project_path[1024];
        char package_path[1024];
        const int project_length =
            snprintf(project_path, sizeof(project_path), "%s/%s.afproj", directory, info->slug);
        const int package_length =
            snprintf(package_path, sizeof(package_path), "%s/%s.afp", directory, info->slug);
        if (project_length < 0 || (size_t)project_length >= sizeof(project_path) ||
            package_length < 0 || (size_t)package_length >= sizeof(package_path)) {
            result = AF_RESULT_LIMIT_EXCEEDED;
            break;
        }
        af_allocator_init_default(&allocator);
        result = af_project_template_build(info->id, &allocator, 64U, &project);
        if (result == AF_RESULT_SUCCESS) {
            result = af_project_save_file(&project, project_path, &allocator);
        }
        if (result == AF_RESULT_SUCCESS) {
            result = af_project_compile_release(&project, &allocator, &package);
        }
        if (result == AF_RESULT_SUCCESS) {
            result = af_file_write_atomic(package_path, package.data, package.size, 1U);
        }
        af_package_blob_destroy(&allocator, &package);
        af_project_destroy(&project);
        if (result == AF_RESULT_SUCCESS) {
            (void)fprintf(stdout, "%s\n", project_path);
            (void)fprintf(stdout, "%s\n", package_path);
        }
    }
    return result;
}

static void af_compiler_templates_print(void) {
    (void)fprintf(stdout, "Arcflow %s project templates:\n", AF_VERSION_STRING);
    for (uint32_t index = 0U; index < af_project_template_count(); ++index) {
        const AfProjectTemplateInfo *info = af_project_template_info((AfProjectTemplateId)index);
        (void)fprintf(stdout, "  %-20s %-22s %s\n", info->slug, info->name, info->description);
    }
}

static void af_compiler_guides_print(void) {
    (void)fprintf(stdout, "Arcflow %s in-product guide topics:\n", AF_VERSION_STRING);
    for (uint32_t index = 0U; index < af_onboarding_guide_count(); ++index) {
        const AfOnboardingGuide *guide = af_onboarding_guide((AfOnboardingTopic)index);
        (void)fprintf(stdout, "  %-22s %s\n", guide->slug, guide->title);
    }
}

static AfResult af_compiler_guide_print(const char *slug) {
    const AfOnboardingGuide *guide = af_onboarding_guide_find(slug);
    if (guide == NULL) {
        (void)fprintf(stderr, "unknown guide topic '%s'; use --list-guides\n",
                      slug == NULL ? "" : slug);
        return AF_RESULT_NOT_FOUND;
    }
    (void)fprintf(stdout,
                  "Arcflow %s | %s channel | project format %u | package format %u | runtime "
                  "ABI %u\n\n%s\n%s\n",
                  AF_VERSION_STRING, AF_RELEASE_CHANNEL, AF_PROJECT_FORMAT_VERSION,
                  AF_PACKAGE_FORMAT_VERSION, AF_RUNTIME_VERSION, guide->title, guide->summary);
    for (uint32_t index = 0U; index < guide->step_count; ++index) {
        (void)fprintf(stdout, "%u. %s\n", index + 1U, guide->steps[index]);
    }
    return AF_RESULT_SUCCESS;
}
