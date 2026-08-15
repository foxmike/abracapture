# c

## Goal

Build the realtime-safe audio capture core: bounded buffering, negotiated PipeWire capture streams, lightweight metrics snapshots, and session lifecycle/reconnection behavior. Materialize exactly four narrow tasks, one per hint.

## Notes

- Async: Wave 1: c.1 (AudioRingBuffer) and c.3 (CaptureMetrics) run in parallel; their write paths are disjoint and neither depends on the other. Wave 2: c.2 (PipeWireCaptureStream) depends on c.1 and consumes the completed AudioRingBuffer write interface. Wave 3: c.4 depends on c.1, c.2, and c.3 and owns lifecycle fan-in, source loss, and reconnection behavior.
- Risk: The PipeWire process callback is a realtime boundary. No disk writes, ffmpeg invocation, Qt GUI work, blocking locks, dynamic container growth, or compression is allowed there. Buffer overflow/underrun and source renegotiation must become observable events rather than silent corruption.
