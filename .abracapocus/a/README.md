# a

## Goal

Establish the production C++20/Qt Quick application foundation, dependency contract, persistent settings schema, and shared audio-domain types. Materialize exactly four narrow tasks, one per hint.

## Notes

- Async: Wave 1: a.1, a.3, and a.4 may run in parallel because their write paths are disjoint. Wave 2: a.2 depends on a.1 so the application bootstrap follows the declared Qt/CMake dependency contract. Phase verification configures and builds only after all four tasks finish.
- Risk: Qt 6.11 Qt Quick is the GUI baseline. Keep dependencies explicit and dynamically linked where appropriate; do not introduce JUCE, a second GUI framework, a package manager, or vendored copies of PipeWire/Qt/FFmpeg. The CMake task must not silently download third-party dependencies.
