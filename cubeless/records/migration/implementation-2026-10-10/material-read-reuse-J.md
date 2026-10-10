# Material read reuse and observed work counters, J

Implementation scope: only `get_material_parameter_collection_values` and `list_material_collection_parameter_nodes` omit a redundant middle read. Root subsequently built J successfully (UBT 12.27 seconds) and verified six cases with eleven samples each against I: all compared DTO fields matched; the four success cases observed two captures, one reuse, no Execute and two applicable collection reads. Error paths retained one capture. Material author regression passed 43 steps. See `cubeless/records/verification/implementation-2026-10-10/material-read-reuse-results-J.md`, baseline-I and comparison-J JSON records. HTTP medians were heavily quantized around 16 ms; no end-to-end speedup claim follows from the traversal reduction.

## Existing work versus reused work

`CubelessMaterialOperations.cpp` previously always captured Before, executed a handler, and independently captured After. For MPC values, `CaptureState` already calls `ReadEffectiveCollectionValues` and keeps its complete output as `Before.Snapshot`. For collection nodes it already calls `ReadEffectiveCollectionNodes` and stores its complete output under `Before.Snapshot.effective_collection_nodes`. `Execute` called those same readers again. The middle result added no independent post-operation assurance: the final CaptureState performed that comparison.

`TryReuseCapturedReadData` selects only those two operation IDs and reuses only their local captured payload. It performs no clone, global cache, caller-provided cache or asset lookup. The response assembly does not mutate the selected shared payload. A missing expected payload falls back to the original Execute route. Other operations, including all mutations, retain Execute. After CaptureState and snapshot/dirty comparison are unchanged; failures retain their preflight/readback/execute/verification codes and observed-change semantics. No save or compile is added or removed.

The source-derived successful-path expectation is:

| Work | I source path | J source path |
| --- | --- | --- |
| GetCollectionValues effective reader | 3 | 2 |
| ListCollectionNodes effective node reader | 3 | 2 |
| Independent captures, both tools | 2 | 2 |
| ListCollectionNodes graph summaries | 2 | 2 |
| ListCollectionNodes direct collection metadata inspections | 2 | 2 |
| Mutation Execute / before / after | preserved | preserved |

These are source expectations, not a claim that all native work drops by one third. ListCollectionNodes still serializes full graph snapshots and checks metadata twice. Its collection views may be rebuilt separately for graph metadata and node validation. General list/analyze have additional filters, summary fields, Material Instance handling and compile-resource observations; projecting those from a previous snapshot requires separate behavior review. This slice intentionally does not eliminate those checks.

## Observed measurements

Every native Material response, including a handled error, gains `performance`:

- `observed_counts`: counters increment at the executed helper entry, loop visit or public getter call site. Missing keys mean zero. The primary counters are `capture_state_calls`, `execute_dispatch_calls`, `captured_read_payload_reuses`, `collection_values_read_calls`, `collection_nodes_read_calls`, `collection_view_build_calls`, `collection_base_chain_visits`, `collection_parameter_declaration_visits`, `collection_effective_name_enumerations`, `collection_typed_parameter_lookups`, `collection_parameter_id_lookups`, `collection_asset_default_lookups`, `collection_runtime_value_lookups`, `collection_node_expression_visits`, `direct_collection_metadata_calls`, `collection_metadata_expression_visits`, and `graph_summary_capture_calls`.
- `native_elapsed_ms_before_final_serialization`: `FPlatformTime::Seconds` elapsed time for the synchronous native helper, including inspection/verification and counter overhead. It excludes Python/journal/MCP transport and final envelope serialization. It is not end-to-end latency or a GPU/shader measurement.
- Explicit scope and semantics strings distinguish observed entry/visit counts from uninstrumented engine-internal recursive work.

`FReadMetricsScope` uses a thread-local current-scope pointer and restores the previous pointer on destruction. Nested calls get their own counters; there is no process-wide accumulating total and no cross-call cached data. The actual operations remain game-thread gated. `read_data_source` records `local_before_capture` or `execute` once dispatch is reached.

I was built before these counters existed. Its internal counts above are a source-derived baseline, never retroactively measured counts. Root can compare stored I payloads/latencies with J. Exact measured internal-count comparisons require an instrumented unoptimized baseline, which this slice does not claim to have run.

## Actual validation plan

Use the same isolated saved MPC chain and collection-expression Material for I and J, with identical arguments. Compare complete native `data`, `before`, `after`, dirty/change flags, verification, error code/stage and limits. Ignore only the additive `performance`/`read_data_source` fields and genuinely external envelope identity/timing. Do not normalize away graph, GUID, value, inheritance or error differences.

J success must show two capture calls, one local payload reuse, no Execute dispatch, and two effective reader calls for the applicable reader. Budget/missing-asset failure must have no mutation or reuse. Existing mutation smoke must still show one Execute and two captures, with no local payload reuse. Repeated read calls must begin counters from zero. Timings need warm/cold conditions and sample size; a single smaller duration is not a performance conclusion.

## Next legacy Material tool

The remaining public Material count is ten, after the existing eleven named tools. `add_custom_material_node` is the best bounded next authoring candidate: specialize existing Add-node validation/conversion/cleanup/readback instead of duplicating its core. Its code, output type, input names, description, position, defines and include paths need exact schema adaptation and bounds, with shader include dependency checks and separate compile-proof status. Existing Add supporting arbitrary properties is not itself proof of this specialized pipeline or public tool parity.

`set_material_parameter_collection_values` follows only after inherited read fixtures pass and value-change versus layout-change behavior is controlled. Preserve asset/runtime/save booleans; a runtime-only subset cannot silently ignore default asset/save requests. Mirror/reference replacement/function expansion/ticker/cache repair and true async compile/save remain separate incomplete work. Source reasons are recorded in `material-next-slice-and-mpc-inheritance.md` and `async-material-compile-plan.md`; no new public tool is counted by this optimization.
