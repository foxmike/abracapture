# h

## Goal

Harden Abracapture for public distribution with focused deterministic tests, real PipeWire recovery/stress coverage, Linux desktop packaging metadata/install rules, and concise user/build documentation. Materialize exactly four narrow tasks, one per hint.

## Notes

- Async: Wave 1: h.1, h.2, h.3, and h.4 may run in parallel only if materialization keeps their write paths exactly disjoint as listed. h.3 alone may modify CMakeLists.txt. Final phase verification performs the release build, focused CTest suite, and integration checks after all four tasks finish.
- Risk: Live PipeWire tests must be bounded and clean up every temporary stream/link they create. Packaging must not bundle incompatible third-party licenses or silently vendor Qt/PipeWire. Documentation must state native capture versus conversion behavior accurately and must not promise bit-perfect capture when PipeWire negotiation or resampling says otherwise.
