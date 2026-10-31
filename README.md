# mesa-wine-gl

Modern OpenGL support for Wine on Apple Silicon macOS, using existing graphics
libraries. The initial architecture is Wine OpenGL → Mesa/Zink → KosmicKrisp →
Metal. Task Force Admiral is the first end-to-end workload.

## Success criteria

- The unmodified game renders correctly through the new GL stack: museum and
  representative missions, including colour grading, post-processing, and ship
  textures. A visible menu or an advertised GL version is not sufficient.
- Focused probes exercise the required GL features, resource sharing,
  synchronization, and presentation, with controls that can detect failures.
- Builds have pinned source revisions and recorded toolchains, and package their
  runtime dependencies. No dependency on the archived project's built artifacts.
- The integration works with the workbench's WineCX environment; compatibility
  with shipped CrossOver is a separate validation target, not assumed from ABI names.

## Project boundaries

This repository owns new graphics-library checkouts (`src/`), patches, bridge
implementation, build recipes, and measurements. `src/`, `toolchains/`, and
`build/` are local outputs and are ignored by Git. Pins live in `sources.lock.json`.

The sibling [wine-workbench-macos](../wine-workbench-macos/README.md) supplies
WineCX runtime builds, game prefixes, the shared Steam library, and launch tooling.
Use a separately named runtime for bridge experiments. Never run two Steam clients
against the shared game library.

[mesa_gl_to_metal](../progs/mesa_gl_to_metal/PROJECT.md) is historical reference.
Its old test results, patches, and hypotheses are not current guarantees. New
experiments start from clean sources; old changes require individual justification.

## Investigation tools

- `bin/inventory-tfa`: read-only JSON inventory of the installed TFA build, DLL
  hashes, GL name strings, and shader feature candidates. This is static evidence,
  not a dynamic call trace.
- `bin/build-mesa --configure-only`: configure the pinned, unmodified x86_64
  Mesa baseline. `bin/build-mesa` also builds it (`JOBS=6` by default).
  The Python environment is `toolchains/python-x86_64`; dependencies are pinned
  in `config/python-build-requirements.txt`. The native toolchain configuration
  is `config/mesa-x86_64.ini`.

- `bin/measure-vulkan`: record GPU capabilities and upstream Zink profile results
  with the fresh driver, in default and experimental custom-border configurations.
  It saves separate logs and provenance under `build/`. Check profile results:
  upstream's checker can exit zero even when a profile is unsupported.
- `bin/build-gl-probe`: build the native surfaceless context probe using Mesa's
  generated compiler flags. Run `build/gl-baseline 40` for a GL 4.0 core request,
  with `VK_DRIVER_FILES` pointing at the build's development ICD.
- `bin/measure-gl`: run native shader/FBO/texture/mip/fence readback checks and
  a deliberately wrong expected-pixel control. Build with `bin/build-gl-probe`
  first. `--experimental-driver` selects the isolated KK build without enabling
  experimental capture.
- `bin/build-kk-experiment`: build the isolated, explicitly patched KK worktree;
  `--verify-only` checks its source against the pin and `patches/series`.
- `bin/build-xfb-probe` and `bin/measure-xfb`: build and run Vulkan
  vertex/GS capture and query tests, cache-reload checks, disabled-feature controls,
  and independent wrong-output/wrong-query controls. The runner
  supplies the experimental flag only for capture scenarios and rejects stale
  probe binaries. See [patch scope](patches/mesa/README.md).
- `bin/measure-gl-xfb`: exercise capture and overflow through native GL/Zink,
  with disabled-feature and wrong-output controls. Build with `bin/build-gl-probe`.
- `bin/build-texel-probe` and `bin/measure-texel`: build and measure native/RGB32
  uniform texel buffers through Vulkan, including type/range/update checks and
  compiler-option cache controls. `--baseline-only` runs the unmodified controls.
- `bin/measure-gl-texel`: run the independent GL/Zink integer-framebuffer checks;
  build with `bin/build-gl-probe` first.
- `bin/measure-gl-gates`: compare actual context/version gates with experiments
  disabled and enabled. These results do not establish conformance.
- `bin/measure-gl --full-experiment`, `bin/measure-gl-xfb --with-rgb32`, and
  `bin/measure-xfb --with-rgb32`: regress the combined experiments.
- `bin/measure-vulkan --full-experiment`: record both Vulkan capabilities and
  upstream profile requirements with the isolated driver and combined flags.
- `bin/trace-tfa [--forward-compatible]`: trace a direct launch through the existing
  workbench prefix, with Steam already online. Saves runtime provenance and logs;
  a game still running at the observation timeout is left running.

## Reproduce the source/build environment

The current toolchain configuration expects x86_64 Homebrew LLVM, SPIR-V tools,
SPIR-V LLVM translator, libclc, Vulkan loader/headers, zstd, bison and Ninja, plus
Xcode and Rosetta. It uses the selected Xcode SDK explicitly. Python dependencies
are project-local:

```sh
mkdir -p src toolchains
git clone https://gitlab.freedesktop.org/mesa/mesa.git src/mesa
git -C src/mesa checkout --detach <mesa.commit-from-sources.lock.json>
/usr/bin/arch -x86_64 /usr/local/bin/python3 -m venv toolchains/python-x86_64
toolchains/python-x86_64/bin/python -m pip install -r config/python-build-requirements.txt
bin/build-mesa
bin/measure-vulkan
```

For the isolated transform-feedback development experiment (requires
`glslangValidator` in addition to the baseline dependencies):

```sh
git -C src/mesa worktree add --detach ../mesa-xfb <mesa.commit-from-sources.lock.json>
git -C src/mesa-xfb apply ../../patches/mesa/0001-kk-gs-xfb-experiment.patch
git -C src/mesa-xfb apply ../../patches/mesa/0002-kk-vertex-xfb-experiment.patch
git -C src/mesa-xfb apply ../../patches/mesa/0003-kk-xfb-query-experiment.patch
git -C src/mesa-xfb apply ../../patches/mesa/0004-kk-rgb32-texel-experiment.patch
bin/build-kk-experiment
bin/build-xfb-probe
bin/measure-xfb
bin/build-gl-probe
bin/measure-gl
bin/measure-gl --experimental-driver
bin/measure-gl-xfb
bin/build-texel-probe
bin/measure-texel
bin/measure-gl-texel
bin/measure-gl-gates
bin/measure-gl --full-experiment
bin/measure-gl-xfb --with-rgb32
bin/measure-xfb --with-rgb32
bin/measure-vulkan --full-experiment
```

The baseline builder refuses modified sources; the experimental builder accepts
only the exact declared patch series at the pinned revision. These are development
builds using recorded local dependencies; packaging a self-contained Wine runtime is
part of the integration work.

For the isolated Wine integration, bridge ABI, build commands, and graphics bundle,
see [bridge/README.md](bridge/README.md).

Read [docs/NOTEBOOK.md](docs/NOTEBOOK.md) for measured state and open questions.
