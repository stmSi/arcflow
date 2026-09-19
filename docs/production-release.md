# Arcflow 1.0 production release

Arcflow 1.0.0 uses the Stable channel, editable-project format 15, runtime-package format 13,
runtime version 10, and Host SDK 1.0.0. A release candidate is accepted only when the automated gate
and the external platform/hardware evidence both pass for the same source revision.

## Automated acceptance

Configure and run the release gate from a clean source revision:

```sh
cmake --preset release
cmake --build --preset release --target arcflow_production_acceptance
```

The target builds the editor, player, compiler, shared/static runtime, examples, shaders, notices,
debug symbols where `objcopy` is available, and portable installer archive. It then:

* Runs the complete unit, workflow, recovery, compiler, migration, install/update/uninstall,
  performance, deterministic fuzz, onboarding, compatibility, and runtime-sample test suite.
* Plays every shipped template across landscape, safe-area, and portrait canvases with zero general
  heap allocations per runtime frame.
* Exercises Host SDK 1.0 version negotiation, package compatibility, ownership flags, feature
  discovery, and validated host-command-buffer recording.
* Runs 60,000 measured runtime updates, representing 1,000 seconds of deterministic playback.
* Rebuilds the Linux portable archive twice with the pinned build epoch and requires identical
  SHA-256 hashes.
* Installs, launches, upgrades, and uninstalls in an isolated prefix while proving that external user
  projects and settings remain untouched.
* Requires version-matched documentation, third-party notices, portable-artifact checksums, and
  source/build stamps.

A passing run writes `arcflow-production-readiness.json` in the release build directory. That report
identifies the source revision, build epoch, platform, format versions, Host SDK, and artifact hash;
it is evidence for that exact artifact, not a blanket result for later commits.

## External acceptance

The automated report does not manufacture evidence that needs real operating systems, drivers, or
release credentials. Before publishing or tagging 1.0.0, release owners must also retain:

* Passing `arcflow_production_acceptance` jobs from both the Linux and Windows release workflows.
* Vulkan validation runs for the supported editor/player workflows, including resize and recovery,
  on the published supported GPU/driver matrix.
* Interactive long-duration editor/player results with stable CPU/GPU memory use.
* AddressSanitizer and UndefinedBehaviorSanitizer results for project-owned code.
* Coverage-guided parser/import fuzz campaign results and triage showing no unresolved critical or
  high-severity defect.
* Artifact signatures when a platform signing tool and release credential are available.

Any missing external item keeps the build a release candidate even when its local JSON report says
`automated_acceptance: true`.

## Compatibility policy

Arcflow uses semantic versioning after 1.0. Patch releases preserve the Host SDK 1.0 ABI and file
formats. Minor 1.x releases may add backward-compatible API fields or optional package features.
Breaking C ABI changes require a new major Host SDK version; breaking runtime-package changes require
a new package format and explicit compatibility/migration behavior. Older editable projects remain
loadable through the documented migration path, while future unknown formats are rejected without
overwriting input.
