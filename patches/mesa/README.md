# KosmicKrisp transform-feedback development patch

`0001-kk-gs-xfb-experiment.patch` connects existing Mesa `poly` geometry capture
machinery to KosmicKrisp. It is isolated from the unmodified baseline in
`src/mesa-xfb` and selected only with `MESA_KK_EXPERIMENTAL=xfb_gs_probe`.
The upstream `all` experimental option also includes this flag.

This patch advertises an incomplete extension for development probes. Do not
use it to claim Vulkan extension conformance or modern OpenGL support. It rejects
vertex-only and tessellation capture and leaves queries, stream selection and
indirect-byte-count drawing unavailable. Measured scope and results live only
in `docs/NOTEBOOK.md`.

The patch preserves captured varyings, retains/counts/prefix-sums geometry
outputs, dispatches pre-GS work, wires buffer bounds and GPU byte counters, and
suppresses XFB writes from padded restart invocations. The standalone Vulkan
probe validates data independently of Zink and checks failure controls.

Apply only the declared `patches/series` to the pinned source. The experimental
builder verifies exact source equality before compiling; never edit vendor
sources without updating and applying an explicit patch. Keep assertions on.
