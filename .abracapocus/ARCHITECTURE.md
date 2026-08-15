# Architecture Contract: ABRACAPTURE

## Product Boundary

Abracapture is a standalone Linux audio-capture utility. It is NOT a DAW, editor timeline, plugin host, or VST application. Scope ends at: discover sources, capture audio, write lossless masters, optional post-capture exports, and present a polished Qt Quick GUI and CLI.

## Layer Topology

```
Qt Quick GUI / CLI
        |
  CaptureViewModel / SourceListModel / WaveformBridge / SettingsViewModel
        |
  CaptureSession (session lifecycle, state machine, reconnection)
        |
  +-----------+-----------+
  |           |           |
AudioRingBuffer  PipeWireCaptureStream  CaptureMetrics
  |           |           |
  +-----------+-----------+
        |
  PipeWireContext / SourceCatalog / SourceDescriptor / PropertyNormalizer
        |
  PipeWire 0.3 daemon (external)
```

Recording path (non-realtime, after samples leave ring buffer):

```
AudioRingBuffer (read) -> RecordingSink -> WavWriter / FlacWriter -> lossless master
                                              |
                                         OutputPathPolicy
                                              |
                                  optional: ExportManager (ffmpeg subprocess)
```

Ancillary:

```
AutoRecordController (level-triggered automation, non-realtime)
CaptureIntegrity (provenance reporting)
RecordingIndex (history, JSON, non-realtime)
NotificationService (freedesktop/tray, non-realtime)
AppSettings (QSettings persistence)
```

## Key Contracts

**Realtime boundary**: The PipeWire process callback (on_process) must not allocate heap, lock mutexes, write to disk, spawn processes, compress audio, or call into Qt. AudioRingBuffer write(), CaptureMetrics publish(), and publication to the preallocated visualization snapshot buffer are the only operations permitted there.

**Visualization snapshot boundary**: PipeWireCaptureStream owns a preallocated, bounded `CaptureVisualizationSnapshot` publisher fed by the process callback. CaptureSession exposes value-copy reads of that snapshot to a non-realtime coordination path. WaveformBridge consumes only queued value copies from that path; it must never read or drain AudioRingBuffer. CaptureMetricsSnapshot carries peak/RMS/integrity metrics only and is not a waveform-sample source.

**PipeWire context ownership**: Phase B owns the pw_thread_loop, pw_context, and pw_core lifetime through PipeWireContext. PipeWireCaptureStream (Phase C) receives the Phase-B context; it does not create a second thread loop or context.

**Source identity / re-resolution**: PipeWire node IDs are not stable across disappear/reappear cycles. Source lookup must use the stable identity established by SourceCatalog (display name, media class, device descriptor) to re-resolve a reappeared stream. CaptureSession reconnection must not assume the original node ID is still valid.

**Lossless-master-first rule**: A successfully finalized lossless master (WAV or FLAC via RecordingSink) is required before any post-capture export via ExportManager may be treated as complete. Export failure must never delete or truncate the master.

**Conversion provenance**: CaptureIntegrity and CaptureResult must accurately report whether capture was Native, Quantized, Resampled, or Converted. Upsampled audio must not be presented as higher-resolution source material.

**No realtime I/O**: File writes, ffmpeg invocations, Qt signal emissions, and blocking synchronization are non-realtime operations. They belong in RecordingSink, ExportManager, and the session coordination layer, not in the process callback.

## Technology Stack

- Language: C++20
- Build: CMake 3.22+, CMAKE_CXX_STANDARD 20
- GUI: Qt 6.11 Qt Quick / Qt Quick Controls 2
- Audio: PipeWire 0.3 APIs (pw_thread_loop, pw_stream, pw_registry)
- WAV: libsndfile (RF64 for >4 GiB)
- FLAC: libFLAC C API (FLAC__stream_encoder_*)
- Export: ffmpeg executable (optional post-capture, never in realtime callback)
- No JUCE, no second GUI framework, no vendored third-party source
