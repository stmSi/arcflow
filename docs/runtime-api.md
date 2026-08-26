# Arcflow Runtime API

Arcflow 1.0 ships Host SDK 1.0.0. Public tables carry `struct_size` and `api_version`; callers must
initialize both before a query or call. Host SDK 1.x follows semantic versioning: later minor
versions remain compatible with earlier 1.x callers, while major-version changes are incompatible.

The public C API is declared in `include/arcflow/arcflow_runtime.h`. `AfRuntime`, package handles, and
instance handles are opaque. Functions return `AfResult`; stale and invalid handles are never
dereferenced.

The host creates the Vulkan instance, physical device, logical device, and graphics queue, then
passes borrowed handles to `af_runtime_create`. Arcflow Runtime does not create another device,
submit host command buffers, call `vkDeviceWaitIdle`, or destroy host objects. A null Vulkan set is
accepted only for package/headless testing; a partially populated set is rejected.

`include/arcflow/arcflow_host_sdk.h` defines the engine-integration contract. The host supplies
allocator, logging, file, resource, Vulkan-device, and command-recording callbacks as needed. Arcflow
validates the borrowed device, target image/view, render area, layouts, sample count, frame index,
and recording flags before entering the registered recording adapter. The host retains ownership of
the command buffer and performs begin/end/submission; Arcflow never waits the device idle.

Ownership and destruction order:

1. The host creates Vulkan objects.
2. Create `AfRuntime`.
3. Load package bytes; the runtime decodes and owns its copy.
4. Create and use instances.
5. Destroy every instance before its package.
6. Destroy packages, then the runtime.
7. The host destroys its Vulkan objects.

The caller retains package input memory and may release it after `af_package_load` returns. Runtime
instances expose playback, pause, stop, seek, speed, direction, update, and safe-area-aware canvas
resize. Pointer, keyboard, and controller input feed a bounded event queue polled through
`af_instance_poll_event`. Hosts can queue named animation events and set Boolean, integer, float,
and trigger state-machine inputs. Package compatibility can be queried without loading: format 13
loads directly, format 12 is identified for upgrade, and packages requiring a newer runtime are
rejected explicitly.
