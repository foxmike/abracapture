# Abracapture

Abracapture is a Linux audio-capture utility for discovering PipeWire application streams, output monitors, and hardware inputs, then capturing a selected source. Its Qt desktop interface provides source metadata, waveform and level visualization, capture controls, and format-integrity reporting; its CLI records WAV or FLAC masters and supports automation and post-capture exports. PipeWire decides the final negotiated stream format; Abracapture reports when that differs from the requested or stored format.

## Features

- Discover capturable PipeWire sources and classify them as application, monitor, or hardware input.
- Select a source by name or node ID and view its advertised sample rate and channel count.
- Monitor waveform and level data while a capture is connected.
- Start and stop captures manually, or use threshold-based auto-start/auto-stop with pre-roll and post-roll.
- Write WAV or lossless integer PCM FLAC masters from the CLI.
- Report native, quantized, resampled, or converted capture provenance.
- Create optional derived WAV, FLAC, MP3, or Ogg/Vorbis files through ffmpeg after the master is finalized.
- Configure the output directory and filename template; review available recording metadata through the recordings view when a history index is populated.
- Use `list-sources` and `record` for shell or headless automation.

Capture is not unconditionally bit-perfect. A PipeWire format negotiation can change the stream format, and writing a selected master can quantize or convert samples. Upsampling does not create higher-resolution source material; derived files retain the provenance of the source and conversion path.

## Build requirements

- A C++20 compiler
- CMake 3.22 or newer
- Qt 6.11 with Core, Quick, and Quick Controls 2
- PipeWire 0.3 (`libpipewire-0.3`)
- libsndfile
- libFLAC
- ffmpeg for optional post-capture exports (not required to build core capture)

## Build and install

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build --prefix "$HOME/.local"
```

Ensure the installed executable is on `PATH` and that a PipeWire session is running before capture.

## Usage

Run `abracapture` to open the desktop application. Choose a discovered source, inspect its advertised format, and use Capture to start or stop a PipeWire capture session. For file masters and exports, use the `record` CLI workflow described below. See [docs/USER_GUIDE.md](docs/USER_GUIDE.md) for the detailed workflow and integrity rules.

For automation:

```sh
abracapture list-sources
abracapture record --source "Application name" --format wav --output ./captures --duration 30
abracapture record --source 42 --format flac --output ./captures --auto-start --auto-stop
```

Run `abracapture record --help` for the complete record-option list. The `--export` option accepts comma-separated ffmpeg formats such as `mp3`, `flac`, `ogg`, or `wav`.

## License

License information is provided with the distribution. The desktop packaging metadata currently identifies the project license as proprietary (`LicenseRef-Proprietary`).
