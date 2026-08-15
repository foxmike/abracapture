# e

## Goal

Add intelligent recording automation, capture-integrity/provenance reporting, optional verified post-capture exports, and first-class CLI operation. Lossy encoding and resampling occur only after a valid lossless master exists. Materialize exactly four narrow tasks, one per hint.

## Notes

- Async: Wave 1: e.1, e.2, and e.3 run in parallel with disjoint files. Wave 2: e.4 depends on e.1, e.2, and e.3 and owns the phase's only src/app/main.cpp integration write.
- Risk: Auto-start/stop must be deterministic under threshold chatter and must preserve configurable pre-roll/post-roll. ffmpeg is an optional post-capture executable dependency, never part of the realtime callback. Export failures must not delete or invalidate the master. The CLI must expose native-vs-converted status.
