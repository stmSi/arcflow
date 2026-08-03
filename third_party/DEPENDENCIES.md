# Vendored dependencies

Arcflow keeps these dependencies inside the project directory. The bootstrap script pins each
source tag or binary package exactly and normal configuration performs no network access.

| Dependency | Pinned version | License |
| --- | --- | --- |
| SDL | 3.4.12 (`release-3.4.12`) | zlib |
| Vulkan-Headers | 1.3.296 (`v1.3.296`) | Apache-2.0 or MIT by file |
| volk | Vulkan SDK 1.3.296.0 | MIT |
| FreeType | 2.13.3 (`VER-2-13-3`) | FreeType License or GPL-2.0-only |
| HarfBuzz | 10.2.0 | Old MIT; some files carry compatible individual notices |
| Vulkan Validation Layers runtime | Arch Linux 1.4.350.1-1 x86_64 package | Apache-2.0 |
| SPIR-V Tools | Arch Linux 1.4.350.1-1 x86_64 package | Apache-2.0 |
| glslang | Arch Linux 1.4.350.1-1 x86_64 package | Apache-2.0 and BSD-3-Clause |
| shaderc/glslc | Arch Linux 2026.2-2 x86_64 package | Apache-2.0 |

Upstream license texts are retained in the corresponding source directories or binary packages.
