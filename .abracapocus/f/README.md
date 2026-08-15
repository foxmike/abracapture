# f

## Goal

Build the primary polished Qt Quick capture experience: source cards with live identification data, waveform and meters, capture controls/integrity status, output destination controls, and settings. Materialize exactly four narrow tasks, one per hint.

## Notes

- Async: Wave 1: f.1, f.2, and f.4 run in parallel with disjoint files. Wave 2: f.3 depends on f.1, f.2, and f.4 and is the only task in this phase allowed to modify qml/Main.qml, src/app/main.cpp, and CMakeLists.txt.
- Build/bootstrap contract: Phase A registers only its initial files and only `appController`. In f.3, extend the actual Phase-A CMake QML mechanism and QQmlApplicationEngine composition for all completed Phase F sources and QML files; do not substitute globbing or invent a different registration scheme.
- Risk: PipeWireCaptureStream produces preallocated, bounded `CaptureVisualizationSnapshot` value copies; CaptureSession exposes them to non-realtime coordination. WaveformBridge consumes those queued copies only. CaptureMetricsSnapshot supplies peak/RMS/integrity data, not waveform samples, and GUI code must never read or drain AudioRingBuffer. The GUI must stay responsive during capture/export and must clearly distinguish application streams, system mix/monitor sources, and physical inputs. Avoid visual complexity that turns the utility into a DAW.
