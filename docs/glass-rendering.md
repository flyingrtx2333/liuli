# Glass-inspired native rendering (v0.2)

This is a Linux, CPU-rendered material study inspired by translucent glass UI.
It is **not Apple's Liquid Glass implementation**, an iOS application, a native
window, a GPU benchmark, or evidence of reduced power consumption.

## Direct runtime contract

`native/glass_renderer.h` provides:

```cpp
void explicit_ui::render_glass(const ViewModel&, const std::string& output_path);
GlassMetrics explicit_ui::glass_metrics(); // cache_builds, render_frames
```

The host retains a generated C++ model and passes it directly to Cairo drawing.
`counter`, `list`, and runtime statistics are read at render time. There is no
JSON snapshot or Python bridge between the live model and renderer. The host,
not the renderer, decides whether a view change warrants a redraw. A PNG is a
headless frame; the illustrated “Updated via stdin” pill is a status label, not a mouse hit target.
Counter events arrive through the host command protocol instead.

The counter's optional `Count: ` label is stripped for large numeric typography.
At most seven current list rows are drawn, in order; a caption explicitly reports
clipping when more rows exist. Filtering/windowing remains the model's job.
There is no fake search query: the results bar is labeled “Filtered / windowed
results” because the model does not currently export a query view.

## Material pipeline and cache

The fixed 1120 × 840 procedural backdrop has colored fields and light ribbons.
Three horizontal/vertical sliding-window blur passes (radius 12) blur the actual
backdrop. The panel samples that blurred image with a bounded, edge-weighted
coordinate displacement, then blends in white tint. This creates a genuine
backdrop-filtered, refraction-like approximation, not physically based optics.
Rounded clipping, translucent inner cards, perimeter highlights, and a layered
soft shadow finish the material.

The composited backdrop, material, and static chrome are built **once per
thread** on first render. Subsequent frames reuse that cached Cairo surface and
paint current values onto a new output surface. This is full-frame CPU redraw
on a visible change, **not dirty-region rasterization**. `cache_builds` counts
completed cache constructions; `render_frames` counts successfully written
PNGs. Reading metrics does not build the cache. No pixel-effect rebuild occurs
for counter/list updates. Static effects would need invalidation if configurable
geometry/theme/backdrop were added later; these are intentionally fixed today.

Cairo surfaces and contexts use RAII. Pixel buffers are fixed to the bounded
canvas and released after cache construction. Text processing is bounded to
512 input bytes per displayed string, validates UTF-8, replaces malformed bytes,
and uses clipping to contain long labels. Cairo errors and PNG write failures
propagate as exceptions; failed output writes do not increment `render_frames`.
The fallback `cairo_abi.h` declares only the used public Cairo ABI when system
headers are absent. Link with the installed `libcairo.so.2`; no added package
download is required.

## Verification scope

Integration checks should establish that initial and changed frames use actual
model values, a repeated unchanged event creates no additional frame, and
multiple render calls retain `cache_builds == 1`. Compare before/after PNGs
visually and read the host's state/renderer counters. These checks do not prove
native window interaction, accessibility, iOS fidelity, GPU performance, or
energy savings.

### Verified in this cloud workspace

- Warning-clean C++17 compile (`-Wall -Wextra -Wpedantic -Werror`).
- Real generated model/host: count 0 → 7, unchanged 7 → 7, then query
  `00007`; render seam counts 1 → 2 → 2 → 3. Only three PNGs were produced.
- Visually checked `evidence/glass-qa/redraw-0.png` and `redraw-2.png`:
  the final frame shows counter 7 and the actual sole row `Item 00007`.
- Separate renderer edge-case harness: two successful frames give one cache
  build, malformed UTF-8 is replaced without Cairo failure, and an output write
  failure does not increase the successful-frame counter. The minimum signed
  64-bit counter remains within its box using adaptive font sizing.

On restricted Linux hosts, set `XDG_CACHE_HOME` to a writable workspace cache
directory to avoid Fontconfig cache warnings; this does not change permissions
or require system installation.
