# BP Authoring Failure Diagnostics Policy

This policy defines the Section 41 failure diagnostics and replay-safety layer
for Blueprint manifest execution.

## Diagnostic Schema

Executor failures use `section_41_failure_diagnostics_v2`. The previous
manifest-step fields are preserved as legacy context, but the diagnostic must
also include:

- `executor_version`
- `failure_category`
- `replay_safety`
- `phase`
- `section`
- `step_id`
- `command`
- `stage_tail`

## Failure Categories

- `policy_block`: executor policy refused live execution.
- `manifest_step_failure`: a manifest command or non-structural operation
  failed.
- `structural_validation_failure`: graph/node/pin/layout/dataflow validation
  failed after command execution.
- `compile_validation_failure`: compile validation reported failure.
- `cleanup_failure`: temporary asset cleanup failed or could not be confirmed.
- `bridge_connection_reset`: the Unreal bridge connection was reset.
- `bridge_connection_refused`: the Unreal bridge refused connection.
- `editor_shutdown`: the editor shut down or received an exit command during
  verification.

## Replay Safety

Every diagnostic must state that durable side effects are not allowed by the
current executor. Replay recommendations are category-specific:

- Policy blocks must not replay until the manifest policy is reinforced.
- Cleanup failures should retry cleanup only before replaying authoring.
- Bridge/editor interruptions may replay temporary authoring only after the
  bridge/editor is repaired and temp cleanup is verified.
- Compile, structural, and manifest-step failures require inspection before
  replay.

## Decision

The executor should never hide an interruption as a generic authoring failure.
Bridge reset, connection refusal, editor shutdown, cleanup failure, compile
failure, and policy block are separate states because their replay behavior is
different.
