# KosmicKrisp graphics development patches

The declared series is isolated from the unmodified baseline in `src/mesa-xfb`
and opt-in: `MESA_KK_EXPERIMENTAL=xfb_gs_probe` enables capture/query work;
`rgb32_texel` enables uniform RGB32 texel-buffer emulation. Comma-separated flags
can be combined. The upstream `all` option also enables them. The capture flag
name is retained as its experiment expands beyond explicit geometry shaders.

- `0001`: retains and dispatches poly's GS count/pre/capture stages, connects
  buffer bounds and byte counters, and suppresses padded restart writes.
- `0002`: generates a pass-through GS for vertex-only capture; owns and caches
  that stage with the vertex pipeline and keys it by topology. Poly assembles
  per-draw topologies within KK's advertised topology-class restrictions.
- `0003`: adds stream-zero transform-feedback queries, accumulating per-draw
  written/needed counts into 64-bit reports and using existing result-copy paths.
- `0004`: emulates uniform RGB32 float/signed/unsigned reads with three scalar
  loads and buffer-size metadata. Native texture IDs remain at descriptor offset
  zero; buffer-view descriptors grow to hold the emulation metadata. Storage and
  image RGB32 capabilities are not advertised. Cache keys include the option.

These patches advertise an incomplete extension for development probes. Do not
use them to claim Vulkan extension conformance or complete modern GL support.
TES-only capture remains rejected. Geometry streams, rasterization-stream
selection and indirect-byte-count drawing remain unavailable. Measured coverage
and remaining limits live only in `docs/NOTEBOOK.md`.

Apply only `patches/series` to the pinned source. The builder verifies exact
source equality before compiling; never edit vendor sources without recording
and applying an explicit patch. Keep assertions enabled. The Vulkan and GL
probe runners check build manifests and complete expected summaries, so an old
binary or an early crash cannot satisfy a negative control.
