#include "onboarding/onboarding.h"

#include <string.h>

static const AfOnboardingGuide af_onboarding_guides[AF_ONBOARDING_TOPIC_COUNT] = {
    [AF_ONBOARDING_INSTALLATION] =
        {.slug = "installation",
         .title = "Installation and portable use",
         .summary = "Install a packaged build or run the portable directory in place.",
         .steps = {"Extract the archive or run the platform installer.",
                   "Keep bin/shaders, bin/fonts, and bin/assets beside the executables.",
                   "Launch arcflow_editor; no global dependency installation is required.",
                   "Use arcflow_compiler --version to verify channel and format versions.",
                   "Projects and user settings live outside the application install tree."},
         .step_count = 5U},
    [AF_ONBOARDING_QUICK_START] =
        {.slug = "quick-start",
         .title = "Create, animate, compile, and play",
         .summary = "The shortest complete Arcflow authoring workflow.",
         .steps = {"Choose File > New From Template and select Guided Basics.",
                   "Select an object in Scene Hierarchy; Shift adds and Ctrl toggles selection.",
                   "Move the playhead, change a property, and press K to add a keyframe.",
                   "Press Space to preview and use the curve editor for easing.",
                   "Save with Ctrl+Shift+S, then press F5 to compile the runtime package.",
                   "Open the resulting .afp with arcflow_player."},
         .step_count = 6U},
    [AF_ONBOARDING_VECTOR_ANIMATION] =
        {.slug = "vector-animation",
         .title = "Animate vector points and Bezier handles",
         .summary = "Every editable path point and cubic handle can have its own track.",
         .steps = {"Create a Path or open the Vector Animation example.",
                   "Select a point; choose Anchor, Handle In, or Handle Out in the timeline.",
                   "Move the playhead and press K before and after changing the selected handle.",
                   "Use Curve Editor to tune interpolation without changing path topology.",
                   "Preview with Space; point and handle tracks evaluate together.",
                   "Remove point tracks before adding or deleting path commands."},
         .step_count = 6U},
    [AF_ONBOARDING_INTERACTIVE_BUTTON] =
        {.slug = "interactive-button",
         .title = "Interactive button workflow",
         .summary = "Build visual button states and exercise pointer or keyboard activation.",
         .steps = {"Open the Interactive Button example and select the Button group.",
                   "Edit the rounded background and focus-ring objects as one hierarchy.",
                   "Use the Press Feedback clip to animate scale, color, and opacity.",
                   "Compile and send pointer down/up or keyboard activate through the C API.",
                   "Poll runtime events and let the host perform application actions.",
                   "Authoring state-machine bindings are limited; host event wiring is required."},
         .step_count = 6U},
    [AF_ONBOARDING_RESPONSIVE_UI] =
        {.slug = "responsive-ui",
         .title = "Responsive UI and safe areas",
         .summary = "Anchors, percent sizing, layout direction, and safe areas adapt a canvas.",
         .steps = {"Open the Responsive UI example and select a layout-managed object.",
                   "Use Inspector layout controls to cycle fixed, percent, and intrinsic sizing.",
                   "Set anchors and pivots before adjusting offsets and minimum sizes.",
                   "Configure the root safe area and Fit, Fill, or Stretch canvas scaling.",
                   "Resize the window and inspect ruler measurements at multiple aspect ratios."},
         .step_count = 5U},
    [AF_ONBOARDING_PARTICLE_EFFECT] =
        {.slug = "particle-effect",
         .title = "Particle effect authoring",
         .summary = "Emitters expose spawning, forces, output, collisions, LOD, and trails.",
         .steps = {"Open the Particle Effect example and select the Emitter object.",
                   "Use the Inspector pages to set spawn shape, lifetime, velocity, and color.",
                   "Tune force, collision, trail, flipbook, and output pages as needed.",
                   "Preview at several zoom levels; particles remain canvas-clipped.",
                   "Set capacity and LOD before compiling to bound runtime cost."},
         .step_count = 5U},
    [AF_ONBOARDING_RUNTIME_INTEGRATION] =
        {.slug = "runtime-integration",
         .title = "Embed the Arcflow C runtime",
         .summary = "Load a compiled package, create an instance, update it, and release handles.",
         .steps = {"Include arcflow/arcflow_runtime.h and link arcflow_runtime.",
                   "Fill AfRuntimeCreateInfo; frames_in_flight must be one or two.",
                   "Call af_runtime_create, af_package_load, then af_instance_create.",
                   "Call play/update, resize, input, property, and event functions from the host.",
                   "Destroy instance before package, then destroy the runtime.",
                   "See the shipped runtime_sample.c; it is built and tested with every release."},
         .step_count = 6U},
    [AF_ONBOARDING_VULKAN_INTEGRATION] =
        {.slug = "vulkan-integration",
         .title = "Vulkan host integration",
         .summary = "Arcflow accepts an existing Vulkan device and graphics queue.",
         .steps =
             {"Create a Vulkan 1.3 instance, physical device, logical device, and queue.",
              "Pass all four Vulkan handles plus the graphics queue family together.",
              "Keep those handles alive until every Arcflow runtime instance is destroyed.",
              "Use host-owned command buffers only through the versioned host SDK contract.",
              "Arcflow does not own presentation, queue submission, waits, or device teardown.",
              "Check runtime and package ABI versions before loading external content."},
         .step_count = 6U},
    [AF_ONBOARDING_FILE_FORMATS] =
        {.slug = "file-formats",
         .title = "Project and package formats",
         .summary = ".afproj is deterministic authoring text; .afp is validated runtime data.",
         .steps = {"Commit .afproj text and project-relative asset sources to source control.",
                   "Never hand-edit .afp packages; regenerate them with arcflow_compiler.",
                   "Resolve <<<<<<<, =======, and >>>>>>> markers before opening a project.",
                   "Use --validate-project, --check-determinism, and --validate in CI.",
                   "Unknown future format versions are rejected before allocation or execution."},
         .step_count = 5U},
    [AF_ONBOARDING_C_API] =
        {.slug = "c-api",
         .title = "C API reference map",
         .summary = "The installed public headers are the authoritative declarations.",
         .steps = {"Runtime lifetime: af_runtime_create and af_runtime_destroy.",
                   "Packages: af_package_load and af_package_destroy.",
                   "Instances: create, destroy, update, play, pause, stop, seek, and resize.",
                   "Input: pointer, keyboard, controller, state inputs, triggers, and events.",
                   "Bindings: property_id, property type, get, set, format, and locale.",
                   "Accessibility and VFX controls are declared in arcflow_runtime.h."},
         .step_count = 6U},
    [AF_ONBOARDING_TROUBLESHOOTING] =
        {.slug = "troubleshooting",
         .title = "Troubleshooting",
         .summary = "Use validation first, then isolate project, package, platform, or GPU state.",
         .steps = {"Run --validate-project; repair missing assets and merge markers first.",
                   "Run --check-determinism and compile a release package again.",
                   "Open Workspace > System / Displays for DPI, input, and Vulkan capabilities.",
                   "Use F11 project validation and F10 profiler inside the editor.",
                   "Keep generated captures when reporting visual or driver-specific problems.",
                   "Unsupported GPU capabilities fail explicitly instead of silently degrading."},
         .step_count = 6U},
    [AF_ONBOARDING_PERFORMANCE] =
        {.slug = "performance",
         .title = "Performance workflow",
         .summary = "Measure frame time, then reduce the subsystem shown to be expensive.",
         .steps = {"Use F10 to inspect frame time, scene size, animation keys, and allocations.",
                   "Bound particle capacity and use LOD before reducing visual quality globally.",
                   "Pack atlases and avoid unnecessary image dimensions and duplicated assets.",
                   "Keep vector topology stable and use the fewest useful animated points.",
                   "Use release packages for representative measurements."},
         .step_count = 5U},
    [AF_ONBOARDING_SECURITY] =
        {.slug = "security",
         .title = "Security boundaries",
         .summary = "Treat projects, packages, plugins, and update artifacts as untrusted input.",
         .steps = {"Validate sizes, formats, runtime ABI, and package versions before use.",
                   "Use atomic writes and rotating backups for project and staged update files.",
                   "Require signature verification before accepting an update artifact.",
                   "Only register trusted editor plugins and validate compiler-transform output.",
                   "Do not expose arbitrary host files through runtime or plugin callbacks."},
         .step_count = 5U},
    [AF_ONBOARDING_MIGRATION] =
        {.slug = "migration",
         .title = "Project and runtime migration",
         .summary = "Migrate authoring data forward while keeping the original recoverable.",
         .steps = {"Keep the original project and its asset directory under source control.",
                   "Open the project in the new editor and review migration diagnostics.",
                   "Save to a new path first when crossing more than one format generation.",
                   "Validate and compile again; do not reuse packages from an incompatible ABI.",
                   "Downgrade by restoring source control, not by editing version fields."},
         .step_count = 5U},
    [AF_ONBOARDING_SHORTCUTS] =
        {.slug = "shortcuts",
         .title = "Keyboard shortcut reference",
         .summary = "F1 opens the live, configurable shortcut list for the current build.",
         .steps = {"Ctrl+N/O/S create, open, and save; Ctrl+Shift+S saves as.",
                   "F5 compiles, Space previews, Home stops, K adds a key, E shows curves.",
                   "Ctrl+Z/Y undo and redo; Ctrl+C/V/D copy, paste, and duplicate.",
                   "Ctrl+F searches hierarchy; Ctrl+Shift+P opens the command palette.",
                   "G toggles grid, N toggles snap, and Alt+Enter toggles fullscreen.",
                   "Open Help > Keyboard Shortcuts to inspect or rebind every command."},
         .step_count = 6U},
    [AF_ONBOARDING_LIMITATIONS] = {
        .slug = "limitations",
        .title = "Known limitations",
        .summary = "Current constraints are explicit so projects can plan around them.",
        .steps = {"Primary validated targets are Linux x64 and Windows x64; macOS is later.",
                  "Cloud collaboration is not built in; use standard source control and locks.",
                  "Lottie import and several advanced SVG/VFX export features remain partial.",
                  "Interactive state-machine authoring is less complete than runtime playback.",
                  "Automatic updates are opt-in and require a host-provided signature verifier.",
                  "GPU behavior still depends on Vulkan driver and feature support."},
        .step_count = 6U}};

uint32_t af_onboarding_guide_count(void) { return AF_ONBOARDING_TOPIC_COUNT; }

const AfOnboardingGuide *af_onboarding_guide(AfOnboardingTopic topic) {
    return (uint32_t)topic < AF_ONBOARDING_TOPIC_COUNT ? &af_onboarding_guides[topic] : NULL;
}

const AfOnboardingGuide *af_onboarding_guide_find(const char *slug) {
    if (slug == NULL) {
        return NULL;
    }
    for (uint32_t index = 0U; index < AF_ONBOARDING_TOPIC_COUNT; ++index) {
        if (strcmp(slug, af_onboarding_guides[index].slug) == 0) {
            return &af_onboarding_guides[index];
        }
    }
    return NULL;
}

AfOnboardingTopic af_onboarding_guide_topic(const AfOnboardingGuide *guide) {
    for (uint32_t index = 0U; guide != NULL && index < AF_ONBOARDING_TOPIC_COUNT; ++index) {
        if (guide == &af_onboarding_guides[index]) {
            return (AfOnboardingTopic)index;
        }
    }
    return AF_ONBOARDING_TOPIC_COUNT;
}
