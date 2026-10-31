# Wine bridge integration

The bridge owns Mesa contexts and drawable resources. Wine owns Windows windows,
input, and native Metal views. `MESA_WINE_LIB` selects the opt-in backend in the
patched macOS Wine driver; without it, Wine retains its Apple OpenGL path. A
selected bridge that fails to load does not silently fall back.

`mesa_wine_api.h` is the C boundary. Its ABI version is checked before calling
into the library. Mesa internal types remain inside the bridge. The Wine patch
contains a copy of this header; change both and bump the ABI for incompatible
changes. Wine's separate internal driver ABI is also checked by Wine itself.

The initial implementation deliberately exposes one BGRA8 double-buffered window
format with depth24/stencil8. It does not expose MSAA, floating-point/sRGB window
formats or pbuffers. Mesa's framebuffer objects remain available. The Wine shared
WGL layer advertises some extensions globally; that is not evidence that every
operation is supported by this backend. Front-buffer presentation, configurable
swap intervals, arbitrary multi-window/thread lifetimes and broader pixel formats
need separate implementation/validation before general application use.

Rendering uses Mesa's state tracker and Zink. Presentation currently completes
Mesa work before accessing the Zink image, blits on the GPU to a Vulkan swapchain
on Wine's CAMetalLayer, and uses swapchain-maintenance presentation fences before
reusing presentation synchronization objects or destroying them. This serialized
path is for correctness bring-up, not performance comparisons. The two Zink
patches enable Metal surfaces and supported presentation-fence functionality;
they do not override GL versions or Vulkan capabilities.

Implementation structure and selected drawable/context code were adapted from
the archived `mesa_gl_to_metal` project. Its forced versions, assertion suppression,
unchecked presentation fallback, unimplemented-success pbuffer stubs and speculative
pixel-format claims are not part of this integration contract. The archive is not
a build dependency.

## Reproduce

The source/build prerequisites are described in the repository README. Build the
KK experiment first. Then:

```sh
bin/build-bridge
bin/build-wine-gl-probe
```

Use an isolated WineCX checkout under this repository's `src/`, at the Wine pin
in `sources.lock.json`, with the source subdirectory recorded there. The sibling
workbench builder consumes the graphics patch series:

```sh
../wine-workbench-macos/bin/build-runtime --winecx 26.3.0 --pe-gcc \
  --name winecx-26.3.0-mesa-gl-x86_64 \
  --src "$PWD/src/winecx-bridge/sources/wine" \
  --extra-series "$PWD/patches/wine/series" --jobs 6
```

The workbench's normal patches and this series are applied and verified together.
Patch hashes are written into the new runtime's PROVENANCE. The original runtime
and vendor source checkout are not experimental build destinations.

```sh
bin/package-graphics "$PWD/build/graphics-bundle"
bin/measure-wine-gl --bundle "$PWD/build/graphics-bundle" --hold
```

The bundler refuses to overwrite an existing destination. It copies non-system
dynamic dependencies, uses relative library paths, ad-hoc signs the copied Mach-O
files, writes a relative Vulkan ICD manifest, and records input/output hashes.
This is a development graphics bundle, not a distributable complete Wine product:
the surrounding Wine runtime still embeds its own installation paths. Relocation
must be measured independently. Release licensing, supported OS/architecture
matrix, signing/notarization and a relocatable complete runtime are later packaging
work.

The runner uses `build/prefix-wgl`, never a game prefix or Steam. It records positive,
wrong-pixel, missing-bridge and Apple-backend controls with exact outcomes. A test
that fails during setup does not count as the wrong-pixel control. `--hold` keeps
the final window visible for up to 90 seconds (or until closed) for capture; framebuffer readbacks alone do not establish
presentation. The expected screen is blue/yellow above red/green. Current results
and remaining gaps belong only in `docs/NOTEBOOK.md`.
