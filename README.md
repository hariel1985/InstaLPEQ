# InstaLPEQ

Free, open-source linear phase EQ plugin built with JUCE. Available as VST3, AU and LV2.

![VST3](https://img.shields.io/badge/format-VST3-blue) ![AU](https://img.shields.io/badge/format-AU-blue) ![LV2](https://img.shields.io/badge/format-LV2-blue) ![C++](https://img.shields.io/badge/language-C%2B%2B17-orange) ![JUCE](https://img.shields.io/badge/framework-JUCE-green) ![License](https://img.shields.io/badge/license-GPL--3.0-lightgrey) ![Build](https://github.com/hariel1985/InstaLPEQ/actions/workflows/build.yml/badge.svg)

## Download

**[Latest Release: v1.0](https://github.com/hariel1985/InstaLPEQ/releases/tag/v1.0)**

### Windows
| File | Description |
|------|-------------|
| [InstaLPEQ-VST3-Win64.zip](https://github.com/hariel1985/InstaLPEQ/releases/download/v1.0/InstaLPEQ-VST3-Win64.zip) | VST3 plugin — copy to `C:\Program Files\Common Files\VST3\` |

### macOS (Universal Binary: Apple Silicon + Intel)
| File | Description |
|------|-------------|
| [InstaLPEQ-VST3-macOS.zip](https://github.com/hariel1985/InstaLPEQ/releases/download/v1.0/InstaLPEQ-VST3-macOS.zip) | VST3 plugin — copy to `~/Library/Audio/Plug-Ins/VST3/` |
| [InstaLPEQ-AU-macOS.zip](https://github.com/hariel1985/InstaLPEQ/releases/download/v1.0/InstaLPEQ-AU-macOS.zip) | Audio Unit — copy to `~/Library/Audio/Plug-Ins/Components/` |

### Linux (x64, built on Ubuntu 22.04)
| File | Description |
|------|-------------|
| [InstaLPEQ-VST3-Linux-x64.zip](https://github.com/hariel1985/InstaLPEQ/releases/download/v1.0/InstaLPEQ-VST3-Linux-x64.zip) | VST3 plugin — copy to `~/.vst3/` |
| [InstaLPEQ-LV2-Linux-x64.zip](https://github.com/hariel1985/InstaLPEQ/releases/download/v1.0/InstaLPEQ-LV2-Linux-x64.zip) | LV2 plugin — copy to `~/.lv2/` |

> **macOS note:** Builds are Universal Binary (Apple Silicon + Intel). Not code-signed — after copying the plugin, remove the quarantine flag in Terminal:
> ```bash
> xattr -cr ~/Library/Audio/Plug-Ins/VST3/InstaLPEQ.vst3
> xattr -cr ~/Library/Audio/Plug-Ins/Components/InstaLPEQ.component
> ```

## Features

### Linear Phase EQ
- True linear phase processing using symmetric FIR convolution
- Zero phase distortion at any gain setting
- 8192-tap FIR filter (configurable: 4096 / 8192 / 16384)
- DAW-compensated latency (~93ms at 44.1kHz default)
- Background thread FIR generation — glitch-free parameter changes

### Interactive EQ Curve Display
- Logarithmic frequency axis (20 Hz — 20 kHz)
- Linear gain axis (-24 dB to +24 dB)
- Click to add EQ nodes (up to 8 bands)
- Drag nodes to adjust frequency and gain
- Scroll wheel to adjust Q/bandwidth
- Right-click for band type selection and delete
- Double-click to reset band to 0 dB
- Real-time frequency response curve with glow effect
- Per-band curve overlay

### Band Types
- Peak (parametric)
- Low Shelf
- High Shelf

### Controls
- Per-band: Frequency, Gain, Q knobs
- Master gain (+/- 24 dB)
- Bypass toggle
- State save/restore (DAW session recall)

### GUI
- Dark modern UI matching InstaDrums visual style
- 3D metal knobs with glow effects (orange for EQ, blue for Q)
- Carbon fiber background texture
- Rajdhani custom font
- Fully resizable window with proportional scaling
- Animated toggle switches
- Color-coded EQ bands (8 distinct colors)

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

## How It Works

InstaLPEQ uses a **FIR-based linear phase** approach:

1. Each EQ band's target magnitude response is computed from IIR filter coefficients (Peak, Low Shelf, or High Shelf)
2. All band magnitudes are multiplied together to form the combined target response
3. An inverse FFT converts the magnitude-only spectrum into a symmetric time-domain impulse response
4. A Blackman-Harris window is applied to minimize truncation artifacts
5. The FIR filter is applied via JUCE's efficient FFT-based `Convolution` engine

This ensures **zero phase distortion** regardless of EQ settings — ideal for mastering, surgical corrections, and transparent tonal shaping.

## Tech Stack

- **Language:** C++17
- **Framework:** JUCE 8
- **Build:** CMake + MSVC / Xcode / GCC
- **Audio DSP:** juce::dsp (FFT, Convolution, IIR coefficient design)
- **Font:** Rajdhani (SIL Open Font License)
