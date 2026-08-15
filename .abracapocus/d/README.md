# d

## Goal

Implement durable lossless master recording with WAV and FLAC writers, safe file finalization, and configurable destination/naming policy. The master path must preserve negotiated source rate/format when possible and record conversion provenance. Materialize exactly four narrow tasks, one per hint.

## Notes

- Async: Wave 1: d.1, d.2, and d.4 run in parallel with disjoint files. Wave 2: d.3 depends on d.1, d.2, and d.4 and is the only writer-orchestration/finalization fan-in task.
- Risk: Incomplete headers, truncated files, >4 GiB WAV limits, disk-full behavior, and FLAC integer-depth handling must fail safely. A successfully finalized lossless master is required before any later export may be treated as complete.
