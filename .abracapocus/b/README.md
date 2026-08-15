# b

## Goal

Implement live PipeWire graph discovery and source classification so Abracapture automatically presents application playback streams, system/output monitor streams, hardware capture inputs, and other relevant audio nodes with stable metadata. Materialize exactly four narrow tasks, one per hint.

## Notes

- Async: Wave 1: b.1, b.2, and b.3 run in parallel with disjoint files. Wave 2: b.4 depends on b.1, b.2, and b.3 and is the only source-catalog fan-in task.
- Risk: PipeWire registry and proxy lifetimes, node disappearance, metadata churn, and sink-monitor semantics are the main risks. Discovery must be event driven; do not shell out to pw-dump/wpctl as the core implementation.
