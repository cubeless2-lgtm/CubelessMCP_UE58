# StackOBot Animation Request Run Template

Copy this template when executing a concrete StackOBot animation request. Keep it
as the per-request working record until the final result is accepted.

## Request

```text
user_request:
date:
operator:
```

## Compiled Intent

Use `docs/stackobot-request-compiler-drills.md` before filling this section.
Then compare the compiled fields against
`docs/stackobot-animation-route-matrix.md`.

```text
target_character:
target_body_area:
timing_type:
runtime_layer:
route:
sample_target:
first_read_or_authoring_command:
verification_command:
expected_evidence:
handoff_template:
cxx_api_status:
ask_user_first:
route_matrix_checked:
route_token_document_map_checked:
route_token_acceptance_map_checked:
route_matrix_notes:
```

## Assumptions

- Reversible assumption:
- User approval already granted:
- Approval still required:

## Safety Scope

| Check | Value |
| --- | --- |
| Original StackOBot assets modified? | `false` |
| Original maps saved? | `false` |
| Sample root | `/Game/_MCP_Sample/AnimStudy` |
| Evidence root | `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy` |
| Bridge | `127.0.0.1:55557` |
| Active plugin copy | `<workspace-parent>/SampleProject/StackOBot/Plugins/UnrealMCP` |

## Preflight Checklist

Fill this before any editor state mutation, transient actor setup, or sample
asset authoring.

| Check | Result | Evidence |
| --- | --- | --- |
| StackOBot project path exists | | |
| Primary bridge `127.0.0.1:55557` reachable | | |
| Active StackOBot plugin copy confirmed | | |
| Required command exposed by current plugin copy | | |
| Command-surface sync needed before C++? | | |
| Pre-existing dirty packages captured | | |
| Sample target under `/Game/_MCP_Sample/AnimStudy` | | |
| `allow_non_sample=false` for authoring commands | | |
| Evidence target under `Saved/MCP/AnimStudy` | | |

## Tivret Handoff

Paste the visible handoff block selected from
`docs/stackobot-animation-tivret-handoff-templates.md`.

```text
Tivret handoff:
```

## Execution Log

| Step | Command or action | Result | Artifact |
| --- | --- | --- | --- |
| 1 | Bridge/preflight | | |
| 2 | Static read/topology | | |
| 3 | Sample authoring or reuse | | |
| 4 | Compile/save | | |
| 5 | Runtime proof | | |
| 6 | Dirty package check | | |
| 7 | Cleanup | | |

## Acceptance Checklist

Use `docs/stackobot-animation-acceptance-checklist.md`.

| Gate | Pass? | Evidence |
| --- | --- | --- |
| route classification and why it was chosen | | |
| assets created or reused | | |
| whether original StackOBot assets were modified | | |
| compile/save result for authored sample assets | | |
| runtime world used for proof | | |
| evidence artifact paths under `<workspace-parent>/SampleProject/StackOBot/Saved/MCP/AnimStudy` | | |
| command `errors` and `warnings` | | |
| dirty content and map package status | | |
| cleanup status for transient actors and play sessions | | |
| C++/API decision recorded: C++/API decision: `not needed`, `candidate`, or `implemented` | | |
| Route-specific proof | | |
| Runtime evidence strength is sufficient | | |
| Route token document map checked | | |
| Route token acceptance map checked | | |

## Final Report Draft

```text
route:
assets_created_or_reused:
original_assets_modified:
runtime_world:
main_command_results:
pose_or_state_evidence:
errors:
warnings:
dirty_packages:
cleanup:
cxx_api_needed:
artifact_paths:
residual_risk:
```

## Work-Log Entry Draft

```markdown
## YYYY-MM-DD StackOBot [request title]

- Request:
- Route:
- Assets/evidence:
- Verification:
- C++/API decision:
- Dirty packages/cleanup:
- Residual risk:
```
