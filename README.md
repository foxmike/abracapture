# Abracapture

**Abracapture** is a standalone Linux audio-capture utility built for advanced PipeWire systems.

Its goal is simple:

> Find any audio stream on a Linux machine, identify it visually, capture it faithfully, and export it in the formats you actually need.

Abracapture is designed to replace workflows that currently require combinations of qpwgraph, REAPER, Audacity, command-line PipeWire utilities, and manual routing.

The application will automatically discover available audio sources, let the user identify the correct stream through live waveform and level visualization, connect to that source programmatically, and record it through a dedicated realtime-safe capture engine.

Abracapture is being developed with **Abracapocus**.

---

## Goals

Abracapture is intended to be useful for musicians, audio engineers, developers, archivists, stream capture, testing, content production, and anyone who needs more control over Linux audio capture than typical desktop recording utilities provide.

The application should:

- automatically discover PipeWire audio streams
- show application playback streams
- show system/output monitor streams
- show hardware capture inputs
- distinguish software, system, and hardware sources
- provide live waveform and level visualization
- connect to sources without requiring qpwgraph
- capture at the source's native format whenever possible
- clearly report when resampling or bit-depth conversion occurs
- create lossless master recordings
- optionally create additional lossless or lossy exports afterward
- support automatic start and stop
- support pre-roll and post-roll
- automatically reconnect when application streams are recreated
- provide configurable output directories and filenames
- maintain lightweight recording history
- support both GUI and CLI operation
- be polished enough to distribute as a real Linux desktop application

Abracapture is **not** intended to become a DAW.

There is no editing timeline, plugin host, mixer, mastering environment, or VST architecture in the scope of the application.

---

# Capture Philosophy

The primary design rule is:

> **Capture faithfully first. Convert afterward.**

Abracapture should preserve the actual PipeWire source format whenever possible.

For example:

```text
SOURCE
32-bit float
48,000 Hz
Stereo

MASTER
32-bit float WAV
48,000 Hz

Native capture
No resampling
No quantization
```

If conversion is requested:

```text
SOURCE
32-bit float
48,000 Hz

EXPORT
24-bit FLAC
96,000 Hz

Conversion:
48 kHz -> 96 kHz resampling
32-bit float -> 24-bit integer
```

Abracapture must never imply that upsampling creates source fidelity that was not present in the original audio.

---

# Supported Sources

Abracapture will discover sources directly from the PipeWire graph.

Expected source categories include:

### Application streams

Examples:

- Firefox
- Chromium
- VLC
- music players
- games
- media applications
- DAWs
- standalone audio applications

### System output

The complete system playback stream can be captured as a simple user-facing source such as:

```text
Entire System Output
```

Users should not need to understand PipeWire monitor nodes to perform this capture.

### Hardware inputs

Physical devices exposed through PipeWire can also be captured.

Examples:

- USB audio interfaces
- microphones
- line inputs
- guitar interfaces
- mixers
- digital audio devices

High-resolution hardware such as a 24-bit / 192 kHz interface can therefore be used directly where the hardware and PipeWire configuration support it.

---

# Source Identification

One of Abracapture's key features is visual source identification.

Each active source should expose information such as:

```text
Firefox - YouTube

48.0 kHz | Stereo | Active

▁▂▃▆█▇▄▃▂▁▂▄▆██▅▃▂

L  ███████████████░
R  ██████████████░░
```

The source browser will provide:

- application/device name
- source type
- sample rate
- sample format
- channel count
- active/silent state
- peak meters
- waveform preview
- favorites/pinned sources
- automatic source appearance/disappearance
- automatic reconnection where possible

Waveform rendering will consume bounded snapshots from the audio engine and will never access realtime buffers directly from the GUI.

---

# Recording Engine

The capture engine will use the native PipeWire API.

The basic pipeline is:

```text
PipeWire graph
      |
      v
Source discovery
      |
      v
PipeWire capture stream
      |
      v
Realtime-safe bounded buffer
      |
      v
Recording worker
      |
      v
Lossless master
      |
      v
Optional export/conversion
```

The PipeWire realtime callback must remain extremely small.

The realtime path must not perform:

- file I/O
- FLAC compression
- FFmpeg execution
- GUI operations
- blocking synchronization
- dynamically growing containers
- expensive memory allocation
- conversion work that can safely occur elsewhere

Capture errors such as buffer loss, source loss, and format renegotiation must be observable rather than silently corrupting recordings.

---

# Lossless Master Formats

Abracapture will initially support lossless recording formats.

## WAV

Planned support includes:

- 16-bit PCM
- 24-bit PCM
- 32-bit PCM
- 32-bit floating point
- common sample rates through 192 kHz where supported
- safe handling of large recordings

32-bit floating-point WAV is particularly useful when capturing floating-point audio directly from the PipeWire graph.

## FLAC

FLAC will be available for integer PCM lossless storage.

Expected support includes:

- 16-bit
- 24-bit
- 32-bit integer where supported by the selected FLAC implementation
- native sample rates through high-resolution formats supported by the source

Abracapture will not pretend that FLAC and floating-point WAV represent identical storage models.

---

# Optional Exports

Lossy formats will be created only **after a valid lossless master has been finalized**.

Optional derived formats may include:

- FLAC
- Opus
- MP3
- AAC
- Ogg Vorbis

FFmpeg will be used for appropriate post-capture conversion rather than adding lossy codec work to the realtime recording path.

Example:

```text
Master
[x] WAV 32-bit float

Additional exports
[x] FLAC
[x] Opus 256 kbps
[ ] MP3
[ ] AAC

After conversion
(*) Keep master
( ) Delete master after verified exports
```

The default behavior should preserve the master.

An export failure must never invalidate or delete a successfully completed master recording.

---

# Intelligent Recording

Abracapture will support automatic recording behavior.

Planned controls include:

- manual Record / Stop
- auto-start when audio exceeds a threshold
- auto-stop after configurable silence
- configurable trigger threshold
- configurable silence duration
- pre-roll
- post-roll
- maximum recording duration
- continuous recording
- file splitting
- silence-based splitting
- source-loss policy
- delayed or scheduled recording

## Pre-roll

Abracapture can continuously maintain a short ring buffer in memory.

If pre-roll is configured for 10 seconds, pressing Record after audio begins can still preserve the previous 10 seconds.

This is useful when the user hears something worth capturing slightly before deciding to record it.

---

# Capture Integrity

Abracapture should always make the actual capture path visible.

Example:

```text
CAPTURE INTEGRITY

Source             Firefox
Source rate        48,000 Hz
Source format      32-bit float
Channels           2

Captured           48,000 Hz / F32
Resampling         NONE
Quantization       NONE
Dropped buffers    0
Peak               -1.7 dBFS

STATUS             NATIVE / LOSSLESS
```

Capture metadata should include:

- source application/device
- PipeWire source identity
- original format
- original sample rate
- channel layout
- recording format
- recording rate
- conversion state
- resampling state
- start/end time
- dropped buffers
- peak level
- source-loss events
- finalization status

---

# GUI

Abracapture will use:

- **C++20**
- **Qt 6**
- **Qt Quick / QML**
- **Qt Quick Controls**

The GUI should remain compact and purpose-built rather than resembling a DAW.

Primary sections:

```text
Sources
Capture
Recordings
Settings
```

Example capture view:

```text
┌ AUDIO SOURCE ───────────────────────────┐
│ Firefox - YouTube                 LIVE │
│ ▂▄▇██▅▃▂▃▆████▆▄▂                     │
│ 48 kHz | F32 | Stereo                  │
└────────────────────────────────────────┘

     L █████████████████░
     R ████████████████░░

 Native Capture ✓

 Pre-roll:   10 sec
 Auto Start: -42 dB
 Auto Stop:  5 sec silence

 ~/Music/Captures/

              ● RECORD
```

Settings will include:

- capture directory
- filename template
- overwrite behavior
- default master format
- additional export formats
- resampling policy
- bit-depth policy
- auto-start threshold
- auto-stop duration
- pre-roll
- post-roll
- channel mapping
- default/favorite source
- monitoring options
- notifications
- metadata behavior
- master retention policy
- appearance preferences

---

# Command-Line Interface

Abracapture will also expose a first-class CLI using the same capture engine as the GUI.

Expected usage:

```text
abracapture list
```

```text
abracapture record \
  --source firefox \
  --native \
  --format wav-f32 \
  --output ~/Music/Captures/song.wav
```

```text
abracapture record \
  --source system \
  --format flac
```

```text
abracapture record \
  --source presonus \
  --rate 192000 \
  --bits 24 \
  --format flac
```

The CLI makes Abracapture suitable for:

- shell automation
- testing
- remote systems
- scheduled capture
- headless machines
- integration with other Linux tools

---

# Recording History

Abracapture will maintain a lightweight history of recordings it creates.

The history should expose:

- filename
- capture time
- duration
- source
- source format
- master format
- conversion status
- output location
- capture integrity
- export status

History is intended as a convenience layer, not as a full media library.

---

# Reliability

Abracapture is intended to survive real Linux desktop audio behavior.

Release testing will cover cases such as:

- source exists before Abracapture starts
- source appears after Abracapture starts
- application destroys and recreates its PipeWire stream
- source disappears during capture
- hardware device is disconnected
- hardware device is reconnected
- source sample rate changes
- source format changes
- stereo capture
- multichannel capture
- high-resolution capture
- long recordings
- recordings larger than 4 GiB
- destination permissions
- disk-full conditions
- interrupted shutdown
- suspended system
- export failure
- PipeWire reconnection
- hardware capture through external audio interfaces

File finalization must be failure-safe so interrupted captures do not silently appear to be valid completed recordings.

---

# Architecture

Primary technologies:

```text
C++20
CMake
Qt 6 / Qt Quick / QML
PipeWire
libsndfile
libFLAC
FFmpeg
CTest
```

Responsibilities are intentionally separated.

```text
src/
├── app/
├── audio/
├── automation/
├── cli/
├── config/
├── core/
├── export/
├── history/
├── pipewire/
├── recording/
└── ui/

qml/
├── components/
├── pages/
└── theme/

tests/
├── core/
└── integration/

packaging/
```

The project will not vendor PipeWire, Qt, or FFmpeg unless there is a compelling distribution requirement.

---

# Development Plan

Abracapture is currently planned as eight implementation phases.

## Phase A — Foundation

- CMake and dependency baseline
- Qt Quick application bootstrap
- persistent settings
- audio-format and conversion domain types

## Phase B — PipeWire Discovery

- PipeWire registry and object lifetime management
- source descriptors and source classification
- application/device metadata normalization
- live source catalog and activity model

## Phase C — Capture Core

- bounded audio ring buffer
- negotiated PipeWire capture stream
- realtime-safe metrics snapshots
- capture lifecycle and automatic reconnection

## Phase D — Lossless Recording

- WAV master writer
- FLAC master writer
- safe recording finalization
- output-directory and filename policies

## Phase E — Automation and Export

- auto-start / auto-stop
- pre-roll / post-roll
- capture-integrity reporting
- post-capture export manager
- CLI workflows

## Phase F — Primary GUI

- live source browser
- source cards
- waveform visualization
- meters
- capture page
- application shell
- settings interface

## Phase G — Product Layer

- recording history
- recording browser
- recording details
- desktop notifications
- theme
- accessibility
- About/product experience

## Phase H — Release Hardening

- deterministic core tests
- PipeWire integration tests
- capture recovery tests
- desktop packaging
- install rules
- public documentation

---

# Abracapocus Development Strategy

The implementation plan contains **32 narrowly scoped tasks across 8 phases**.

Tasks are deliberately small so different coding models can be used according to difficulty and cost.

Current routing:

```text
8 hardest tasks
Codex
gpt-5.6-sol
reasoning policy: xhigh

8 next-hardest tasks
Codex
gpt-5.6-terra
reasoning policy: high

8 medium/easier tasks
Antigravity
gemini-3.7-flash-medium

8 narrowest tasks
Codex
gpt-5.6-luna
reasoning policy: medium
```

The selected Codex models and reasoning levels were empirically validated before project execution.

The selected Antigravity model was also empirically validated with `agy 1.1.13`.

For current Antigravity Gemini models, reasoning effort is encoded directly in the model identifier:

```text
gemini-3.7-flash-low
gemini-3.7-flash-medium
gemini-3.7-flash-high
```

---

# Project Principles

Abracapture should remain:

- native
- fast
- realtime-safe
- transparent
- deterministic
- automation-friendly
- useful without requiring audio-routing expertise
- powerful enough for advanced Linux audio workflows
- visually polished
- narrow in purpose

The core product proposition remains:

> **Find it. See it. Capture it faithfully.**
