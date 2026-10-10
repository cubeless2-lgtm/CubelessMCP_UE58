# Legacy lessons awaiting UE5.8 confirmation

Source: `codex/docs/memory/promoted-lessons.md`, with supporting workflow/reports inventoried in `records/migration/planning-2026-10-10/legacy-operations-inventory.json`. These are file-backed historical observations, not imported chat memory. Archive manifest records exact source hashes. No row below claims UE5.8 runtime validation.

| Cause observed/reported in UE5.6 | Mitigation to evaluate | Applies when | Evidence level / UE5.8 status |
| --- | --- | --- | --- |
| Inline map loading retained world references and could report World Memory Leaks | Native preflight and deferred load with status before subsequent work | Replacing editor worlds during MCP work | Historical summary; not rerun |
| Vertex UV lookup on a nonexistent channel could assert | Query and bound-check channel count first | Static mesh UV access | Historical summary; not rerun |
| RPC timeout left mutation outcome uncertain | Inspect actual object state before deciding on retry | Mutation disconnect/timeout | Historical failure principle; engine scenario not rerun |
| Project-specific helper classes became asset dependencies | Use engine-native results and inspect references after reload | Blueprint/graph authoring | Current architectural requirement; package proof pending |
| Python verifier reported success before saving level | Record save result before final success; preserve failure report | SplineLoopConveyor saved verifier | Source review of `scripts/study_spline_loop_conveyor/verify_pure_saved.py`; not rerun |
| Cleanup exception could suppress the primary smoke report | Record cleanup independently and always persist accumulated stages | `Python/scripts/smoke_blueprint_mutation_contract_live.py` | Source-reviewed risk; new external harness validation separate |
| PASS/phase alone accepted stale prerequisite evidence | Compare input/environment fingerprints | ReactiveSurface recovery gate | Source-reviewed contract gap; not rerun |

Do not mechanically reuse old ports, fixed drive paths, engine ceiling rules, model settings or approval tokens. Read original context before applying any mitigation; a historical workaround can be unnecessary or wrong in UE5.8.
