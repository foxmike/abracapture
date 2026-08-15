# g

## Goal

Add the polished product layer around capture: recording history, details/actions, desktop notifications, theme/accessibility, and About/product identity. Materialize exactly four narrow tasks, one per hint.

## Notes

- Async: Wave 1: g.1, g.3, and g.4 may run in parallel; their write paths are disjoint. Wave 2: g.2 depends on g.1 and consumes the recording index. g.4 is the only task in this phase allowed to modify qml/Main.qml; g.3 is the only task allowed to modify src/ui/CaptureViewModel.cpp.
- Risk: History should index Abracapture outputs and sidecar metadata without becoming a media library/database project. Notifications must not block capture. Theme work must preserve keyboard operation, readable scaling, and clear capture-state indicators.
