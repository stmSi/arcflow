# Arcflow Runtime API

The public C API is declared in `include/arcflow/arcflow_runtime.h`. `AfRuntime`, package handles, and
instance handles are opaque. Functions return `AfResult`; stale and invalid handles are never
dereferenced.

The host creates the Vulkan instance, physical device, logical device, and graphics queue, then
passes borrowed handles to `af_runtime_create`. Arcflow Runtime does not create another device,
submit host command buffers, call `vkDeviceWaitIdle`, or destroy host objects. A null Vulkan set is
accepted only for package/headless testing; a partially populated set is rejected.

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
and trigger state-machine inputs. Rendering remains hosted by Arcflow Player's renderer boundary;
host-command-buffer recording is not yet public.
