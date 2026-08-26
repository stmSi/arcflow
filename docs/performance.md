# Performance targets

Arcflow's long-duration CPU gate uses the shipped `vector-animation` template as a stable representative
workload. The gate compiles a stripped release package, validates it, repeatedly loads it, and then
plays 60,000 updates after warm-up. It runs in Debug, sanitizer, and optimized CI configurations.

The Arcflow 1.0 minimum targets are:

* Mean package load no greater than 50 ms over 16 loads.
* Mean runtime update no greater than 1 ms over 60,000 frames.
* Zero general heap allocations in every measured runtime update.
* A valid scene after 1,000 seconds of simulated playback.

Run the gate directly with:

```sh
./build/debug/arcflow_beta_performance_tests
```

It prints one JSON object for CI collection. These CPU thresholds are release blockers when the
same workload hash and supported hardware class regress. They do not substitute for renderer frame
time, GPU memory, integrated/discrete GPU, vendor-driver, or long-duration interactive testing;
those results belong to the hardware matrix before stable release.

`arcflow_release_tests` additionally release-compiles, validates, loads, resizes, and plays all six
shipped templates for an aggregate 3,600 frames while checking the stable Host SDK and zero runtime
frame allocations. The production acceptance target runs both gates.
