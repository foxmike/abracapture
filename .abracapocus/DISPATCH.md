# Dispatch: ABRACAPTURE

## Phase Sequencing

Phases a through h execute sequentially in skeleton order. No cross-phase task dependencies exist or are permitted. Phase N must complete (all tasks pass verification, phase_verification.json checks pass) before Phase N+1 begins.

## Within-Phase Concurrency

Within each phase, task execution follows the explicit waves defined in that phase's README.md and in PLAN_SKELETON.json async_notes:

- Tasks in the same wave have disjoint allowed_write_paths and may run in parallel.
- A task with dependencies must wait for all named same-phase prerequisites to complete before starting.
- Fan-in tasks (tasks with 2+ dependencies) wait for all named tasks.
- Files with a single owner per phase (e.g., qml/Main.qml, src/app/main.cpp) are serialized through the owning task's wave assignment.

## Routing

Task routing (selected_backend + model) in each task JSON is authoritative. Do not flatten mixed routing to a single backend.

- codex_cli + gpt-5.6-sol: 8 tasks (hardest: PipeWireContext, PipeWireCaptureStream, CaptureSession, FlacWriter, RecordingSink, WaveformBridge, PipeWire integration tests)
- codex_cli + gpt-5.6-terra: 8 tasks (next-hardest: Qt bootstrap, SourceDescriptor, SourceCatalog, WavWriter, CaptureIntegrity, ExportManager, CaptureViewModel, packaging)
- codex_cli + gpt-5.6-luna: 8 tasks (focused: CMake baseline, AudioFormat, PropertyNormalizer, AudioRingBuffer, CaptureMetrics, OutputPathPolicy, theme/About, tests + docs)
- antigravity_cli + gemini-3.7-flash-medium: 8 tasks (AppSettings, OutputPathPolicy, SourceBrowser, SettingsViewModel, CLI, RecordingIndex, RecordingsViewModel, NotificationService)

## Model Policy

- Codex effort levels (Sol=xhigh, Terra=high, Luna=medium) are the intended policy once abracapocus codex_cli routing has an explicit supported pass-through for model_reasoning_effort. Until then, the validated model pins are authoritative and effort must not be silently assumed.
- For Antigravity/gemini-3.7-flash-medium, effort is encoded in the model slug. Do not pass a separate --effort argument for this model slug with agy 1.1.13.
- Do not add a reasoning_effort field to task JSON; the task schema does not currently support it.
- Do not route Abracapture tasks to Claude/Opus through Antigravity.

## Overlapping Writes

Tasks with overlapping allowed_write_paths must not run concurrently. The wave structure in each phase's README.md and PLAN_SKELETON.json ensures this. If a dispatcher detects an overlap outside the defined wave structure, serialize the conflicting tasks.

## Acceptance Gate

A task is accepted when its verification_commands all pass and no file outside its allowed_write_paths has been created or modified. The reconciler and change assessor are advisory; their output is metadata only and must not cause rejection. See GLOBAL_EXECUTION_RULES.md.
