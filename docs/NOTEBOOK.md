# Notebook

Current state and investigation record. Distinguish measured results, user reports,
and hypotheses. Historical reference results are not measurements of this project.






## 2026-11-01 — Public source preparation and binary distribution blocker

User requested public source/history publication of both projects, including the existing full desktop screenshots. Added the connected source-build guide, explicit GL host provisioning, and source-build versus release-maintainer packaging boundaries. No runtime/game code changes and no new gameplay claim in this documentation pass. The new source snapshot is tagged `tfa-source-preview-1` in both repositories.

User report from downloaded separate-account testing: repeated macOS security approvals for individual components make the binary unsuitable for general release. Measured signing inspection: Wine executable unsigned; Mesa bridge ad-hoc signed without a Team ID. Developer ID signing/notarization is deferred by agreement. Keep the binary release a draft; public source availability is a separate milestone. Earlier portable museum/mission and cold setup results remain historical evidence, not proof of a signed download experience.


## 2026-10-31 — Fresh C: installation completed a normal mission

**User report:** played fully normally; entering 3D took some moments and was the
only observed hiccup. The earlier apparent stall recovered without intervention.
The cause/duration of that delay was not sampled, so do not label it a proven
shader-compilation stall. **Measured:** fresh game process exited 0; launch log
selects GL 4.6 on Zink/KosmicKrisp, direct WASAPI with 44100 Hz / 441/2646 buffering,
and presents a GPU frame. Combined with the 49-check swap and 139-check LUT probes,
this establishes the fresh-build/fresh-bottle TFA milestone on this host.

Steam and TFA were installed into the new prefix; TFA uses the default C: library,
with isolated Documents and only three seeded window settings. No user prefix,
login credentials, game settings or built runtime were copied from the working
setup. Host Homebrew/Xcode prerequisites and the verified font/Microsoft installer
cache were reused; cold installer downloads and another Mac remain unvalidated.
Full evidence is under the graphics clone's docs/evidence/2026-10-31/clean-rebuild/.

Next: make a separately named runtime/package relocatable and test the final
packaged binaries. Current Wine still embeds absolute dlopen library names;
its successful source rebuild does not make a zip portable. Preserve this working
runtime and prefix while addressing packaging. No release has been published.

## 2026-10-31 — Default C: installation supported and fresh game launched

User installed TFA in Steam's default C: library. S: is optional. Added workbench
`bin/find-steam-game` to read app manifests in the default Steam library and
registered libraryfolders.vdf paths through the prefix's drive mappings. TFA's
profile resolves its working directory at launch; inventory, trace and LUT tools
use the same helper. Five focused fixture tests pass: default C:, S:, custom drive
with spaces, missing executable, and duplicate installations. Also measured actual
C: discovery in the new bottle and S: discovery in the original bottle.

Fresh installed build is 26195252. TFA.exe, OpenAL32.dll, SDL3.dll, both tone-map
GLSL files and base_lut.png match the original installation's SHA256 values.
The first LUT attempt exposed a shared user-site ARM Pillow / x86 Python mismatch;
created an isolated probe venv with Pillow 12.1.1. The rerun passes 139/139 and
its negative control produces the expected 96 failures. Evidence remains under
build/tfa-lut-20261031T121948.639675Z and ../fresh-game-comparison.json.

Fresh Documents had no TFA settings. Seeded only Full Screen=No, Video Mode=1280
x 720, border=Yes; no post-processing or other settings copied. Launched via the
new gamerun profile, PID 82723, log ../logs/tfa-fresh-play-20261031T122111Z.log.
User visual/audio/gameplay confirmation is pending; leave the game running.
This supersedes earlier instructions requiring S: for this test. The immutable
source/release checkpoint will need both updated repositories; binary relocation
and cold Microsoft/font installer retrieval remain unvalidated.

## 2026-10-31 — Fresh Steam installed; user login/game install pending

Fresh-prefix retry completed successfully: 31 fonts, native VC++ runtime override,
and Wine Mono installed. `get-addons` resolved the absolute source path from the
new runtime's PROVENANCE and downloaded Mono 10.4.1 / Gecko 2.47.4. Prefix setup
uses no DYLD_FALLBACK_LIBRARY_PATH. The winetricks script came from immutable
commit 08304e81f9ac9a83c552a6bd78689040d174bf95 and matches the original SHA256.
Font/Microsoft installers were read from the host's existing winetricks download
cache; cold retrieval of those installers remains a release qualification check.

Only symlinks inside the new prefix's user folders were replaced by local empty
directories, preserving host folders. Documents is now internal and had no TFA
settings. S: resolves to this reproduction workspace's separate library.
Valve's official Steam installer was downloaded, hashed in ../steam-installer.json,
and run with /S. Startup carries the existing Steam helper argument patch and
WINEMSYNC=1. The bootstrapper updated win32 then win64, exited 42 during handoff,
and the new Steam client and helpers remain running (observed main PID 79829).
Logs: ../logs/install-steam.log, steam-first-launch.log, mkprefix-pinned.log.

User was asked to sign in directly, add S: in Steam Settings > Storage, and install
TFA there. No login credentials were requested or copied. Wait for confirmation
before game launch. The personal Steam client was stopped at the user's request;
its prefix/runtime remain unchanged. Binary relocation/package validation is
still pending, as are fresh-bottle TFA shader probe and gameplay checks.

## 2026-10-31 — Clean source rebuild and graphics controls pass; fresh bottle in progress

User returned and explicitly resumed testing. The held supervisor was identity-
checked and resumed; Wine had completed successfully while the supervisor was
stopped. Fresh GStreamer, KK, Mesa bridge, Wine and graphics packaging all passed.
The rebuilt Wine swap probe reports 49 checks / 0 failures; the wrong-reference
control reports the expected 25 failures; missing-bridge and Apple-backend controls
both reject initialization as expected. Full build-driver exit status is zero.
Evidence is in the graphics clone's docs/evidence/2026-10-31/clean-rebuild/.
This is a clean-source rebuild on the existing host, not independent-host validation
or bit-identical rebuild proof. No game has yet run in the fresh bottle.

Fresh-prefix setup exposed a false pin: get-winetricks downloaded master despite
recording a checksum. The exact matching upstream commit was recovered as
08304e81f9ac9a83c552a6bd78689040d174bf95; a fresh download from that immutable URL
matches the original checksum. Fixed only in the reproduction branch. The failed
prefix is preserved under build/prefix-before-winetricks-pin-fix, and creation
from scratch is being retried with provisioning output saved inside the prefix.
At the user's explicit request original Steam PID 85377 was verified against its
executable path and stopped using SIGTERM; it exited without SIGKILL. Fresh Steam
installation/login, isolated Documents, TFA installation and gameplay remain next.

## 2026-10-31 — Working stack available through workbench gamerun tfa

The workbench's `bin/gamerun tfa` now selects the tested runtime, graphics-bundle-
swap-fix, existing Steam prefix and direct WASAPI audio. It launches TFA directly
with SteamAppId/SteamGameId, checks the running Steam executable belongs to that
prefix, validates bundle hashes, and refuses a duplicate game. Start workbench
Steam first and wait for login. `tfa-baseline` preserves the former Apple profile.
This supersedes the older instruction that only trace-tfa selects this stack.

**Measured:** `tfa-gamerun-20261031T094215Z.log` initializes GL 4.6 on Zink/KK,
presents a GPU frame and selects WASAPI at 44100 Hz, 441/2646 buffering.
**User confirmed:** menu and clear music work as before. Missing-bundle and
live duplicate-game controls both reject the launch. The successful game is left
running. Log snapshot and launcher-source hashes are in
`docs/evidence/2026-10-31/tfa-gamerun/`. No CrossOver.app compatibility or portable
binary packaging is established by this launcher change.

## 2026-10-31 — Loading flicker fixed; direct WASAPI music confirmed in mission

**User-confirmed outcome:** loading-picture alternation/overlap disappeared in
museum testing. With direct OpenAL WASAPI output, music is clear at startup/menu
and in a mission ("it sounded perfect"). The user considers the currently observed
issues resolved sufficiently to enjoy the game. Earlier museum rendering, textures,
mission progression and map controls also worked. This is a usable early TFA
baseline, not general GL conformance or coverage of every mission/material.

**Measured graphics cause/fix:** after exchanging front/back resources, the bridge
updated the drawable stamp but did not dirty Mesa's cached framebuffer state.
`mesa_wine_swap_buffers` now calls
`st_context_invalidate_state(st, ST_INVALIDATE_FB_STATE)`, as Mesa's WGL frontend
does after flush. The new `wine-gl-swap` probe batches draws/swaps without per-frame
readbacks or framebuffer rebinds. The old bundle fails four of 49 checks (even-size
batches present an older image); the fixed bundle passes 49/49. The deliberate
wrong-reference control fails 25 checks, and missing-bridge/Apple controls fail as
expected. The installed TFA LUT regression still passes 139 checks, with its
wrong-reference control failing the expected 96. This addresses the reproduced
cached-framebuffer defect and the user-confirmed game symptom.

**Measured audio isolation and user listening:** the game's own OpenAL DLL reports
OpenAL Soft 1.24.2-unknown (the earlier Ogre "OpenAL version 2.2" line was not its
implementation version). An isolated static-buffer tone probe, with no game, GL,
video decoding or streaming refill, reproduced the periodic garble in both float
and signed-16-bit inputs through the default SDL3 backend. Identical inputs with
only `ALSOFT_DRIVERS=wasapi` changed sounded clear in both formats. The actual game
then used that backend without diagnostic injection; the user confirmed clear
in-mission music. Logs show both backends output stereo Float32 at 44100 Hz;
SDL3 uses 441/882 buffering, direct WASAPI 441/2646. SDL3 itself selects WASAPI.
Thus direct WASAPI is a validated backend workaround; the precise SDL3/Wine
buffering/timing defect is not yet located. Do not label this a float-format or
sample-rate-only bug, or infer why effects were less audibly affected.

Offline OpenAL loopback at 48 kHz reproduced the independent decoded reference
exactly for float input; signed-16-bit differed only by expected quantization
(RMS 9.09e-6). The first 20 contiguous captured splash PCM blocks matched an
independent FFmpeg decode within about 1.2e-7. These are bounded checks, not proof
of every music stream. A 44.1 kHz comparison against FFmpeg differed and is not a
validated resampler diagnosis. Copyrighted PCM remains ignored under build/tmp.

**Working launch** (Steam already online in the existing workbench prefix):

```sh
cd /Users/tailblues/mesa-wine-gl
bin/trace-tfa --graphics-bundle build/graphics-bundle-swap-fix \
  --openal-driver wasapi --wine-debug=-all,+loaddll --wait
```

Runtime: `winecx-26.3.0-mesa-gl-x86_64`, normal workbench fixes plus graphics
integration, WINEMSYNC=1. The original integration-2 bundle remains available for
comparison. Bordered 1280x720 settings remain in effect. No game DLL/shader/music
replacement was used. The stock workbench `tfa` profile is still the old baseline;
use the command above for this tested configuration.

Evidence: `docs/evidence/2026-10-31/tfa-fixes/` contains before/after probes,
LUT regression, audio backend logs/hashes, and a snapshot of the successful game
run `tfa-20261031T091238.886790Z`. Game run was left running; the snapshot is not an
exit-status claim. Exact live logs remain under build. Remaining broader work:
repeatability/performance and additional GL/application coverage, separately from
these two resolved user-visible issues.

## 2026-10-31 — Mission progression and music-only distortion clarified

**User report from the ongoing windowed run:** Coral Sea starts at midnight,
simulation time advances toward morning, and map operations appear correct.
The game itself is reported to work. Engine, wind and sea-ambience audio sound
correct; **only music playback is garbled/broken**. This supersedes the broader
"audio broken" description above/in older entries. The static loading picture
and progress-bar screen still flicker between different pictures.

These are two remaining user-reported defects in the tested session, not a
claim that all game features or general GL compatibility are complete. The
music-specific result makes music decoding/streaming/buffering a useful next
investigation target; it does not yet locate the fault or exclude shared audio
code. Preserve the working effects path and compare music separately. For the
loading flicker, reduce presentation behaviour without per-frame readbacks
before changing buffer exchange/invalidation. Keep the ongoing mission running.

## 2026-10-31 — First mission user report and runtime patch verification

**User report, current windowed run:** mission entered and is proceeding okay;
loading-image alternation persists, loading was somewhat long, and garbled audio
(possibly music) also persists during the mission and is now the worst issue.
This is initial mission evidence, not a completed mission or performance benchmark.

**Verified from runtime provenance and current patch hashes:** the Mesa Wine
runtime includes all three normal workbench Wine patches: argument injection
0004, msync contention cleanup 0007, and macOS GS last-error mirroring 0008, plus
the two opt-in graphics patches. Forced SSE PE compilation is retained. The live
run records WINEMSYNC=1, the existing Steam prefix and bundled GStreamer paths.
Diagnostic-only patches are excluded as in the normal runtime; DXMT is a separate
backend and is not involved in this OpenGL run. The trace loads native OpenAL32,
OgreOggSound and SDL3, plus Wine mmdevapi/CoreAudio. Which audio producer causes
the distorted music remains unmeasured; the old 32-bit DirectSound/x87 cause
must not be assumed. Next priority is a bounded audio comparison/reduction.

## 2026-10-31 — Museum visually confirmed; usable windowed follow-up

**Measured/user-confirmed:** `tfa-20261031T082354.135183Z`, with the persistent
launcher, reached the main menu and museum. The user's 09:28:12 and 09:29:02
screenshots show a carrier and South Dakota with ship surfaces, ocean, sky and
museum controls. The user reports textures look normal. Hash rechecks confirm
both tone-mapping GLSL files, `base_lut.png`, `tonemapping.material` and `a.program`
are unchanged from the focused probe. LUT is enabled in the installed shader;
there was no game shader workaround. This establishes museum rendering, not all
materials, dynamic LUT presets, mission correctness or frame-time performance.

The apparent museum hang recovered without intervention. A three-second sample
while its loading image persisted caught the main thread waiting during texture
upload, the Gallium worker waiting on a compute pipeline, and a Zink compiler
worker inside KK -> `mtl_new_library` -> Metal compiler scheduling. This measures
a compilation wait during the delay, not the duration or cause of every loading
pause. **User reports still open:** rapid alternation of old/new loading images
at transitions, and garbled audio with intermittent beeps. The loaded audio path
is native game OpenAL Soft 2.2 -> Wine mmdevapi/CoreAudio; no `dsound.dll` load was
found in this trace. Both runtime provenance files retain forced SSE codegen, so
the old 32-bit DirectSound/x87 diagnosis is not established for this workload.

**Archive comparison, source inspection:** March's `run_tfa.sh` disabled
`GALLIUM_THREAD` and `mesa_glthread`, inserted `nofatal_assert.dylib`, and used
its own Wine/prefix and loader paths. Its project also forced GL versions,
modified re-entrant submission/queue locking and suppressed assertions, matched
some varyings by type, and disabled the game's `applyLut`. Its bridge presented
from the same back resource rather than exchanging front/back textures. Today's
KK already updates the push-descriptor layout unconditionally (the archive's
specific descriptor fix). None of those old changes was applied blindly here.
Current Mesa's WGL frontend explicitly invalidates framebuffer state after flush;
our bridge swaps resources and bumps a stamp. A missing invalidation is a candidate
for the reported image alternation, not yet a reproduced fault. Future reduction
should avoid per-frame readbacks that could disturb cached framebuffer state.

**Display usability:** the user could not reach the museum Exit button because
the bottom was clipped. At their request only TFA PID 48245 was stopped (SIGTERM,
recorded exit -15); Steam remained running. The original settings were already
`Full Screen=No`, `Video Mode=1920 x 1080`, `border=No`: borderless windowed.
After backing up `Documents/My Games/TaskForceAdmiral/config/options.cfg`, only
resolution and border were changed to 1280x720 and Yes. Relaunch
`tfa-20261031T083438.062691Z` logs `1280x720 windowed`, `border=default`, and the
same Zink/KK renderer. **User confirmation: the window fits.** This run also uses
`--wine-debug=-all,+loaddll` to remove the very verbose per-frame WGL trace; do not
treat any performance/audio difference as a resolution-only A/B test. The game
is left running with `--wait`. Stock launch profile and shader assets are unchanged.

**Next:** a small mission now that controls are reachable; independently reduce
loading-image alternation, compare warm loading/compilation times, and diagnose
audio using its actual OpenAL path. Missions are not yet tested. Screenshots,
compressed traces/samples, settings backup and window verification are under
`docs/evidence/2026-10-31/tfa-integration/`.

## 2026-10-31 — Installed TFA LUT probe and first game launch

**Measured:** the installed, unchanged `hdr_finalToneMapping.glsl` and separately
attached `hdr_tonemap_util.glsl` compile, link and produce reference-matching
pixels through the Windows WGL bridge. `tfa-lut-20261031T081959.681228Z` completes
**139 checks, 0 failures**; its wrong-reference control completes **139 checks,
exactly 96 failures**, exit 1. The largest absolute component error is 0.000163093
against a 0.002 tolerance. The historical LUT failure is not reproduced by this
focused test; this does not settle colour grading inside the full game.

`bin/measure-tfa-lut --bundle build/graphics-bundle-integration-2` compiles the
probe with MinGW and reads game assets into ignored build output. No proprietary
shader/image assets are committed. Evidence records their hashes, the probe and
launcher source hashes, binary/compiler, graphics bundle and Wine provenance.
The probe reuses the 33-check WGL window test, then adds 106 checks:

- Sixteen HDR input colours (black, low values, primaries, greys, mixed colours,
  highlights) are read from an SSBO by a synthetic compute shader, written to an
  RGBA32F image and sampled by the actual TFA fragment program after a texture-fetch
  memory barrier. A second dispatch rotates inputs, detecting stale results.
- TFA's curve/exposure/white-point/saturation defaults are supplied through its
  two uniform blocks. Bloom and light-shaft input textures are zero. A scalar CPU
  reference independently calculates filmic tone mapping, sRGB conversion,
  bilinear LUT sampling with the shader's Y flip, and blue-slice interpolation.
- The actual 1024x32 `base_lut.png` is tested with one level and an 11-level
  generated mip chain; exact base-level upload/readback is checked. A diagnostic
  RGB-inverted copy verifies that changing the LUT changes output. The game
  asset is never edited. All three variants use clamp and bilinear filtering.
- The negative control offsets the expected RGB values by 0.125 and must fail
  every one of the 96 final pixel comparisons while completing the same path.

**Scope:** actual final-pass GLSL and LUT, synthetic vertex/compute producer,
controlled uniforms/textures and a float FBO. This does not test all TFA shaders,
its actual culling compute programs, nonzero bloom/shafts, OGRE's texture upload
choices, museum scenes or missions. No Wine/Mesa driver patch was needed here.

**First game launch, measured:** `tfa-20261031T082132.514242Z` used the new
runtime/bundle directly with the existing live `steam-winecx` Steam session.
The stock game/profile were unchanged, no second Steam client was started.
Ogre reports GL 4.6 and Zink/KosmicKrisp; the bridge logs its render fence and
first GPU presentation, followed by repeated swaps. Loading-screen resources and
project resources are parsed. **User report:** TFA appeared to run normally and
was loading toward the main menu, then disappeared. No interactive menu, museum,
mission or in-game LUT correctness has yet been confirmed.

The runner's 50-second snapshot recorded the process still alive, but the trace
stopped about two seconds after the wrapper exited and a later process check
found no TFA or crash reporter. There is no crash diagnostic in the captured tail.
**Hypothesis, not established:** managed tool-session cleanup ended the orphaned
process. The launcher now supports `--wait` to stay alive until TFA exits and
record its final status. Relaunch `tfa-20261031T082354.135183Z` uses that option.
Some Ogre script warnings mention missing parameters/resources; their effect is
unresolved and they are not evidence of a driver compiler failure by themselves.

The capture-probe timeout is now 240 seconds when holding a window for 90 seconds,
allowing startup time as well; normal runs retain 120 seconds. A new held run has
not yet verified that timeout adjustment.

**Next:** verify the persistent game launch reaches an interactive menu, then
museum and representative missions with LUT enabled; capture concrete rendering
failures before changing the driver. Evidence is in
`docs/evidence/2026-10-31/tfa-integration/`.

## 2026-10-31 — First Wine bridge integration measurements

**Measured:** an x86-64 Windows probe runs through WineCX -> Mesa/Zink ->
KosmicKrisp -> Metal with GL 4.6 core / GLSL 4.60, without version/extension
overrides. The complete controlled run passes all 33 positive checks. This is
Windows API/rendering and presentation-submission evidence. **Visually confirmed
from the user's screenshot (2026-10-31 08:58:15 local):** the Wine integration
probe is visible with blue/yellow above red/green, matching the expected colours
and orientation. This confirms this probe's onscreen output, not a game or general
GL compatibility result. The desktop tool itself still could not capture it.

The visual-confirmation run `wine-gl-20261031T075719.775217Z` reached
`PASS 60 consecutive GPU presentations` and `CAPTURE_READY`, but the runner
subsequently reported `exit=timeout` during the requested hold; there is no final
summary. Retain this as visual evidence, not another completed 33/33 run. The
prior completed controlled matrix remains the automated pass evidence. The user
initially reported no window, then supplied the visible probe screenshot; the
cause of the initial delay has not been measured.

### Isolated implementation and ownership

- `bridge/` contains the versioned C API and Mesa frontend. Selected drawable and
  context code was adapted from the archive, with forced versions removed and
  current Mesa API changes applied. No archive build artifact is used.
- `patches/wine/series` adds a separate opt-in macOS GL backend and strict context
  attribute handling to the pinned WineCX 26.3.0 source. `MESA_WINE_LIB` selects
  it; failed loading does not silently select Apple GL. Existing Wine Metal view
  helpers are reused. The Apple backend remains the default.
- `patches/bridge.series` enables Metal surfaces and supported swapchain-maintenance
  presentation fences in Zink. KK remains the previously measured four-patch
  experiment. New Wine/Mesa checkouts are `src/winecx-bridge` and `src/mesa-bridge`.
- The workbench builder accepts an explicit additional patch series, verifies
  actual source contents against the reconstructed complete series, and records
  patch hashes. Runtime: `winecx-26.3.0-mesa-gl-x86_64`. Its dependency and both-PE-
  architecture checks pass. The original runtime, TFA profile and game remain
  the baseline.

### What the probes establish

`wine-gl-20261031T071023.401277Z` is the final controlled run:

- Positive: exit 0, **33 checks, 0 failures**. This includes actual 4.6 core
  creation through WGL, Zink/KK identity, GLSL shader drawing, all four quadrant
  readbacks at 640x480 and 480x320, front-buffer contents after swaps, a shared
  texture and GL fence across two contexts, and 60 consecutive successful
  presentations. Six additional alternating-colour frames are checked without
  resetting the viewport or rebinding the framebuffer, program, VAO or context.
  Resize is tested with an explicit context rebind.
- Wrong expected output: exit 1, **33 checks, exactly 17 failures**. Pixel alpha
  and one expected shared-texture byte are deliberately changed; execution must
  reach the full summary. An early crash cannot satisfy this control.
- Missing bridge: exit 2 at setup, with the expected library-load diagnostic.
- Apple backend on the same experimental Wine runtime: exit 2 at the requested
  4.6 context. It does not satisfy the Mesa renderer or presentation checks.

A native bridge window preflight completed four GPU presentations and clean
teardown independently of Wine. Presentation uses GPU image blits; it does not
copy frames through CPU readback. Readbacks in the probes are correctness checks.
The initial presentation code is deliberately serialized with Mesa completion and
explicit presentation fences. No frame-time or throughput claim follows.

`bin/package-graphics` produced a roughly 212 MiB development graphics bundle.
Its non-system dylib dependencies and Vulkan loader are bundled, rewritten to
relative paths and ad-hoc signed; its ICD uses a relative library path. A copy
under `/private/tmp/mesa-wine-gl-relocated-20261031` created a native 4.6 context
and returned the expected red pixel with library-path overrides removed. The
loaded-image list confirms the graphics dylibs came from the relocated directory.
The final Windows matrix used a fresh relocated copy of the render-fence-fixed
bundle at `/private/tmp/mesa-wine-gl-relocated-fenced-20261031`. **The surrounding Wine runtime
still embeds its installation path; this is not yet a relocatable Wine release.**

### Bring-up observations and limits

- Compilation exposed the archived frontend's removed `get_param` callback and
  missing current Zink include paths. The new frontend uses `broken_invalidate`
  and derives includes from the current Mesa/Zink compile commands.
- Present semaphore reuse was changed to use the driver's advertised presentation
  fences, rather than assuming a submission fence covers presentation completion.
- An extended capture attempt reached all rendering checks but exceeded the
  runner's timeout while holding the window. The hold was changed from a fixed
  count of sleeps to a wall-clock deadline. This timed-out run is retained as
  `wine-gl-20261031T065658.740653Z`, not counted as a pass. The earlier completed
  positive run is `wine-gl-20261031T065453.106922Z`. The corrected wall-clock
  hold subsequently completed with exit 0 in `wine-gl-20261031T070510.674288Z`.
- Source review found that `ST_FLUSH_WAIT` does nothing when `st_context_flush`
  receives a NULL fence output. The initial bridge had that combination. This
  was not a measured rendering failure: the expanded alternating-frame probe
  already passed 33/33 against the earlier bundle (`wine-gl-20261031T070819.534121Z`).
  The final bridge instead requests a render fence, calls `fence_finish`, checks
  its result, releases the fence, and only then accesses the Zink image. The
  final Windows logs record successful completion before first presentation.
- An unchanged full Wine rebuild recognizes both overlapping graphics patches
  as already applied, passes exact source verification and runtime checks, and
  produces a Wine driver byte-identical to the tested one.
- The frontend exposes only a BGRA8 double-buffered depth24/stencil8 window
  format. MSAA/sRGB/float window formats, pbuffers, front-buffer presentation,
  configurable swap intervals, broader resize behaviour and multi-window/thread
  lifetime behaviour remain outside this measurement. Wine advertises some WGL
  extensions globally even when this frontend offers no corresponding formats.
- Assertions remain enabled. Existing KK feature limitations still apply. No
  TFA shaders, LUT assets, museum or missions have run on this bridge yet.

**Next:** exercise actual TFA shader and material dependencies, especially
compute-to-draw synchronization and the LUT pass. Separately, give the capture
hold sufficient runner timeout and verify clean completion. Preserve the stock TFA profile for baseline comparisons and select the
experimental runtime explicitly for application experiments. Evidence is under `docs/evidence/2026-10-31/wine-integration/`;
reproduction and the interface contract are in `bridge/README.md`.

## 2026-10-31 — RGB32 texel buffers and combined-stack recheck

**Measured:** the RGB32 uniform-texel-buffer experiment passes 48/48 Vulkan
compute checks and 48/48 GL-to-Zink fragment/readback checks. With capture,
custom-border and RGB32 experiments enabled together, Mesa reports GL 4.6 core
and compatibility / ES 3.2. Actual GL 4.0, 4.3 and 4.6 core context requests
succeed with no version/extension overrides and no missing extension in Mesa's
desktop version-gate list. This is not full GL conformance or a TFA result.

### Implementation and controlled format tests

`0004-kk-rgb32-texel-experiment.patch`, enabled by
`MESA_KK_EXPERIMENTAL=rgb32_texel`, implements **uniform** RGB32 float, signed
integer and unsigned integer buffer reads. It does not advertise RGB32 storage
texel-buffer or image-format support. Buffer descriptors retain the native Metal
texture ID and add the emulated view's GPU base address and element count. NIR
selects native texture access or three scalar 32-bit global loads, supplies the
missing alpha component, and handles buffer-size queries. The compiler option
is included in shader cache keys.

The test matrix covers R32, RG32, RGB32 and RGBA32 for all three scalar types;
zero/nonzero aligned view offsets; four/seven-element ranges; first, middle and
last texel reads; shader size queries; and two updates using the same view.
Exact integer values above float's exact-integer range are checked as raw bits.
Readback guards remain unchanged; the Vulkan probe also verifies its input
buffer was not modified. The GL probe reads an integer framebuffer, independently
of the Vulkan compute/SSBO path.

- Baseline and experiment-disabled Vulkan drivers pass all 36 native-format
  cases and report RGB32 unavailable. The native wrong-output control fails
  36/36, confirming it reaches the tested path.
- RGB32-enabled Vulkan and GL runs pass 48/48, including native-format controls;
  wrong-output runs fail 48/48. GL native-only runs pass 36/36 and their control
  fails 36/36.
- A native-only cache saved by this same experimental driver is rejected for
  all three shader types when RGB32 is enabled and compilation is forbidden.
  Recompilation succeeds. A fresh process reloads the resulting RGB32 cache
  with compilation forbidden and passes all 48 checks. Thus the cache-isolation
  control does not merely compare unrelated driver build UUIDs.
- The first driver build caught an incorrect NIR builder signature; it was
  corrected before GPU measurements. Assertions remain enabled.

### Step 2: measured capability and regression recheck

`bin/measure-gl-gates` provides a repeatable context matrix:

- Unmodified baseline and experiment-disabled driver: GL 3.3 request fails.
- Capture plus custom borders, without RGB32: GL 3.3 succeeds, GL 4.0 fails.
- Capture plus custom borders plus RGB32: GL 3.3, 4.0, 4.3 and 4.6 requests
  succeed; reported maximum is 4.6 core/compatibility with GLSL 4.60.

The combined stack also passes the 26 rendering checks, two GL capture/overflow
checks, and 39 Vulkan capture/query checks, including saved-cache reloads and
independent wrong-data/wrong-query controls. These are focused correctness
measurements, not a claim about all operations exposed by the reported version.

**Still measured unsupported:** upstream `zink_check_requirements` continues to
reject every desktop GL Vulkan profile even though it exits zero. Its combined
report still lists incremental presentation, non-solid fill, image-derived
border swizzles and rectangular-line features. RGB32 and transform-feedback
capability failures are removed. Actual Mesa version gates and Vulkan profile
requirements are different checks; preserve both results.

Remaining limits from the capture milestone still apply (including TES-only
capture, adjacency, primitive restart, multiple buffers/streams and full
pipeline-switching coverage). RGB32 bounds checks are implemented, but the
current matrix does not validate out-of-range/robustness behavior, descriptor
indexing, storage writes, 16-bit shader results, or large-buffer stress.
Wine integration, presentation, the actual TFA shaders/LUT and museum/missions
remain untested on the new stack. The baseline source and workbench runtime/game
were not changed.

**Next:** assess the remaining profile gaps against the actual TFA workload and
begin an isolated Wine bridge/presentation experiment. A successful 4.6 context
is a prerequisite now measured, not sufficient evidence to package this as
complete modern GL support. Preserve the unmodified baseline for comparisons.

Final evidence under `docs/evidence/2026-10-31/rgb32/`:

- `texel-20261030T235623.518560Z`: native/RGB32 Vulkan, cache and negative controls.
- `gl-texel-20261030T235625.741744Z`: native/RGB32 GL and negative controls.
- `gl-gates-20261030T235631.094026Z`: context/version matrix.
- `render-20261030T235634.575186Z`: combined rendering regression.
- `gl-xfb-20261030T235636.388341Z`: combined GL capture regression.
- `xfb-20261030T235504.316138Z`: combined Vulkan capture/query regression.
- `vulkan-20261030T235638.416287Z`: full Vulkan reports and upstream profiles.

Each runner records selected environment, exit status and provenance; probe
runners reject stale source/binary manifests. Generated binaries and pipeline
cache blobs remain in `build/`. UTC directory names still fall on October 30;
the local Europe/Berlin date is October 31.

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
