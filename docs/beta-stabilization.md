# Beta stabilization

Milestone 29 freezes major feature work while Arcflow fixes correctness, compatibility, security,
accessibility, workflow, and measured performance defects.

## Severity and release blockers

Arcflow uses four severity levels: Critical, High, Medium, and Low. The bounded diagnostics registry
in `src/stability/diagnostics.h` deduplicates stable issue identifiers, counts occurrences, supports
resolution and reopening, and refuses to declare a build ready if diagnostics were dropped.

Open critical issues always block release. Data loss, remote-code execution, package-parser memory
corruption, ordinary-workflow crashes, Vulkan validation errors, package incompatibility, unbounded
memory growth, and incorrect save/load behavior are release blockers at any severity. High or
critical security issues also block release.

## Project audit

Run the beta audit against an editable project:

```sh
./build/debug/arcflow_compiler --beta-audit project.afproj
```

The audit performs strict project and asset loading, an in-memory save/load migration round trip,
byte-for-byte comparison of the normalized editable-project text, release compilation of both
forms, package validation, and deterministic package comparison. It exits unsuccessfully and prints
categorized diagnostics when a release blocker is found. The audit never overwrites the input
project.

The audit complements, rather than replaces, AddressSanitizer, UndefinedBehaviorSanitizer, Vulkan
validation, hardware-matrix runs, recovery testing, and manual accessibility testing.

The representative CPU and allocation gate is documented in [performance targets](performance.md).
It emits JSON so CI can retain exact measurements instead of treating performance as a subjective
check.

Project, package, image, SVG path, font, and interchange mutation thresholds are documented in
[deterministic fuzzing](fuzzing.md). The gates run under both sanitizers, reject validator/decoder
disagreement, and validate accepted imported data before releasing its backing storage.

## Project migration

Migrate an older editable project explicitly with:

```sh
./build/debug/arcflow_compiler --migrate-project old.afproj migrated.afproj
```

Migration strictly loads the source and its project-relative assets, writes the current format to
memory, reopens and reserializes it against the destination asset directory, and compiles both forms
as release packages. The destination is written atomically only when the normalized editable data
and validated runtime package bytes are identical. Existing destinations receive three rotating
backups; passing the same path performs a recoverable in-place migration. Future project versions
are rejected without output.
