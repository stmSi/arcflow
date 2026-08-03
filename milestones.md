# Arcflow milestone roadmap

Follow milestones in order.

Do not start a later milestone until the active milestone:

* Builds successfully
* Passes relevant tests
* Meets its acceptance criteria
* Has no known critical correctness or security defects
* Preserves previously completed functionality

Only implement the currently requested milestone.

---

# Milestone 1: Foundation and analytic vector player

Implement the smallest usable Arcflow runtime foundation.

## Core

Implement:

* Structured errors
* Logging
* Overflow-safe arithmetic
* Arena allocator
* Scratch allocator
* Generation-based handles
* File loading with size limits
* Basic vectors and matrices
* 2D affine transforms
* Debug allocation tracking

## Vulkan

Implement:

* SDL3 window
* Vulkan 1.3 instance
* Debug messenger
* Physical-device selection
* Logical device
* Graphics and presentation queues
* Swapchain
* Swapchain recreation
* Dynamic rendering
* Synchronization 2
* Two frames in flight
* Command pools and command buffers
* Persistent upload ring
* Vulkan object names
* Validation-layer support

Use one primary graphics queue.

Do not implement asynchronous compute or multithreaded command recording.

## Rendering

Render these analytically with instanced quads:

* Rectangle
* Rounded rectangle
* Ellipse

Support:

* Position
* Rotation
* Scale
* Bounds
* Solid fill
* Opacity
* Corner radius
* Parent-child transforms
* Layer ordering
* Canvas resizing
* Premultiplied-alpha blending

## Animation

Support:

* Position tracks
* Rotation tracks
* Scale tracks
* Opacity tracks
* Fill-color tracks
* Corner-radius tracks
* Step interpolation
* Linear interpolation
* Cubic Bézier easing
* Play
* Pause
* Stop
* Seek
* Loop
* Reverse playback
* Playback speed

## Runtime package

Implement:

* Versioned Arcflow Runtime Package
* Package writer
* Package validator
* Package loader
* Scene hierarchy sections
* Primitive sections
* Animation sections
* Bounds and overflow validation

## Player

Create `arcflow_player`.

It must load and play a generated demonstration package without editor code.

## Acceptance criteria

* All Milestone 1 targets compile.
* Unit tests pass.
* All three primitive types render.
* Parent-child transforms work.
* Animation playback works.
* Reverse and looping playback work.
* Package validation rejects malformed data.
* Playback performs zero general heap allocations per frame.
* Vulkan validation reports no errors in the tested workflow.
* Swapchain recreation works.

---

# Milestone 2: Basic vector editor

Create the initial Arcflow Editor.

Implement:

* Dockable editor window layout
* Canvas viewport
* Scene hierarchy panel
* Property inspector
* Toolbar
* Selection system
* Rectangle creation
* Rounded-rectangle creation
* Ellipse creation
* Node deletion
* Node duplication
* Node renaming
* Layer reordering
* Parenting and reparenting
* Transform editing
* Fill-color editing
* Opacity editing
* Corner-radius editing
* Canvas zoom
* Canvas pan
* Grid display
* Basic snapping
* Transform gizmos

Implement an editor command system supporting:

* Apply
* Undo
* Redo
* Command merging
* Stable target identifiers
* Bounded undo memory

Implement editable project saving and loading.

Implement project compilation to an Arcflow Runtime Package.

## Acceptance criteria

* Users can create and edit primitive scenes.
* Undo and redo work for every implemented edit.
* Projects save and reopen correctly.
* Invalid projects are rejected safely.
* Compiled packages play in `arcflow_player`.
* Editor selection state is not serialized into runtime packages.
* Continuous drag edits merge into one undo operation.

---

# Milestone 3: Timeline and curve editor

Implement the primary animation-authoring workflow.

Implement:

* Animation clips
* Timeline ruler
* Timeline zoom and pan
* Track hierarchy
* Add keyframe
* Delete keyframe
* Move keyframe
* Duplicate keyframe
* Multi-keyframe selection
* Box selection
* Timeline snapping
* Playback controls
* Playhead scrubbing
* Loop-region editing
* Step interpolation
* Linear interpolation
* Cubic Bézier easing
* Curve editor
* Tangent editing
* Easing presets
* Dope-sheet view
* Copy and paste keyframes
* Scale keyframe timing
* Reverse selected keyframes
* Timeline undo and redo

## Acceptance criteria

* Every initial animated property can be authored visually.
* Scrubbing is deterministic.
* Moving and editing keyframes supports undo and redo.
* Empty and malformed tracks fail safely.
* Large keyframe selections remain responsive.
* Normal playback performs zero general heap allocations.

---

# Milestone 4: Arbitrary vector paths

Implement editable vector paths.

Support:

* Move commands
* Line segments
* Cubic Bézier segments
* Open contours
* Closed contours
* Multiple contours
* Path-point selection
* Control-handle editing
* Point insertion
* Point deletion
* Point conversion
* Path closing and opening
* Path reversal
* Path hit testing

Implement:

* Adaptive path flattening
* CPU tessellation
* Geometry caching
* Geometry generation counters
* Solid fills
* Non-zero fill rule
* Even-odd fill rule
* Bounded subdivision depth
* Malformed-path validation

Do not rebuild geometry when only transforms, opacity, or paint values change.

## Acceptance criteria

* Arbitrary paths render correctly.
* Self-intersecting and compound paths behave predictably.
* Tessellation cannot recurse or subdivide without bounds.
* Unchanged paths reuse cached geometry.
* Path editing supports undo and redo.
* Large paths cannot exhaust memory through unbounded input.

---

# Milestone 5: Strokes, gradients, and vector styling

Implement production-quality vector styling.

Support:

* Stroke width
* Butt caps
* Round caps
* Square caps
* Miter joins
* Round joins
* Bevel joins
* Miter limits
* Dashed strokes
* Dash offsets
* Variable-width strokes later in this milestone
* Linear gradients
* Radial gradients
* Conic gradients
* Multiple gradient stops
* Gradient transforms
* Per-stop opacity
* Fill and stroke opacity
* Paint inheritance
* Style presets

Add rendering-quality tests for:

* Thin strokes
* Sharp joins
* Large strokes
* Tiny shapes
* High-DPI output
* Extreme transforms
* Nested contours

## Acceptance criteria

* Stroke joins and caps are visually correct.
* Gradient interpolation is consistent.
* Styling changes do not unnecessarily rebuild geometry.
* Extreme stroke inputs remain bounded and safe.
* Rendering remains stable across supported GPU vendors.

---

# Milestone 6: Responsive layout and text

Implement resolution-independent interface layout and text.

## Layout

Support:

* Reference canvas resolution
* Canvas scaling modes
* Anchors
* Pivots
* Fixed dimensions
* Percentage dimensions
* Minimum and maximum dimensions
* Aspect-ratio constraints
* Safe areas
* Nested transforms
* Alignment containers
* Basic horizontal and vertical layout groups

Use this order:

```text
Parent layout
    ↓
Node layout
    ↓
Base transform
    ↓
Animation overrides
    ↓
Final world transform
```

## Text

Use HarfBuzz and FreeType.

Support:

* UTF-8 validation
* Text shaping
* Left-to-right text
* Right-to-left text
* Kerning
* Ligatures
* Alignment
* Wrapping
* Line spacing
* Truncation
* Runtime text replacement
* Font fallback
* Glyph-atlas caching
* Partial atlas updates

Begin with grayscale glyph rendering.

Add SDF or MSDF rendering only after the basic text system is stable.

## Acceptance criteria

* Layout adapts correctly across aspect ratios.
* Safe areas work.
* Layout and animation interaction follows the documented order.
* International text shapes correctly.
* Glyph caches remain bounded.
* Runtime text replacement does not rebuild unrelated scene data.

---

# Milestone 7: Reusable components and symbols

Implement reusable animated components.

Support:

* Component definitions
* Component instances
* Nested components
* Instance transforms
* Instance text overrides
* Instance color overrides
* Exposed numeric properties
* Exposed Boolean properties
* Exposed image properties
* Independent component playback
* Shared immutable component data
* Component editing mode
* Component detachment
* Component duplication

Detect and reject:

* Cyclic component references
* Excessive nesting
* Invalid overrides
* Missing component references

## Acceptance criteria

* Multiple component instances reuse shared data.
* Instances maintain independent animation state.
* Component cycles are rejected.
* Overrides serialize and compile correctly.
* Runtime packages avoid unnecessary duplication.

---

# Milestone 8: Input, hit testing, and animation events

Implement interactive UI foundations.

Support:

* Pointer hit testing
* Pointer enter
* Pointer leave
* Pointer move
* Pointer down
* Pointer up
* Click
* Double click
* Drag start
* Drag
* Drag end
* Focus
* Blur
* Keyboard activation
* Controller navigation
* Disabled interaction state

Hit testing must respect:

* Node transforms
* Visibility
* Opacity thresholds
* Parent visibility
* Clipping
* Masks later
* Layer order
* Pointer-passthrough settings

Implement named animation events.

The runtime must queue events and expose them through the public C API.

Do not execute arbitrary scripts.

## Acceptance criteria

* Input behavior is deterministic.
* Pointer capture works during dragging.
* Hidden or clipped nodes do not receive input.
* Event queues are bounded.
* Host applications can poll events safely.
* Input processing performs no uncontrolled allocation.

---

# Milestone 9: Animation state machines

Implement state-machine-driven interfaces.

Support input types:

* Boolean
* Integer
* Float
* Trigger

Support:

* Entry state
* Exit state
* Animation state
* Any-state transitions
* Transition priorities
* Transition durations
* Transition easing
* Exit-time conditions
* Input comparisons
* Trigger conditions
* Animation-completion conditions
* Event conditions
* State tags
* Nested state machines
* State-machine debugging view

Add a strict maximum transition count per update.

## Acceptance criteria

* State transitions are deterministic.
* Infinite transition loops are prevented.
* Host applications can set inputs through the C API.
* State machines support reusable UI components.
* State-machine data compiles into compact runtime data.
* Runtime evaluation allocates no general heap memory.

---

# Milestone 10: Advanced animation authoring

Expand animation capabilities.

Implement:

* Multiple clips
* Clip duplication
* Clip trimming
* Clip splitting
* Clip markers
* Timeline regions
* Nested timelines
* Time remapping
* Hold frames
* Animation layers
* Additive animation
* Property blending
* Clip transitions
* Animation speed curves
* Motion-path display
* Motion-path editing
* Onion-skin preview
* Ghosted frames
* Animation retiming tools
* Easing preset library

Support reusable easing presets:

* Ease in
* Ease out
* Ease in-out
* Back
* Bounce
* Elastic
* Overshoot
* Custom cubic Bézier

## Acceptance criteria

* Multiple clips safely animate the same scene.
* Blending and additive animation produce stable results.
* Reverse playback and time remapping work.
* Nested animation cycles are rejected.
* Large timeline projects remain responsive.
* All timeline operations support undo and redo.

---

# Milestone 11: Masks, clipping, and advanced composition

Implement advanced layer composition.

Support:

* Rectangle clipping
* Rounded-rectangle clipping
* Vector-path clipping
* Alpha masks
* Inverted masks
* Nested masks
* Offscreen layers
* Group opacity
* Blend modes
* Layer filters
* Blur
* Drop shadows
* Inner shadows
* Glow
* Color adjustments
* Cached offscreen layers
* Render-target pooling

Initially support common blend modes only:

* Normal
* Add
* Multiply
* Screen

Add others after validation and profiling.

## Acceptance criteria

* Nested clipping behaves correctly.
* Masks work with animated transforms.
* Offscreen resources are reused.
* Render-target memory remains bounded.
* Unsupported blend modes fail clearly.
* Composition produces no synchronization validation errors.

---

# Milestone 12: Shape animation and deformation

Implement advanced vector animation.

Support:

* Path-point animation
* Shape morphing
* Compatible-path validation
* Trim paths
* Animated stroke width
* Animated gradient stops
* Animated gradient transforms
* Animated masks
* Mesh deformation
* Shape-key targets
* Shape interpolation
* Path-follow animation

Prevent incompatible morph targets from producing unsafe data.

## Acceptance criteria

* Compatible paths morph smoothly.
* Incompatible morph data is rejected.
* Trim paths work with open and closed contours.
* Deformation preserves valid geometry.
* Geometry caches update only when necessary.
* Shape animation remains deterministic.

---

# Milestone 13: Rigging and procedural motion

Implement 2D rigging.

Support:

* Bone hierarchy
* Skin weights
* Bone editing
* Bone animation
* IK chains
* IK targets
* Aim constraints
* Position constraints
* Rotation constraints
* Scale constraints
* Distance constraints
* Path-follow constraints
* Transform-copy constraints

Implement procedural motion:

* Spring motion
* Damped motion
* Oscillation
* Noise motion
* Follow-through
* Secondary motion
* Fixed-step simulation
* Baking procedural motion to keyframes

Use bounded solver iterations.

Reject cyclic constraint graphs.

## Acceptance criteria

* Rigged vector characters animate correctly.
* Constraint evaluation is deterministic.
* Constraint cycles are rejected.
* Procedural motion can be baked.
* Solver costs are bounded and measurable.
* Deformation does not corrupt cached geometry.

---

# Milestone 14: GPU particle foundation

Implement GPU-driven particles.

Use:

* Fixed-capacity GPU pools
* Alive index lists
* Dead index lists
* Compute shaders
* GPU counters
* Indirect draw commands
* Graphics-queue compute initially
* Deterministic random seeds
* No normal-frame GPU readback

Support emitters:

* Point
* Line
* Rectangle
* Circle
* Vector path

Support properties:

* Spawn rate
* Burst
* Lifetime
* Initial position
* Initial velocity
* Initial rotation
* Initial size
* Initial color
* Gravity
* Directional wind
* Drag
* Color over lifetime
* Size over lifetime
* Rotation over lifetime

Support outputs:

* Billboard
* UI-space billboard
* Flipbook

## Acceptance criteria

* Particle simulation remains on the GPU.
* Particle counts do not require CPU readback.
* Capacity overflow is handled predictably.
* Simulation is deterministic for a fixed seed and time step.
* Particle rendering uses indirect commands.
* Runtime particle memory remains bounded.

---

# Milestone 15: Advanced real-time VFX

Expand the VFX system.

Implement:

* Curl-noise turbulence
* Radial force
* Vortex force
* Attractors
* Repulsors
* Vector fields
* Collision planes
* Depth collision
* Signed-distance-field collision later
* Trails
* Ribbons
* Mesh particles
* Decals
* Heat distortion
* Refraction effects
* Soft particles
* Particle lights or light proxies
* Sub-emitters
* Event-driven bursts
* Particles spawning particles
* Particle spawning along animated paths

Add VFX LOD:

* Maximum particle count
* Spawn-rate scaling
* Simulation-frequency scaling
* Renderer disabling
* Distance-based quality
* Host-defined quality level

## Acceptance criteria

* Advanced forces are stable.
* Trails and ribbons remain bounded.
* Recursive emitters have strict limits.
* VFX LOD visibly reduces cost.
* Collision failures do not corrupt particle pools.
* GPU and CPU timings are measurable.

---

# Milestone 16: Images, atlases, and asset pipeline

Implement a production asset pipeline.

Support:

* PNG import
* JPEG import where appropriate
* KTX2 import
* SVG import
* Image sequences
* Flipbook generation
* Texture atlases
* Atlas packing
* Texture trimming
* Nine-slice images
* Sprite borders
* Mip generation
* Compression settings
* Color-space metadata
* Asset deduplication
* Asset dependency tracking
* Missing-asset diagnostics
* Project-relative asset paths

The compiler must:

* Normalize assets
* Deduplicate identical resources
* Validate dimensions and formats
* Generate runtime-ready textures
* Reject unsafe or oversized inputs

## Acceptance criteria

* Imported assets compile consistently.
* Runtime packages contain no absolute editor paths.
* Duplicate assets are stored once.
* Color-space metadata remains correct.
* Invalid and oversized images are rejected.
* Atlas generation is deterministic.

---

# Milestone 17: Data binding and runtime customization

Implement exposed runtime properties.

Support:

* Scalar values
* Integer values
* Boolean values
* Colors
* Text
* Images
* Visibility
* Progress values
* Animation selection
* Playback controls
* State-machine inputs
* Particle parameters

Use stable hashed property identifiers.

Development packages may retain names for debugging.

Release packages may strip unnecessary names.

Implement formatted values:

* Integer formatting
* Decimal precision
* Percentage formatting
* Time formatting
* Prefixes
* Suffixes
* Plural-ready placeholders

Do not implement an unrestricted expression language.

Use typed, validated binding operations.

## Acceptance criteria

* Games can update Arcflow content without recompiling it.
* Invalid property identifiers return structured errors.
* Type mismatches are rejected.
* Release packages can remove debug names.
* Binding evaluation remains bounded.
* Runtime updates allocate no general heap memory.

---

# Milestone 18: Localization and accessibility

Implement localization support.

Support:

* Locale-specific text tables
* Runtime locale switching
* Font fallback chains
* Right-to-left layout
* Locale-aware number formatting
* Locale-aware date and time placeholders
* Text expansion testing
* Missing-translation diagnostics

Implement accessibility metadata:

* Accessible labels
* Roles
* States
* Descriptions
* Focus order
* Keyboard navigation
* Controller navigation
* Reduced-motion mode
* High-contrast overrides
* Text scaling
* Color-blind-safe preview modes

The runtime should expose accessibility information to host applications rather than attempting to replace platform accessibility APIs.

## Acceptance criteria

* Runtime locale switching works.
* Right-to-left UI layouts correctly.
* Text scaling does not corrupt layouts.
* Reduced-motion mode can disable or simplify animation.
* Focus navigation is deterministic.
* Accessibility metadata is available through the runtime API.

---

# Milestone 19: Import and export interoperability

Implement importers where licensing and format documentation permit.

Consider supporting:

* SVG
* Lottie JSON
* Sprite sheets
* Image sequences
* Common easing formats

Implement exporters for:

* Arcflow editable projects
* Arcflow Runtime Packages
* PNG image sequences
* Sprite sheets
* Preview videos through an optional external encoder integration
* Static SVG where representable

Importers must:

* Validate all input
* Produce warnings for unsupported features
* Avoid silently changing semantics
* Preserve source metadata where useful
* Enforce strict complexity limits

## Acceptance criteria

* Supported SVG assets import reliably.
* Unsupported imported features produce clear diagnostics.
* Malformed files cannot crash the editor.
* Exported image sequences match editor playback.
* Import operations support undo or transactional rollback.

---

# Milestone 20: Plugin and extension architecture

Implement a controlled extension system.

Start with native tool plugins for the editor only.

Support extension points for:

* Importers
* Exporters
* Custom editor panels
* Asset processors
* Validation passes
* Compiler transforms
* Preview tools

Do not allow plugins to modify internal memory directly.

Use versioned plugin interfaces.

Provide:

* API version checks
* Capability declarations
* Plugin loading diagnostics
* Plugin isolation boundaries where practical
* Safe plugin unloading rules
* Plugin crash attribution

Do not load untrusted plugins automatically.

Runtime plugins are out of scope until the editor plugin system is stable.

## Acceptance criteria

* Incompatible plugins are rejected safely.
* Plugins cannot bypass package validation.
* Editor operation without plugins remains unchanged.
* Plugin API versions are explicit.
* Plugin failures produce actionable diagnostics.

---

# Milestone 21: Runtime SDK and engine integrations

Finalize the Arcflow Runtime SDK.

Provide:

* Stable C API
* Static and shared library builds
* Existing-Vulkan-device integration
* Host command-buffer integration
* Host synchronization documentation
* Allocator callbacks
* Logging callbacks
* File-loading callbacks
* Custom resource callbacks
* Threading rules
* Version queries
* Feature queries
* Backward-compatible package handling where possible

Create example integrations for:

* Minimal SDL3 Vulkan application
* Custom Vulkan engine
* Unreal Engine plugin
* Unity native plugin where practical
* Godot native extension where practical

Engine integrations must wrap the same core runtime rather than duplicating runtime logic.

## Acceptance criteria

* Runtime works without SDL3.
* Runtime does not create a second Vulkan device.
* Host ownership rules are explicit.
* Integration examples compile where their SDKs are available.
* Package compatibility behavior is tested.
* Public API changes follow semantic versioning.

---

# Milestone 22: Performance and scalability

Perform evidence-based optimization.

Profile:

* Animation evaluation
* Layout
* Hit testing
* Scene traversal
* Tessellation
* Geometry cache
* Text shaping
* Glyph uploads
* Render-command generation
* Draw submission
* Particle simulation
* GPU composition
* Package loading
* Editor undo memory

Optimize only measured bottlenecks.

Potential optimizations include:

* SIMD animation sampling
* Multithreaded editor tasks
* Multithreaded package compilation
* Parallel path tessellation
* Scene dirty propagation
* Incremental layout
* Incremental hit-test structures
* Pipeline caching
* Descriptor indexing
* Buffer suballocation
* Persistent staging
* GPU culling
* Asynchronous compute where profiling proves a benefit
* Background asset compilation
* Incremental project saving

Add benchmark scenes for:

* Large UI hierarchy
* Heavy animation
* Complex paths
* Heavy text
* Many component instances
* Large particle systems
* Deep clipping
* Mixed workload

## Acceptance criteria

* No uncontrolled per-frame allocation.
* Performance regressions are detected automatically.
* Benchmark results are repeatable.
* Memory growth remains bounded.
* Optimizations preserve deterministic behavior.
* Optional fast paths have safe fallbacks.

---

# Milestone 23: Reliability, fuzzing, and recovery

Harden all parsers and runtime systems.

Add fuzz targets for:

* Runtime package validator
* Editable project parser
* SVG importer
* Lottie importer if supported
* Font-loading boundary
* Image metadata parser
* Path parser
* Animation-track decoder

Add:

* Property-based tests
* Long-duration playback tests
* Repeated load/unload tests
* Device-loss handling strategy
* Swapchain stress tests
* Resize stress tests
* Undo-history stress tests
* Corrupted project recovery
* Autosave
* Crash recovery
* Atomic project saves
* Temporary-file cleanup
* Backup rotation

Run:

* AddressSanitizer
* UndefinedBehaviorSanitizer
* ThreadSanitizer where applicable
* Static analysis
* Vulkan validation
* GPU-assisted validation where practical

## Acceptance criteria

* Fuzzing finds no unresolved critical parser defects.
* Interrupted saves do not destroy the previous project.
* Autosave recovery works.
* Repeated package loading does not leak.
* Long playback remains stable.
* Recoverable errors do not terminate the application.

---

# Milestone 24: Editor workflow and usability polish

Polish the Arcflow authoring experience.

Implement:

* Customizable workspaces
* Docking layouts
* Command palette
* Configurable shortcuts
* Context menus
* Search
* Hierarchy filtering
* Asset browser
* Recent projects
* Templates
* Preset library
* Alignment tools
* Distribution tools
* Smart guides
* Measurement overlays
* Rulers
* Canvas guides
* Multi-selection inspector
* Batch property editing
* Copy and paste across projects
* Drag-and-drop assets
* Missing-resource repair
* Autosave controls
* Recovery interface
* Integrated profiling panel
* Validation panel
* Error navigation
* In-editor documentation links

Ensure keyboard-only operation for core workflows.

## Acceptance criteria

* Common authoring tasks require minimal interaction.
* Keyboard shortcuts cover essential workflows.
* Error messages link to affected objects.
* Layout and workspace settings persist.
* Large projects remain navigable.
* User preferences do not contaminate project or runtime data.

---

# Milestone 25: Collaboration and project management

Implement optional team-oriented project features.

Support:

* Stable text-friendly project metadata
* Asset source tracking
* External-change detection
* Reload and merge warnings
* Project locking indicators
* Conflict detection
* Deterministic project serialization
* Deterministic runtime compilation
* Source-control-friendly identifiers
* Change history metadata
* Project validation in command-line mode
* Headless package compilation
* Continuous-integration support

Do not build cloud collaboration into the core product initially.

Ensure standard source-control systems work well first.

## Acceptance criteria

* Identical project inputs produce identical packages.
* Headless compilation works.
* CI can validate and compile projects.
* External file changes are detected safely.
* Serialization minimizes unnecessary diffs.
* Merge conflicts produce actionable diagnostics.

---

# Milestone 26: Cross-platform hardening

Complete platform validation.

Primary platforms:

* Windows x64
* Linux x64

Optional later platform:

* macOS through MoltenVK

Validate:

* High-DPI behavior
* Multiple monitors
* Mixed-DPI monitors
* Window resizing
* Fullscreen
* Input methods
* Clipboard
* File dialogs
* Keyboard layouts
* Tablet or pen input where supported
* GPU vendor differences
* Integrated GPUs
* Discrete GPUs
* Vulkan loader variations

Do not claim platform support until builds and runtime workflows are tested on that platform.

## Acceptance criteria

* Supported platforms pass the release test suite.
* High-DPI scaling behaves correctly.
* Projects produce consistent runtime results across platforms.
* Unsupported GPU capabilities produce clear errors.
* Platform-specific code remains isolated.

---

# Milestone 27: Release engineering

Create a complete release pipeline.

Implement:

* Reproducible builds
* Signed release artifacts where supported
* Version stamping
* Semantic versioning
* Package-format versioning
* Runtime compatibility tests
* Automated installers
* Portable builds
* Debug-symbol generation
* Crash-symbol handling
* License aggregation
* Third-party notice generation
* Update checks
* Optional automatic updater
* Release-channel support
* Migration testing
* Uninstall testing
* Clean-user-machine testing

Create release channels:

* Development
* Preview
* Beta
* Stable

## Acceptance criteria

* Clean machines can install and launch Arcflow.
* Release builds contain no debug-only dependencies.
* Runtime and package versions are checked correctly.
* Upgrades preserve user projects and settings.
* Uninstallation does not delete user projects.
* Release artifacts are reproducible where practical.

---

# Milestone 28: Documentation, examples, and onboarding

Complete user-facing and developer-facing documentation.

Provide:

* Installation guide
* Quick-start tutorial
* Vector-animation tutorial
* Interactive button tutorial
* Responsive UI tutorial
* Particle-effect tutorial
* Runtime integration tutorial
* Vulkan integration guide
* File-format guide
* C API reference
* Troubleshooting guide
* Performance guide
* Security guide
* Migration guide
* Keyboard shortcut reference
* Example projects
* Sample runtime applications
* Template library

Implement in-product onboarding:

* Welcome screen
* New-project templates
* Guided first project
* Contextual help
* Discoverable shortcuts
* Example browser

## Acceptance criteria

* A new user can create, animate, compile, and play a project without external assistance.
* Runtime integration documentation matches the shipped API.
* Examples build successfully.
* Documentation version matches the application version.
* Known limitations are documented honestly.

---

# Milestone 29: Beta stabilization

Freeze major features.

During beta:

* Do not add large new subsystems.
* Fix crashes.
* Fix data-loss defects.
* Fix package compatibility defects.
* Fix severe rendering inconsistencies.
* Fix accessibility blockers.
* Fix major workflow problems.
* Improve diagnostics.
* Improve migration handling.
* Optimize major measured bottlenecks.
* Expand hardware testing.
* Expand real-world project testing.

Establish severity levels:

* Critical
* High
* Medium
* Low

Release blockers include:

* Data loss
* Remote-code execution
* Package parser memory corruption
* Repeatable crashes in ordinary workflows
* Vulkan validation errors in supported workflows
* Broken package compatibility
* Unbounded memory growth
* Incorrect save or load behavior

## Acceptance criteria

* No unresolved critical defects.
* No known data-loss defects.
* No unresolved high-severity security defects.
* Supported workflows complete without validation errors.
* Beta projects migrate successfully.
* Performance meets documented minimum targets.

---

# Milestone 30: Arcflow 1.0 production release

Prepare the first stable Arcflow release.

The 1.0 release must include:

* Stable vector editor
* Timeline and curve editor
* Responsive layout
* International text
* Reusable components
* Interactive state machines
* Data binding
* Production vector rendering
* GPU particle effects
* Runtime package compiler
* Standalone player
* Stable C runtime API
* Vulkan engine integration
* Autosave and recovery
* Headless validation and compilation
* Production documentation
* Example projects
* Installers and portable builds

## Final acceptance criteria

Arcflow 1.0 is ready only when:

1. All supported targets build in clean environments.
2. Automated tests pass.
3. Sanitizers show no unresolved project-owned defects.
4. Package and project parsers pass required fuzzing thresholds.
5. No known critical or high-severity security defects remain.
6. No known data-loss defects remain.
7. Vulkan validation reports no errors in supported workflows.
8. Runtime playback performs no general heap allocations per frame.
9. Runtime package loading remains bounded and validated.
10. Performance benchmarks meet documented minimum targets.
11. Long-duration stability tests pass.
12. Windows and Linux release workflows pass.
13. Runtime API and package compatibility tests pass.
14. Installation, update, and uninstallation tests pass.
15. Documentation and examples match the shipped release.
16. The editor can create, animate, compile, and preview a complete production UI project.
17. The runtime can load and render that project inside a host Vulkan application.
18. The final release contains no placeholder implementations for advertised features.

After Arcflow 1.0, use semantic versioning and plan later features through separate product roadmaps rather than extending this initial milestone list indefinitely.

