# SpectroFX — Real‑Time Audio Effects via Graphical Spectrogram Manipulation

SpectroFX is a stereo VCV Rack module that treats the **magnitude spectrum like an image**, applies OpenCV-style operations to it, and then reconstructs audio using selectable phase engines (RAW, classic Phase-Vocoder, or PV-Lock). The UI shows a live spectrogram and a band-select overlay so you can confine processing to a frequency range in real time. &#x20;



## Why

Many classic image operators map beautifully onto spectral magnitude. With SpectroFX you can blur, sharpen, edge-enhance, emboss, mirror or stretch spectra—**per channel, with CV control**—and keep transients coherent via phase models designed for synthesis continuity. &#x20;



## Features

* **Image-style spectral FX (per-bin, 1D over frequency):** Blur, Sharpen, Edge Enhance, Emboss, Mirror, and Spectral Stretch. Each effect has independent L/R knobs plus optional CV. CV is mapped at **±10 V → ±1.0** intensity.&#x20;
* **Spectral Gate** to attenuate content under a relative threshold.&#x20;
* **Phase engines:**

  * **RAW** (analysis phase passthrough)
  * **PV** (phase-vocoder with instantaneous frequency)
  * **PV-Lock** (identity phase locking around spectral peaks)
    Griffin–Lim is intentionally **not used** in this project. &#x20;
* **Band-select overlay (Mask2D):** click-drag on the spectrogram to choose the frequency band where FX apply; lock-free UI↔DSP swap for glitch-free audio. &#x20;
* **Live spectrogram UI**, panel drawn entirely in code (no SVG).&#x20;
* **Stereo I/O:** BYPASS L/R (dry) and PROCESSED L/R (wet).&#x20;



## Signal Flow (DSP)

1. **STFT** with periodic √Hann, `N = 1024`, `H = N/2` (guaranteed COLA(Constant OverLap-Add)). Latency ≈ `N` samples.&#x20;
2. **FFTW** forward transform → **OpenCV** FX on magnitude → **phase engine** synthesizes complex spectrum → **IFFT**.&#x20;
3. **Overlap-Add**, soft limiter, and DC-block for clean output.&#x20;



## Controls & I/O

* **Per-channel knobs (L/R):** BLUR, SHARPEN, EDGE, EMBOSS, MIRROR, GATE, STRETCH. Each has a matching **CV input**. CV adds `0.1 × voltage` to the knob value (±10 V → ±1.0 range).&#x20;
* **Phase Mode** (RAW / PV / PV-Lock) via context menu; on-panel LED + text indicator.&#x20;
* **Band-select overlay:** click-drag on the spectrogram to set **low/high** bounds; toggle and presets in the context menu. Thread-safe mask swapping avoids locks on the audio thread. &#x20;
* **Jacks:**

  * **Inputs:** IN L, IN R
  * **Outputs:** BYPASS L/R (dry through), PROC L/R (processed)&#x20;

**Quick patch:** Feed audio to **IN L/R**, monitor **PROC L/R**. Use the overlay to limit FX to a band (e.g., mids), then raise **SHARPEN** or add a touch of **BLUR** for tone shaping.&#x20;



## Build

**Prerequisites**

* VCV Rack SDK installed (set `RACK_DIR`)
* **FFTW3** (with threads) and **OpenCV** available to your toolchain
* A C++17 compiler

**Steps**

```bash
# from your Rack plugins dir
git clone <this-repo> SpectroFX
cd SpectroFX
make RACK_DIR=/path/to/Rack-SDK
```

On success, VCV Rack will discover a module named **“SpectroFX”** registered by the plugin at load time.&#x20;



## Architecture Notes

* **PhaseEngine** keeps per-channel history of analysis/synthesis phase; PV computes expected phase advance and unwraps deviations; PV-Lock propagates peak phases to neighbors for crisper transients.&#x20;
* **Mask2D** holds a `[HIST × K]` buffer pair (front/back). The UI writes to **back** and marks it dirty; audio thread atomically swaps to **front** at frame start—simple, low-cost, and lock-free for `HIST≈256, K≈513`.&#x20;
* **UI** is drawn with NanoVG (no external SVG assets) and includes a heatmap-style spectrogram plus in-panel I/O groupings.&#x20;
* **Performance:** FFTW thread pool is initialized once; plans use two threads. Soft-limiter and DC-block help keep levels sane.&#x20;



## Roadmap

* Additional spectral operators and smarter mask painting tools
* Quality-of-life UI refinements and presets



## Acknowledgments

Built with **VCV Rack**, **FFTW**, and **OpenCV**. Thanks to the open-source communities behind these projects. (Implementation references in this repo point to the relevant files.)




## Contributing

Issues and PRs are welcome—especially bug reports with minimal patches/recordings and platform details. For code changes, keep UI thread and audio thread responsibilities clearly separated (see `Mask2D` and the DSP pipeline for patterns). &#x20;



## Module ID

The module model is registered as **“SpectroFX”** when the plugin initializes.&#x20;
