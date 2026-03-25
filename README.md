# InstaLPEQ

Free, open-source linear phase EQ plugin built with JUCE. Available as VST3, AU and LV2.

![VST3](https://img.shields.io/badge/format-VST3-blue) ![AU](https://img.shields.io/badge/format-AU-blue) ![LV2](https://img.shields.io/badge/format-LV2-blue) ![C++](https://img.shields.io/badge/language-C%2B%2B17-orange) ![JUCE](https://img.shields.io/badge/framework-JUCE-green) ![License](https://img.shields.io/badge/license-GPL--3.0-lightgrey) ![Build](https://github.com/hariel1985/InstaLPEQ/actions/workflows/build.yml/badge.svg)

## Why Linear Phase EQ?

Traditional (minimum phase) EQs alter the **phase** of the signal at the frequencies they boost or cut. This causes:
- **Phase smearing** — transients lose their shape, especially on drums and percussive material
- **Asymmetric waveforms** — the signal before and after the EQ change point don't align in time
- **Coloration** — even subtle EQ moves can change the character of the sound beyond the intended frequency adjustment

A **linear phase EQ** applies the exact same time delay to all frequencies. This means:
- **Zero phase distortion** — the waveform shape is perfectly preserved
- **Pristine transients** — drums, plucks, and attacks stay tight and punchy
- **Transparent tonal shaping** — only the frequency balance changes, nothing else
- **Perfect for mastering** — no cumulative phase artifacts when stacking multiple EQ moves
- **Ideal for parallel processing** — EQ'd and dry signals stay perfectly time-aligned when summed

The trade-off is a small amount of latency (automatically compensated by the DAW), which makes linear phase EQ unsuitable for live monitoring but perfect for mixing and mastering.

## Download

**[Latest Release: v1.3](https://github.com/hariel1985/InstaLPEQ/releases/tag/v1.3)**

### Windows
| File | Description |
|------|-------------|
| [InstaLPEQ-VST3-Win64.zip](https://github.com/hariel1985/InstaLPEQ/releases/download/v1.3/InstaLPEQ-VST3-Win64.zip) | VST3 plugin — copy to `C:\Program Files\Common Files\VST3\` |

### macOS (Universal Binary: Apple Silicon + Intel)
| File | Description |
|------|-------------|
| [InstaLPEQ-VST3-macOS.zip](https://github.com/hariel1985/InstaLPEQ/releases/download/v1.3/InstaLPEQ-VST3-macOS.zip) | VST3 plugin — copy to `~/Library/Audio/Plug-Ins/VST3/` |
| [InstaLPEQ-AU-macOS.zip](https://github.com/hariel1985/InstaLPEQ/releases/download/v1.3/InstaLPEQ-AU-macOS.zip) | Audio Unit — copy to `~/Library/Audio/Plug-Ins/Components/` |

### Linux (x64, built on Ubuntu 22.04)
| File | Description |
|------|-------------|
| [InstaLPEQ-VST3-Linux-x64.zip](https://github.com/hariel1985/InstaLPEQ/releases/download/v1.3/InstaLPEQ-VST3-Linux-x64.zip) | VST3 plugin — copy to `~/.vst3/` |
| [InstaLPEQ-LV2-Linux-x64.zip](https://github.com/hariel1985/InstaLPEQ/releases/download/v1.3/InstaLPEQ-LV2-Linux-x64.zip) | LV2 plugin — copy to `~/.lv2/` |

> **macOS note:** Builds are Universal Binary (Apple Silicon + Intel). Not code-signed — after copying the plugin, remove the quarantine flag in Terminal:
> ```bash
> xattr -cr ~/Library/Audio/Plug-Ins/VST3/InstaLPEQ.vst3
> xattr -cr ~/Library/Audio/Plug-Ins/Components/InstaLPEQ.component
> ```

## Features

### Linear Phase EQ Engine
- True linear phase processing using symmetric FIR convolution
- Zero phase distortion — the waveform shape is perfectly preserved at any gain setting
- Mathematically transparent: only magnitude changes, phase stays untouched
- FIR impulse response normalized for unity passthrough (0 dB at flat settings)
- Background thread FIR generation — glitch-free, click-free parameter changes
- DAW-compensated latency for seamless integration

### Configurable FIR Resolution
Six quality levels to balance precision vs. latency:

| Taps | Latency (44.1 kHz) | Best for |
|------|---------------------|----------|
| 512 | ~6 ms | Low-latency monitoring |
| 1024 | ~12 ms | Tracking |
| **2048** | **~23 ms** | **Default — mixing** |
| 4096 | ~46 ms | Detailed work |
| 8192 | ~93 ms | Mastering |
| 16384 | ~186 ms | Maximum precision |

Low tap counts have reduced accuracy below ~100 Hz — a warning is displayed when using 512 or 1024 taps.

### Interactive EQ Curve Display
- Logarithmic frequency axis (20 Hz — 20 kHz)
- Linear gain axis (-24 dB to +24 dB)
- Click anywhere to add an EQ node (up to 8 bands)
- Drag nodes to adjust frequency and gain in real time
- Scroll wheel over a node to adjust Q/bandwidth
- Right-click a node for band type selection or delete
- Double-click a node to reset it to 0 dB
- Combined frequency response curve with glow effect
- Individual per-band curve overlays (color-coded)
- Real-time FFT spectrum analyzer behind the EQ curves (shows live audio content)

### Band Types
- **Peak** (parametric) — boost or cut a specific frequency range
- **Low Shelf** — boost or cut everything below a frequency
- **High Shelf** — boost or cut everything above a frequency

### Auto Makeup Gain
- Automatically compensates for the loudness change caused by EQ settings
- Computed from the actual FIR frequency response (not theoretical) — accounts for FIR resolution limits
- RMS-based calculation with linear frequency weighting (matches white noise / broadband signals)
- Toggleable on/off — displays the current compensation value in dB
- Mastering-safe: fixed value based on EQ curve, no signal-dependent gain changes

### Output Limiter
- Brickwall limiter with 0 dB ceiling
- Toggleable on/off
- Prevents clipping when applying large EQ boosts
- 50 ms release time

### Drag-and-Drop Signal Chain
- Reorderable processing chain at the bottom of the GUI
- Three blocks: **Master Gain**, **Limiter**, **Auto Gain**
- Drag blocks to change processing order (e.g., put limiter before or after gain)
- Visual arrows show signal flow direction
- Chain order saved/restored with DAW session

### Controls
- **Per-band:** Frequency, Gain, Q knobs with 3D metal styling
- **Master Gain:** +/- 24 dB output level control
- **Bypass:** global bypass toggle
- **New Band:** button to add a new EQ node at 1 kHz / 0 dB
- **FIR Quality:** dropdown to select tap count / latency
- All knobs reset to default on double-click

### GUI
- Dark modern UI with InstaDrums visual style
- 3D metal knobs with multi-layer glow effects (orange for frequency/gain, blue for Q)
- Carbon fiber background texture
- Rajdhani custom font (embedded)
- Fully resizable window (700x450 — 1920x1080) with proportional scaling
- Animated toggle switches with smooth lerp
- Color-coded EQ bands (8 distinct colors)
- All fonts and UI elements scale with window size
- State save/restore — all settings recalled with DAW session

## How It Works

InstaLPEQ uses a **FIR-based linear phase** approach:

1. Each EQ band's target magnitude response is computed from IIR filter coefficients (Peak, Low Shelf, or High Shelf)
2. All band magnitudes are multiplied together to form the combined target frequency response
3. An inverse FFT converts the magnitude-only spectrum (zero phase) into a symmetric time-domain impulse response
4. A Blackman-Harris window is applied to minimize truncation artifacts
5. The FIR is normalized so a flat spectrum produces exactly 0 dB passthrough
6. The FIR filter is applied via JUCE's efficient FFT-based partitioned `Convolution` engine
7. Auto makeup gain is computed from the actual FIR frequency response (forward FFT of the final filter)

This ensures **mathematically perfect phase linearity** — the only thing that changes is the frequency balance. The original waveform shape, transient character, and stereo image are completely preserved.

## Building

### Requirements
- CMake 3.22+
- JUCE framework (cloned to `../JUCE` relative to project)

#### Windows
- Visual Studio 2022 Build Tools (C++ workload)

#### macOS
- Xcode 14+

#### Linux (Ubuntu 22.04+)
```bash
sudo apt-get install build-essential cmake git libasound2-dev \
  libfreetype6-dev libx11-dev libxrandr-dev libxcursor-dev \
  libxinerama-dev libwebkit2gtk-4.1-dev libcurl4-openssl-dev
```

### Build Steps

```bash
git clone https://github.com/juce-framework/JUCE.git ../JUCE
cmake -B build -G "Visual Studio 17 2022" -A x64    # Windows
cmake -B build -G Xcode                              # macOS
cmake -B build -DCMAKE_BUILD_TYPE=Release             # Linux
cmake --build build --config Release
```

Output:
- VST3: `build/InstaLPEQ_artefacts/Release/VST3/InstaLPEQ.vst3`
- AU: `build/InstaLPEQ_artefacts/Release/AU/InstaLPEQ.component` (macOS)
- LV2: `build/InstaLPEQ_artefacts/Release/LV2/InstaLPEQ.lv2`

## Tech Stack

- **Language:** C++17
- **Framework:** JUCE 8
- **Build:** CMake + MSVC / Xcode / GCC
- **Audio DSP:** juce::dsp (FFT, Convolution, IIR coefficient design, Limiter)
- **Font:** Rajdhani (SIL Open Font License)
