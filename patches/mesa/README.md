# KosmicKrisp transform-feedback development patches

The declared series is isolated from the unmodified baseline in `src/mesa-xfb`
and enabled only with `MESA_KK_EXPERIMENTAL=xfb_gs_probe`. The upstream `all`
experimental option also includes this flag. The flag name is retained as the
experiment expands beyond explicit geometry shaders.

- `0001`: retains and dispatches poly's GS count/pre/capture stages, connects
  buffer bounds and byte counters, and suppresses padded restart writes.
- `0002`: generates a pass-through GS for vertex-only capture; owns and caches
  that stage with the vertex pipeline and keys it by topology. Poly assembles
  per-draw topologies within KK's advertised topology-class restrictions.
- `0003`: adds stream-zero transform-feedback queries, accumulating per-draw
  written/needed counts into 64-bit reports and using existing result-copy paths.

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
