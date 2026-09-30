# AMD shared memory in the CPU panel

Author: **ajfero**.

This contribution was developed with AI assistance. Any upstream submission must
be labelled **[AI generated]**, as required by CONTRIBUTING.md. Existing upstream
copyright notices and the Apache-2.0 license are preserved.

On an AMD APU, the GPU summary can report only a small BIOS-reserved VRAM pool even
when the driver also manages a much larger pool of shared system RAM. This change
adds a separate **GTT** line directly below the GPU summary in the CPU
panel. It shows a usage bar and used/total bytes from ROCm SMI's GTT counters.
The new counters are currently collected through ROCm SMI on Linux; the AMD sysfs
fallback and other GPU backends do not populate this row.

The total is a driver-reported budget, not currently free RAM or additional
physical VRAM. No capacity is hardcoded and nothing changes the BIOS, kernel
parameters, memory allocation or GPU settings. Shared usage is never added to the
existing VRAM counter or the system RAM counter.

## Display settings

In Options > CPU, `show_gpu_shared` controls the new line (enabled by default).
It follows `show_gpu_info`: Auto hides a GPU's CPU summary when its dedicated GPU
panel is open; On keeps the summary visible. The dedicated GPU panel is unchanged.
Unsupported GPUs get no extra line. A failed sample is shown as N/A instead of a
stale number or a false zero. The label is always GTT; narrow panels use compact units.

## Build and test without replacing the installed btop

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBTOP_GPU=ON -DBUILD_TESTING=ON
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
./build/btop
```

For a local preview that preserves the normal btop configuration, copy
`~/.config/btop/btop.conf` to a separate XDG config directory before launching.

Installing the resulting binary under the existing `btop` name keeps the same
command and configuration. Back up the previous executable before replacing it.

Tests cover mixed GPU visibility, missing and zero samples, bounded meters,
large counters, narrow/wide formatting, and separation from reserved VRAM.
