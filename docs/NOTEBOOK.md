# Notebook

Current state and investigation record. Distinguish measured results, user reports,
and hypotheses. Historical reference results are not measurements of this project.


## 2026-10-31 — Vertex capture, queries, and GL-to-Zink validation

**Measured:** the opt-in driver now passes 39/39 Vulkan capture checks and two
native GL-to-Zink capture checks. The original 26/26 rendering suite also passes
against the changed driver with capture disabled. No GL version override or
assertion suppression was used. The workbench runtime and game remain unchanged.
The earlier explicit-GS-only limits below describe the previous checkpoint.

### Capture path and measurements

`0002-kk-vertex-xfb-experiment.patch` reuses Mesa's existing
`poly_nir_passthrough_gs` generator to capture vertex-only output through the
geometry path. The generated stage belongs to the vertex shader object and is
serialized/deserialized with it. Its topology is part of the experimental cache
key. The opt-in retains the existing name `xfb_gs_probe` for script compatibility.

The Vulkan checks now cover:

- Vertex-only and explicit-GS capture; indexed and instanced draws; byte offsets;
  pause/resume; bounded overflow and untouched output/counter guards.
- Point, line-list/strip and triangle-list/strip/fan vertex capture, including
  incomplete primitives and rejecting partial primitives when the buffer fills.
- Dynamic topology transitions within a topology class: line list to strip,
  triangle list to strip, and strip to indexed fan.
- Triangle capture while rendering, plus rendering with capture disabled:
  independent 8×8 RGBA8 readback checks the expected colour and copy guards.
- Stream-zero transform-feedback queries for every case: written versus needed,
  32/64-bit CPU retrieval and GPU copies, availability, and untouched guards.
  A reset but unused query must return `VK_NOT_READY` with unavailable status.
  Two draws within one query validate accumulation.

`0003-kk-xfb-query-experiment.patch` adds stream-zero query reports to the existing
query pool machinery. It accumulates poly's bounded per-draw counts into 64-bit
reports and advertises queries only under the experimental flag. The experiment
reports one stream; geometry streams, rasterization-stream selection and
indirect-byte-count drawing remain unavailable.

The suite runs again in a fresh process from saved pipeline-cache data. All
11 pipeline creations must report application-cache hits and succeed with
`FAIL_ON_PIPELINE_COMPILE_REQUIRED`. An empty-cache control must return
`VK_PIPELINE_COMPILE_REQUIRED`, proving that check is effective. The wrong-data
and separate wrong-query runs both fail 39/39 as expected. Baseline and
experiment-disabled drivers still report transform feedback unsupported.

The expected query semantics follow the [Vulkan transform-feedback query
specification](https://docs.vulkan.org/spec/latest/chapters/queries.html#queries-transform-feedback).
Dynamic topology follows KK's false `dynamicPrimitiveTopologyUnrestricted`
property and Vulkan's [topology-class restriction](https://docs.vulkan.org/guide/latest/primitive_topology.html).

### Corrections exposed by the tests

- The first line/triangle suite passed 24/33: triangles captured line-shaped
  results. The original key distinguished only point/non-point pipelines, which
  was insufficient for the generated capture stage. Adding topology to the
  experimental key made the unchanged 33-case suite pass, including cache reload.
  This is a correction to the new emulation path, not a claim of a demonstrated
  standalone upstream triangle-rendering defect.
- The GL probe initially had no complete framebuffer; draws returned
  `GL_INVALID_FRAMEBUFFER_OPERATION`. Supplying an 8×8 attachment allowed the
  test to reach Zink. That setup failure was not a driver capture result.
- Zink then failed pipeline creation with `VK_ERROR_FEATURE_NOT_PRESENT` because
  the first patch rejected dynamic topology. KK advertises topology-class
  restrictions; supporting that dynamic state made GL capture and overflow pass.
  The new Vulkan transition checks exercise the same class-preserving behavior.
- An initial GL-probe compile failed on an incorrect header path. An old binary
  subsequently only created a context and printed no capture summary; that run
  is not a capture result. `bin/measure-gl-xfb` now verifies the probe build
  manifest and requires the complete expected summary and exit status.

### Measured GL gates and remaining work

With `MESA_KK_EXPERIMENTAL=xfb_gs_probe,custom_border`, Zink reports GL 3.3 core
and compatibility, ES 3.1, and a single missing desktop version-gate extension:
`ARB_texture_buffer_object_rgb32`. A real 3.3 context succeeds; 4.0 and 4.6
requests still fail. These capability reports do not establish complete GL 3.3
conformance. Actual GL capture/overflow and primitive-written queries pass
2/2, and the wrong-byte control fails 2/2.

**Next:** RGB32 texture-buffer support, starting with native-format controls and
float/signed/unsigned format, range, size-query and update checks. Source inspection
shows `kk_CreateBufferView` creates a native Metal texture view and the current
format map lacks RGB32 texel-buffer entries. An emulation path is a candidate,
not yet implemented or measured. Then rerun GL gates and focused GL correctness
checks before Wine integration and actual TFA/LUT assessment.

Remaining capture limits include TES-only output, adjacency, primitive restart,
indirect capture draws, multiple buffers/streams, large-count stress, multiview,
and full pipeline-switching coverage. The current test workloads do not establish
these. The shader path still rejects unsupported non-GS capture stages. No broad
extension-conformance or end-to-end game success is claimed.

Evidence under `docs/evidence/2026-10-31/`:

- `xfb-20261030T233807.978016Z`: final 39-case suite, cold/cache controls,
  independent negative controls, hashes and launch environment.
- `gl-xfb-20261030T233811.101436Z`: final GL capture/overflow controls.
- `render-20261030T233816.192099Z`: rendering regression and pixel control.
- `gl-contexts-20261030T233256.483716Z`: version gates before the dynamic-topology
  change; final GL capture logs independently confirm the same 3.3/RGB32 gate.
- `xfb-20261030T232823.451573Z`: topology/cache failure before its correction.
- `gl-xfb-20261030T233454.889522Z`: Zink pipeline rejection before dynamic support.

Timestamps in directory names are UTC (October 30); local measurements were
October 31 in Europe/Berlin. Generated pipeline-cache binaries remain in `build/`.

## 2026-10-30 — Native rendering and transform-feedback experiment

**Measured:** fresh unmodified Mesa/Zink/KK passes 26/26 native GL rendering
checks both with default KK options and with experimental custom borders. These
cover FBO clear/readback, GLSL 1.20 shader drawing, texture upload/subimage changes,
GPU fence waits, and all eleven mip levels of a 1024×32 texture in RGBA8/RGBA16F.
The deliberately wrong expected-pixel run fails 26/26 and exits 1. No assertions
or advertised versions were overridden. These tests do not run TFA's LUT shader.

**Test correction:** the first mip-chain probe sampled interpolated coordinates
exactly on nearest-filter texel boundaries. Two pixels chose the adjacent texel
at levels 0–6. The corrected probe addresses texel centres explicitly; all levels
pass. The initial and corrected logs are preserved in
`docs/evidence/2026-10-30/rendering/`. Do not treat the initial result as a
confirmed driver defect. Repeat with `bin/build-gl-probe && bin/measure-gl`.

**Measured:** the isolated experimental KK driver passes 11/11 explicit geometry
shader transform-feedback checks on Metal. These compare captured bytes, byte
counters, and untouched guard regions against CPU expectations: base vertex and
instance, indexed draws, instancing, pause/resume, nonzero resume offset, bounded
overflow, capture disabled, and variable GS output counts/prefix sums. The wrong
expected-data control fails all 11 and exits 1. The unmodified driver and the
experimental driver without its opt-in flag both report transform feedback
unsupported. A vertex-only capture pipeline is explicitly rejected with
`VK_ERROR_FEATURE_NOT_PRESENT`; the runner checks that rejection too.

The separate pinned worktree (`src/mesa-xfb`) and build (`build/kk-xfb-x86_64`)
carry the patch in `patches/series`. `bin/build-kk-experiment --verify-only`
checks source equality against that series with a temporary Git index. The
baseline source/build remain unchanged. The opt-in is
`MESA_KK_EXPERIMENTAL=xfb_gs_probe`. This is an incomplete development path,
not general transform-feedback support or a basis for advertising modern GL.
All capture workloads measured here use point-list geometry and rasterizer
discard. Vertex-only/TES capture, queries, other topologies, multiple streams,
indirect draws, capture while rendering, and cache round trips remain unverified
or unimplemented. The 26 rendering checks also pass against this driver with
capture disabled, including the deliberately failing pixel control.

Implementation observations:

- Existing Mesa `poly` supplies the GS count, prefix-sum, pre-GS and capture
  lowering. KK previously discarded the auxiliary shaders. The patch retains
  and dispatches them and wires GPU buffer offsets/counters.
- The first capture probe advanced counters but retained sentinel output bytes.
  Preserving XFB output intrinsics before varying optimization made the static
  cases pass. This follows existing Mesa compiler ordering.
- Variable-output cases then overwrote one captured record. Guarding global
  stores for the padded `UINT32_MAX` vertex invocation made those cases pass.
  The padding explanation is supported by generated code and the controlled
  change; broader topology behavior still needs measurement.
- An early count-shader NIR assertion exposed stale stage/XFB metadata; it was
  corrected rather than disabling assertions.
- Probe runners verify binary/input hashes and require the expected complete
  test summary as well as exit status. A deliberately corrupted build manifest
  was rejected before execution. An old six-case binary cannot establish an
  eleven-case result.

Final evidence: `docs/evidence/2026-10-30/transform-feedback/` contains the
four-scenario run `xfb-20261030T224217.238285Z` and its hashes/arguments/environment.
The rendering evidence includes `render-20261030T223936.513891Z` (baseline) and
`render-20261030T223941.247624Z` (experimental driver, capture disabled).
Reproduce using the commands in README. Raw intermediate compiler/GPU debugging
logs remain in `build/`; they are not the final result.

**Next:** implement and independently validate vertex-only capture, then expand
capture/query/topology coverage before exposing it to Zink. Recheck the native
GL version gates (including RGB32 texel buffers) without overrides. Only after
those gates are backed by correctness tests should the Wine bridge and TFA
rendering be assessed. The 1024×32 mip probe does not establish that TFA's actual
LUT shader or colour grading is correct; that remains an end-to-end target.

## 2026-10-30 — Fresh baseline and scope

User direction: build proper modern GL support for Apple Silicon Wine, initially
Mesa/Zink → KosmicKrisp → Metal, and make unmodified TFA render correctly.
All new graphics sources/checkouts belong here. Reassess the archived prototype
freely: game, libraries, OS, and driver behavior may all have changed.
The old LUT workaround is a historical result, not an acceptable final solution.

### Measured host and game

- Apple Silicon M1 Max; macOS 27.0.1 (26A434).
- Selected Xcode: `/Applications/Xcode-26.6.0.app/Contents/Developer`, SDK 26.5.
- Workbench runtime: WineCX 26.3.0 / Wine 11.0, built 2026-10-10T21:48:19Z,
  patches wine-0004, wine-0007, wine-0008.
- TFA app 1281220, build 26195252, `bin/master/TFA.exe` (x86-64), Ogre GL3Plus.
  Current `hdr_finalToneMapping.glsl:191` calls `applyLut` unchanged.
- Steam's 22:45 launch installed VC++ 2022 x86/x64 and OpenAL successfully,
  then exited 1 after `wglCreateContextAttribsARB failed`.

### Controlled context comparison

Direct launches used the same live Steam prefix/runtime and unmodified game,
`SteamAppId=1281220`, `SteamGameId=1281220`, `WINEMSYNC=1`, and
`WINEDEBUG=+timestamp,+wgl,+loaddll`. Mesa/GL override and injected-library
variables were cleared. Both runs reached TFA/Ogre and exited 1.
Raw records: `build/baseline-2026-10-30/`.

1. **Stock:** Wine's trace records major=4, minor=0, core profile=1, no context
   flags. Wine rejects it with `OS X only supports forward-compatible 3.2+ contexts`.
2. **Only change: `CX_FWD_COMPAT_GL_CTX=1`.** This is existing CW Hack 24834
   (`winemac.drv/opengl.c`), not a new patch. Context creation succeeds. Ogre
   reports Apple M1 Max, GL 4.1.0.0, compute programs unavailable, shader
   profiles through glsl410. Shader compilation then fails (including
   Plankton_OctahedralImpostor_VP.glsl and Plankton_PBS_VP.glsl); process exits 1.

The original context failure is therefore isolated to the flag requirement.
The comparison establishes later shader failures on the Apple GL path. It does
not yet establish the cause of each shader error or the final process exit.
No flag was made persistent in the workbench profile or Steam configuration.

### Fresh upstream baseline

Mesa main cloned into `src/mesa`, pinned at
`18b8ad0b923a1af555deff8899a8d7976c2fe184` (2026-10-30T21:31:06Z),
version `27.0.0-devel`. Latest listed stable release at lookup: 26.2.6,
commit `8e87a503ebd91408c88596d1b9eeb5f4c0b2088d` (comparison candidate).
No archived Mesa patches have been applied.

Measured Vulkan enumeration reports KosmicKrisp/Mesa 27.0.0-devel on Apple M1 Max,
Vulkan 1.4.366. Geometry/tessellation shaders, nullDescriptor, maintenance5, and
depth clip support are advertised by the live driver. This is a capability report,
not a correctness result. Custom border colour remains an explicitly experimental
option in upstream documentation.

The unmodified x86_64 driver and Zink requirements checker built successfully,
with assertions enabled. The first compiler sanity check failed because
Homebrew's `/usr/local/etc/clang/x86_64-apple-darwin.cfg` injected the CLT macOS 27
SDK into Xcode 26.6's linker, which rejected a TAPI architecture. Native compiler
commands now use `--no-default-config`, allowing the explicitly selected SDKROOT.
No system toolchain configuration was changed.

### Measured Vulkan requirements

Both `/usr/local/bin/vulkaninfo` and the freshly built upstream
`zink_check_requirements` ran against the explicit development ICD, first with
no KK options and then with `MESA_KK_EXPERIMENTAL=custom_border`. Both enumerate
the M1 Max. All commands exit 0; **the checker also exits 0 when profiles fail**.
Its text reports all desktop GL baseline profiles unsupported, including GL 4.6.

For GL 4.6, the default report identifies custom border colour/swizzle,
incremental presentation, non-solid fill, image-derived border swizzle,
rectangular lines, transform feedback, and RGB32F uniform texel-buffer support.
Enabling `custom_border` removes the custom-colour extension/feature failures,
but leaves the other requirements, including `borderColorSwizzleFromImage`.
No GL version override or assertion suppression was used.

These are measured capability-profile failures, not proof that every GL operation
fails or that all listed gaps are needed by TFA. The native context measurements below establish the actual GL limit separately. The checker successfully
reports several OpenCL profiles supported, confirming it reaches capability queries.

Evidence: `docs/evidence/2026-10-30/`, including complete Vulkan reports, checker
output, source/driver provenance, TFA inventory and context traces. Raw build logs
remain under `build/`. The new repeat command is `bin/measure-vulkan`.

### Measured native GL contexts

`bin/build-gl-probe` builds `probes/gl-baseline.c` against the unchanged Mesa
libraries, using the generated state-tracker compiler flags. The first run failed
before reaching Zink because the development executable lacked an rpath for Zink's
`@rpath/libvulkan.1.dylib` lookup. Adding that explicit development rpath fixed
loading; no `DYLD_FALLBACK_LIBRARY_PATH` was used. Initial failure logs are retained
and are not treated as GL capability results.

Both default and `custom_border` configurations now measure:

- Zink screen: `zink Vulkan 1.4(Apple M1 Max (MESA_KOSMICKRISP))`.
- `st_api_query_versions`: core=0, compatibility=21, ES1=11, ES2=20.
- A GL 4.0 core request fails. `st_api_create_context` returns error 1; this
  generic state-tracker error is not evidence of physical memory exhaustion.
- A 1.0 compatibility request succeeds, returns GL 2.1 / GLSL 1.20, and reports
  GL error 0. This is the positive context-creation control. No drawing, presentation,
  or LUT correctness has been measured by this probe.
- Core capability initialization reports GLSL ceiling 460, 8 colour attachments,
  4 samples, 16384 texture/renderbuffer size, 2048 array layers, 32 vertex texture
  units, 15 vertex uniform blocks, and 2048 vertex stride.
- Missing extension fields used by Mesa's desktop version calculation:
  `EXT_transform_feedback`, `ARB_transform_feedback2/3`,
  `ARB_transform_feedback_instanced`, `ARB_enhanced_layouts`, and
  `ARB_texture_buffer_object_rgb32`.

**Measured plus source-traced:** `main/version.c` requires
`EXT_transform_feedback` at GL 3.0; all other core 3.0 conditions in that function
are satisfied in the probe. Consequently the first version gate is transform
feedback. `st_extensions.c` also requires transform-feedback buffers for enhanced
layouts. These results do not establish that satisfying the gate alone yields
correct rendering or a conformant modern GL implementation.

**Next implementation lead (source inspection only):** Mesa's shared `src/poly`
library already contains transform-feedback counters, buffer addressing and shader
lowering. KK uses `poly` for geometry/tessellation but clears `nir->xfb_info` and
omits the transform-feedback path (`kk_shader.c`, `libkk/kk_geometry.cl`). Evaluate
reusing that implementation with focused buffer/query/overflow tests before writing
new machinery. RGB32 texel-buffer support and the remaining Vulkan profile gaps
need their own reductions. No driver patch has yet been applied.

### Current game inventory

`bin/inventory-tfa` measured 295 GLSL files, 71 with compute-workgroup declarations,
17 SSBO candidates, 57 image-operation candidates, 10 atomic-operation candidates,
and one geometry-stage candidate. Version directives extend through GLSL 460.
These static counts are not a dynamic call trace; absence from a scan does not
prove non-use. The inventory records DLL and shader hashes for future comparisons.

### Next measurements

- Reduce the missing transform-feedback and RGB32 buffer paths; evaluate existing
  Mesa implementations. Do not advertise capabilities before their operations work.
- Add focused rendering probes from the refreshed game inventory and measured gaps.
  Preserve the unmodified baseline and separate patched measurements.
- Select the smallest WineCX integration after examining current Mesa interfaces.
  Test GL contexts and rendering before TFA. If the old LUT symptom recurs, reduce
  it to an upload/view/sampling test with independently verified data and controls.

### Historical reference

The March project used Wine 11.2, Mesa commit c016346, a Gallium bridge, and
Zink → KK → Metal. It reported museum rendering with only final-shader `applyLut`
colour grading disabled; tonemapping/bloom/sRGB remained. Some ship textures
still had artifacts. Its detailed notes say the actual 11-mip LUT texture was
never independently read back at the Metal level, so the precise cause remained open.
The recorded 7/7 feature, 4/4 context, and 8/8 RGBA16F results are historical.

Upstream references consulted:
- https://docs.mesa3d.org/drivers/zink.html
- https://docs.mesa3d.org/drivers/kosmickrisp.html
- https://docs.mesa3d.org/drivers/kosmickrisp/workarounds.html
- https://docs.mesa3d.org/relnotes.html
