# Vulkan lifecycle

Initialization order is volk, Vulkan 1.3 instance, optional debug messenger, SDL surface, physical
device and capability selection, logical device and queues, swapchain and views, dynamic-rendering
pipeline, persistent upload ring, then two frame contexts. Every failed stage goes through the same
reverse-order cleanup path.

Required device capabilities are Vulkan 1.3 dynamic rendering, synchronization 2, timeline
semaphores, graphics, swapchain presentation, and host-coherent upload memory. One primary graphics
queue records work; a presentation queue is used only when the platform requires a separate family.
There is no asynchronous compute or multithreaded recording.

Each frame waits its fence, acquires an image, resets its pool/fence, writes its upload-ring slice,
and records a synchronization-2 transition from discarded contents to color attachment. Dynamic
rendering clears and draws the analytic batch, then another barrier transitions to present. A
`vkQueueSubmit2` waits/signals binary swapchain semaphores, followed by presentation. Timeline
support is detected and enabled for future cross-frame resources; binary semaphores and fences are
the appropriate swapchain primitives.

Resize and out-of-date results mark the swapchain for recreation. Recreation waits for the device,
destroys the pipeline, views, and old swapchain, then rebuilds for the new extent/format. Ordinary
rendering never calls `vkDeviceWaitIdle`.

Destruction waits once, then destroys frame synchronization/pools, upload mapping/buffer/memory,
pipeline/layout, image views, swapchain, device, SDL surface, debug messenger, and instance. Host
runtime objects are destroyed before the renderer destroys their borrowed Vulkan device.

