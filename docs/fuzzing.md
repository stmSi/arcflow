# Deterministic fuzzing

Arcflow treats editable projects and runtime packages as untrusted input. The beta CI gate builds
all six shipped project templates, serializes and release-compiles them, and applies deterministic
bit, byte, truncation, extension, range, deletion, boundary-value, and embedded-zero mutations.

The companion import gate applies the same mutation families to valid PNG, JPEG, KTX2, SVG, CSS
easing, and OpenType font seeds. The SVG seed includes line, quadratic, cubic, close, and polyline
path data. Accepted images must have bounded dimensions, valid format/color metadata, owned pixels,
and a safe destruction path. Accepted easing functions must have finite, bounded control points.
Accepted fonts are shaped before the retained font bytes are released. Lottie is asserted to remain
explicitly unsupported rather than sending arbitrary bytes to a parser that does not exist.

The required per-run thresholds are:

* At least 768 editable-project parser cases.
* At least 3,072 runtime-package validator cases.
* At least one third of package cases have a repaired size and checksum, allowing mutations to
  exercise section and record validation instead of stopping at checksum rejection.
* Every unexpectedly accepted project must validate and serialize again.
* Every unexpectedly accepted package must decode successfully.
* At least 256 mutations for each of PNG, JPEG, KTX2, and SVG, with accepted and rejected cases for
  every format.
* At least 256 CSS easing mutations and 128 font mutations, each with accepted and rejected cases.

Run the gate with:

```sh
./build/debug/arcflow_parser_fuzz_tests
./build/debug/arcflow_import_fuzz_tests
```

The test prints its fixed seed, workload hash, acceptance counts, and thresholds as JSON. It runs in
Debug, AddressSanitizer, and UndefinedBehaviorSanitizer configurations. Any crash, sanitizer finding,
unexpected error class, validation/decode disagreement, or missed threshold fails CI.

These deterministic gates provide repeatable regression coverage and minimum release thresholds.
They do not replace longer coverage-guided fuzzing, corpus minimization, or retained crash corpora.
Those campaigns remain part of the broader hardware/security release evidence.
