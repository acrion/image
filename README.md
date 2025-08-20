Type-safe pixel containers and foundational image-processing utilities for high-dynamic-range and scientific imaging — designed to interoperate with the **acrion** toolchain and containers.

`acrion_image` is a header-only C++ library that provides:

* **Template-based image containers** supporting multiple *bits-per-sample* (per-channel bit depth) and numeric domains (unsigned integer and floating-point).
* A **type-erased `Bitmap` façade** for runtime dispatch across sample types.
* Fast **display conversion** (window/level with gamma/log mapping) to 8-bit buffers for UIs.
* A **diagonal/edge-aware interpolation** routine that improves upon naïve bilinear sampling.
* Simple but practical **drawing and statistics** (lines, local min/max, peak finding).
* Clean **color handling** with brightness (luma) adjustments that preserve chroma.
* Zero-copy options via `stable_reference_buffer` and **OpenMP** parallelization.

It provides the in-memory format for [acrion image-tools](https://github.com/acrion/image-tools) (the [nexuslua](https://github.com/acrion/nexuslua) plugin) and the UI [acrionphoto](https://github.com/acrion/photo).

---

## Why this library?

Most imaging stacks pick one dominant pixel type and silently up/down-convert. `acrion_image` takes the opposite approach: it embraces the **native sample format**.

* **Template over sample type:** `BitmapData<T>` exists for `uint8_t`, `uint16_t`, `uint32_t`, `uint64_t`, and `double` (64-bit floating point), keeping arithmetic precise and avoiding accidental truncation in scientific workflows.
* **Runtime polymorphism when you want it:** `Bitmap` wraps the templated variants and exposes a uniform API (create, stats, conversion, etc.) while keeping the underlying type intact.
* **Display without data loss:** a high-quality, gamma/log-aware **8-bit conversion** path is provided for visualization, not computation.

Compared to [OpenCV](https://opencv.org), this is a **complementary** approach focused on correctness across **bits-per-sample** and reproducible display mapping, while still allowing a bridge to OpenCV when convenient (see below).

---

## Features at a glance

* **Containers**

  * `BitmapData<T>` with 1/3/4 channels (Gray, RGB, RGBA) and explicit channel indices.
  * Zero-copy construction from external memory through `stable_reference_buffer`.
  * Type-erased `Bitmap` that converts to/from a generic `nested_map` container for plugin I/O.

* **Bit depth & numeric domain**

  * Supported sample types: `uint8_t`, `uint16_t`, `uint32_t`, `uint64_t`, `double`.
  * `Bitmap::Depth()` returns **bytes per sample**; negative values denote floating point (`-8` for `double`).

* **Interpolation (edge/diagonal aware)**

  * `interpolation::Do(...)` mixes corner and edge samples with **distance-based weights** to reduce directional bias and stair-stepping typical of bilinear sampling. It blends two estimates (edge-mixed and corner-mixed) with a data-dependent weight for robust results.

* **Color & brightness**

  * `Color<T>` with luma (`Gray()`) using standard 0.299/0.587/0.114 coefficients.
  * `WithBrightness(Y)` adjusts **luma while preserving chroma** via a YUV-like transform.

* **Display conversion**

  * `BitmapData<T>::ConvertToDepth8(...)` maps arbitrary dynamic range to 8-bit:

    * **Window/level** via `SetMin/MaxDisplayedBrightness`.
    * **Gamma/log mapping** with a precomputed table (linear when `gamma == 0`).
    * Outputs **BGRA** for RGB/RGBA sources (UI-friendly) and single-channel for gray.
    * Optional ROI and **as-you-scale** conversion (keeps aspect ratio, letterboxes).

* **Image ops & utilities**

  * `AbsoluteDiff` (per-channel, saturating).
  * Region stats: `Max/Min`, `MaxGray`, `MinGray` (+ optional average, stddev, second brightest/darkest).
  * Drawing primitives (Bresenham lines; vector-based drawing).
  * `ContainsColors()` (detects chroma vs gray).
  * OpenMP-accelerated loops.

* **Ecosystem integration**

  * Converts to/from a **generic container** (`nested_map<xpod, xpod>`) for **nexuslua** plugins.
  * Alias for `Magick::Quantum` to match ImageMagick’s quantum depth.
  * Plays nicely with the **acrion container** that ships **cfitsio** + **ImageMagick** for I/O; `acrion_image` itself remains focused on in-memory processing.

---

## Relationship to `acrion_image_tools` and the acrion stack

* **`acrion_image`**: core pixel containers + math (this repo).
* **`acrion_image_tools`**: algorithms and I/O modules built on top of `acrion_image`, pluggable into **nexuslua**.
* **Containerization**: an **`acrion_image` container image** provides **cfitsio** and **ImageMagick** in one place; both **acrionphoto** (UI) and **nexuslua** (CLI) consume that, exchanging pixels in the **`acrion_image`** in-memory format.
* **UI**: `ConvertToDepth8` and brightness windows are what the UI uses to render consistent previews across wildly different source bit depths.

---

## Quick start

### Add to your CMake project

```cmake
include(FetchContent)

FetchContent_Declare(
  acrion_image
  GIT_REPOSITORY git@github.com:acrion/image.git
  GIT_TAG        master # or a release tag
)
FetchContent_MakeAvailable(acrion_image)

add_executable(app main.cpp)
target_link_libraries(app PRIVATE acrion_image) # transitively links OpenMP and Cbeam
```

> `acrion_image` is an **INTERFACE** target; headers are under `include/`.
> Dependencies: **OpenMP** (for parallel loops) and [Cbeam](https://github.com/acrion/Cbeam) (containers, memory helpers).

### Create, fill, and convert for display

```cpp
#include "acrion/image/bitmap.hpp"
#include "acrion/image/color.hpp"

using namespace acrion::image;

// Create a 16-bit RGB image (1024x768)
Bitmap rgb16(BitmapData<uint16_t>(1024, 768, 3));

// Paint something
auto& data = rgb16.Get<1>(); // index 1 == uint16_t
data.Draw(10, 10, 200, 100, Color<uint16_t>(65535, 0, 0)); // red line

// Set display window and convert to 8-bit BGRA for preview
data.SetMinDisplayedBrightness(0);
data.SetMaxDisplayedBrightness(40000);
std::unique_ptr<uint8_t[]> preview(
  data.ConvertToDepth8(/*gamma*/0.6, /*x*/0, /*y*/0, /*w*/0, /*h*/0, /*scaledW*/800, /*scaledH*/0)
);
// 'preview.get()' now points to BGRA pixels ready for your UI framework.
```

### Runtime type handling

```cpp
void render(const Bitmap& img) {
  auto* rgba = img.ConvertToDepth8(0.5);
  // ... upload to texture, then:
  delete[] rgba;
}
```

---

## Interpolation, briefly

`interpolation::Do(dx, dy, ...)`:

* clamps the request to valid bounds,
* samples the four **corners** (`a,b,c,d`) and the four **edge midpoints**,
* builds two estimates:

  * `col1`: weighted mix along **edges** (top/bottom/left/right),
  * `col2`: weighted mix of the **four corners**,
* blends `col1` and `col2` with a data-dependent weight derived from distances to edge midpoints.

This **diagonal/edge awareness** reduces artifacts like directional bias and over-blurring seen in plain bilinear interpolation, while keeping the cost close to bilinear.

---

## Color and brightness mapping

* `Color<T>::Gray()` implements luma with `(0.299, 0.587, 0.114)` coefficients.
* `WithBrightness(Y)` converts to a YUV-like space and replaces luma `Y`, preserving chroma — useful for exposure/contrast adjustments without hue drift.
* Display conversion (`ConvertToDepth8`) performs **window/level** first, then an optional **gamma/logarithmic** mapping:

  * `gamma == 0`: linear mapping.
  * `gamma != 0`: a precomputed table mixes linear and logarithmic responses, tuned for very large dynamic ranges (e.g., scientific/astronomical sensors).

---

## OpenCV bridge (optional)

There’s a lightweight bridge in `bitmap_data.hpp` that can be enabled to interoperate with OpenCV `cv::Mat` (the operators are in place but commented). If you enable OpenCV in your build, you can re-introduce these conversions (or add explicit helpers) to exchange buffers without copies:

```cpp
// Example sketch (enable OpenCV and adapt as needed):
// cv::Mat view(img.Height(), img.Width(),
//              CV_MAKETYPE(cv::DataType<T>::depth, img.Channels()),
//              img.Buffer());
```

> This bridge is intentionally **opt-in** so the core stays lean and OpenCV-free by default.

---

## Integrating with acrion containers & tools

* **I/O** (FITS, TIFF, PNG, etc.) is handled by the **acrion container** image (cfitsio + ImageMagick) and the higher-level **`acrion_image_tools`** modules.
* `Bitmap` converts to/from a **generic `nested_map`** so your **nexuslua** plugins can pass images across process boundaries and modules consistently.
* From the UI (**acrionphoto**) to the CLI (**nexuslua**), the shared memory representation is **`acrion_image`**.

---

## Testing

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Unit tests cover color math and basic behaviors; extend as needed.

---

## FAQ

**Is this a replacement for OpenCV?**
No — it’s a **pixel-type-first container & math layer**. Use it alongside OpenCV if you like; enable the optional bridge for `cv::Mat` views.

**Why does `Depth()` return negative for some images?**
Negative **bytes-per-sample** indicate a **floating-point** sample type (`double` → `-8`), while positive values indicate unsigned integer types (`1,2,4,8`).

**What order does display conversion use for RGB?**
`ConvertToDepth8` outputs **BGRA** for RGB/RGBA inputs and a single channel for grayscale, which plays nicely with many UI frameworks.

**Where are file readers/writers?**
Purposefully out of scope. Use `acrion_image_tools` together with the **acrion** container (cfitsio + ImageMagick) for robust I/O.

---

## Minimal example (complete)

```cpp
#include "acrion/image/bitmap.hpp"
#include "acrion/image/color.hpp"

using namespace acrion::image;

int main() {
  Bitmap img(BitmapData<uint8_t>(256, 256, 3));
  auto& d = img.Get<0>(); // uint8_t

  // Draw two lines
  d.Draw(10, 10, 200, 10, Color<uint8_t>(255, 0, 0));
  d.Draw(10, 10, 10, 200, Color<uint8_t>(0, 255, 0));

  // Visualize with gentle gamma and windowing
  d.SetMinDisplayedBrightness(0);
  d.SetMaxDisplayedBrightness(255);
  uint8_t* bgra = d.ConvertToDepth8(0.4, /*x*/0, /*y*/0, /*w*/0, /*h*/0, /*scaledW*/256, /*scaledH*/256);

  // ... use BGRA pixels ...
  delete[] bgra;
  return 0;
}
```

---

## Community

Have questions about the library, want to share what you've built, or discuss image processing techniques? Join our community on Discord!

💬 **[Join the acrion image Discord Server](https://discord.gg/FeWM5s5jKm)**

---

## Acknowledgements

* Built on **Cbeam** (containers, memory utilities) and **OpenMP**.
* Integrates with **ImageMagick** (quantum depth) and **cfitsio** via the acrion container image.

---

If you’re integrating `acrion_image` into a larger pipeline (UI or CLI), have questions about the interpolation model, or need commercial support, feel free to reach out.
