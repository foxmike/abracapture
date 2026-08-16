# Abracapture User Guide

Abracapture captures audio sources exposed by PipeWire. It can record WAV or FLAC masters and can ask ffmpeg to create derived files after a master is complete. The exact negotiated PipeWire format is authoritative: a capture is not promised to be bit-perfect when PipeWire changes the requested format or when the selected output requires conversion.

## Getting started

### First launch

1. Start a PipeWire session and launch `abracapture`.
2. Open the Capture page. Abracapture connects to PipeWire and discovers capturable nodes.
3. If the list is empty, confirm that PipeWire is running and use Refresh. The source list is live, so applications and devices appearing later can be discovered.
4. Select a source to see its category, node ID, advertised sample rate, and channel count.
5. Choose Start Capture. The interface shows connection/capture state, waveform, meters, duration, output destination, and capture-integrity status.

The current GUI composition owns the PipeWire capture session and visualization; file-master writing and ffmpeg export are provided by the `record` CLI workflow. Use the CLI when you need a finalized WAV or FLAC file.

The GUI starts in capture mode when no CLI subcommand is supplied. `abracapture list-sources` and `abracapture record ...` run without the GUI.

## Selecting capture sources

The source browser includes the categories exposed by the PipeWire graph:

- **Application streams** — playback streams from applications such as browsers, players, games, or other audio programs.
- **Monitor streams** — output-monitor sources representing playback from a system or device output.
- **Hardware inputs** — microphones, interfaces, line inputs, and other physical capture devices.

Select the specific node you want. A source name, application name, node name, and numeric node ID can be used with the CLI. Names are matched case-insensitively, first exactly and then by substring; a numeric value is treated as a node ID.

## Capture controls

### Start and stop

Use Start Capture and Stop Capture in the GUI. A selected source must be available before a capture can begin. If the source disappears, the capture session can enter reconnecting state and attempts to resolve the same source again; the final result should be checked for interruptions and integrity status.

### Auto-start and auto-stop

Enable these controls in Settings when recording should follow the signal level instead of manual buttons. Auto-start uses a dBFS threshold; auto-stop watches for signal below its stop threshold and then enters post-roll. The GUI settings persist the auto-start threshold, auto-stop silence duration, and enablement state.

### Pre-roll and post-roll

Pre-roll keeps a bounded recent audio buffer so audio immediately before an automatic start can be included. Post-roll keeps recording after the signal falls below the auto-stop threshold. Set each duration in seconds in Settings; the accepted range is 0–300 seconds. A zero value disables that part of the buffer.

The CLI accepts the same concepts in milliseconds:

```sh
abracapture record --source 42 --auto-start --auto-stop \
  --pre-roll 5000 --post-roll 2000
```

## Output formats

### WAV and FLAC masters

The master format can be `wav` or `flac` in the GUI and CLI. WAV stores PCM or floating-point samples supported by the writer. FLAC stores integer PCM; the CLI FLAC path records a 16-bit integer master and therefore may quantize a floating-point PipeWire stream. FLAC compression itself is lossless after the integer samples have been selected.

The master is finalized through a temporary file and then moved into place. A completed master is the source for any later export.

### Native format preservation and conversion disclosure

Abracapture requests a format, sample rate, and channel count from PipeWire based on the selected source. PipeWire may negotiate another supported format. The capture engine records the negotiated format and compares it with the requested path and output format.

The UI uses these labels:

- **Native signal path** — negotiated format, rate, and channels match the requested capture values.
- **Quantized capture** — the sample rate matches but the stored sample representation has reduced or changed precision, such as float to integer.
- **Resampled capture** — the sample rate differs.
- **Converted capture** — another format difference is present.

These labels describe the actual path; “native” is not an unconditional promise of bit-perfect audio. A negotiated PipeWire conversion, a master-format conversion, resampling, buffer loss, or a lossy export can change the resulting file. Upsampling only changes the file’s sample rate. It is not presented as higher-resolution source material.

## Post-capture exports

Exports run after the master is finalized and verified. ffmpeg is launched outside the realtime capture path. Supported export formats in the current implementation include:

- Lossless: FLAC and WAV.
- Lossy: MP3 and Ogg/Vorbis (`ogg` or `vorbis`).

The GUI exposes one configured export format. The CLI performs exports and accepts a comma-separated list:

```sh
abracapture record --source 42 --format wav --output ./captures \
  --export flac,mp3,ogg
```

Exports are derived files, not replacements for the master. They may resample or change bit depth according to the ffmpeg export settings. Install `ffmpeg` and ensure it is available on `PATH` to use this feature. An export failure does not make a successfully finalized master invalid, but the command reports a non-zero export result.

## Output directory and naming

The default GUI output directory is the user Music location under `Abracapture`. Change it on the Settings page. The default filename pattern is:

```text
{source}_{date}_{time}
```

Supported filename placeholders include `{source}`, `{date}`, `{time}`, `{datetime}`, `{format}`, `{samplerate}`, `{channels}`, and `{counter}`. `{sample_rate}`, `{sampleRate}`, `{channel_count}`, `{channelCount}`, and `{sequence}` are accepted aliases. If no extension is supplied, the selected format is appended. Unsafe characters from source names are replaced, and an existing path receives a numbered suffix rather than being overwritten.

The CLI `--output` value may be a destination directory or a file path. Without `--output`, the CLI writes to the current directory using `{source}_{date}_{time}`. A directory uses the selected master format; a file path uses its parent and stem.

## Capture history

The project includes a JSON-backed recording index and a Recordings view for entries containing file path, format, sample rate, bit depth, channels, duration, timestamp, native/converted status, and optional sidecar path. Entries are sorted newest first; the view can inspect an entry, reveal its containing directory, refresh, and remove an index entry.

The current application entry point does not automatically append each newly finalized GUI capture to that index. If the Recordings view is empty, it means no entries have been supplied to the history model; it does not mean that files in the output directory were scanned automatically.

## Settings

Settings are persisted with Qt settings for the Abracapture application. The GUI provides:

- output directory and filename pattern;
- WAV or FLAC master selection;
- optional export enablement and MP3, FLAC, OGG, or WAV export format;
- auto-start threshold and auto-stop silence duration;
- auto-start/auto-stop enablement;
- pre-roll and post-roll durations.

Invalid paths, empty naming patterns, unsupported formats, and out-of-range automation values are rejected with a validation message. Use Reset to Defaults when needed.

## CLI automation

The CLI is selected by a subcommand:

```sh
abracapture list-sources
```

`list-sources` prints discovered sources as indented JSON, including `name`, numeric `id`, `media_class`, and `activity` fields.

Record a source with:

```sh
abracapture record \
  --source "Firefox" \
  --format wav \
  --output ./captures \
  --duration 60
```

Available `record` options are:

- `--source` / `-s` — required source name, partial name, or node ID.
- `--output` / `-o` — file path or destination directory.
- `--format` / `-f` — `wav` or `flac`; defaults to `wav`.
- `--duration` / `-d` — positive duration in seconds.
- `--auto-start` — wait for the signal to cross the auto-start threshold.
- `--auto-stop` — stop after the signal remains below the stop threshold for the configured post-roll behavior.
- `--pre-roll` — pre-roll duration in milliseconds.
- `--post-roll` — post-roll duration in milliseconds.
- `--export` / `-e` — comma-separated derived formats such as `mp3,flac,ogg`.
- `--help` — show record-command help.

The command prints a capture-integrity summary after finalization. Exit status `0` indicates capture and requested exports completed; source lookup, PipeWire, master-finalization, and export failures return non-zero statuses. Stop a running capture with `Ctrl-C` or `SIGTERM` so the writer can finalize the master.

## Capture Integrity

Capture integrity is a provenance report, not a guarantee that every source reaches disk unchanged.

### Native capture

A PipeWire stream is native when its negotiated sample format, sample rate, and channel count match the requested capture values; this is the meaning of the capture stream’s `isNativeCapture` status. A completed capture report separately compares negotiated and file sample format/rate to label the stored result. For example, a requested 48 kHz, two-channel Float32 stream written as a matching WAV master can be native if PipeWire accepts that exact negotiation.

### Quantized, resampled, and converted capture

A capture is classified as quantized when the rate matches but the stored sample representation changes precision. It is resampled when the negotiated and stored rates differ. It is converted when other format fields differ. The FLAC writer accepts integer PCM only, so a float-to-integer master is a conversion/quantization case even though FLAC compression is lossless for the resulting integers.

PipeWire can negotiate a different rate, sample format, or channel count than the request. The capture status can also report format renegotiation, reconnecting, errors, and overflow metrics. Check the displayed label and negotiated/file metadata for every capture that matters. Neither a native label nor a higher output sample rate proves that the source contained more information: upsampled audio is not presented as higher-resolution source material.

### What to retain

Keep the finalized WAV or FLAC master as the reference recording. Treat ffmpeg outputs as derived deliverables, and retain the integrity label and metadata when comparing or archiving captures.
