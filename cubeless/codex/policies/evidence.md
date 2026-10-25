# Evidence levels and completion

Use explicit statuses: `unassessed`, `source_reviewed`, `archived`, `implemented`, `registered`, `executed`, `behavior_verified`, `blocked`. Archive is not implementation. Registration is not behavior verification. For mutation also report compile, save, reload, dependency inspection and no-MCP package/run separately.

Evidence records include source paths/hashes/commit, engine build, project, test input fingerprint, toolset/schema, stage timing, observed output, expected checks, side effects and reports. Mark absent checks `not_run`; do not convert them into passes. Choose representative success and failure cases by risk and changed behavior, prioritizing build, registration, core operations and truthful failure state. Report-tampering tests and additional independent verification need a concrete risk, prior defect or changed contract; do not duplicate equivalent evidence routinely.

Keep complete feature and pipeline tracking without requiring every verification layer for every feature. Reuse applicable evidence with its source version and scope; save/reload at meaningful persistence boundaries and group packaging at major integration or runtime-dependency changes. An untested variant remains explicitly untested even when a representative case passes.

Legacy ledger final dispositions are: (1) Cubeless implemented+registered+required verified behavior; (2) official tool proven equivalent in IO/defaults/errors/side effects/verification; (3) old transport-only functionality removed with evidence; (4) incomplete. Planned disposition is distinct from final disposition. File counts, routes, Python definitions and pipeline candidates overlap and must not be added as unique feature totals.

Record performance with call count, compile/save count, workload and wall time. Source LOC and person-hour estimates are not measured engine performance. Past UE5.6 evidence stays labeled UE5.6 until independently rerun in UE5.8.

Identify binaries by source-bundle inventory, engine build, DLL hash and an existing artifact location. Reuse those references across checks instead of making a DLL/PDB backup for every verification. Keep the active projects' required binaries/symbols, one verified latest recovery build and any distinct failure artifact needed for reproduction. Remove only stopped, owned, demonstrably regenerable or redundant outputs; preserve source/config/assets and raw success/failure records.

Current baseline summaries must be built from explicit current fields. Do not spread an older baseline record into a new one while replacing only `files`/`count`: old `protected_original_count`, `total_count`, owned-asset maps and source receipts can remain misleading. Keep previous summaries under a named historical object, preserve the original record, and correct metadata through a successor without repeating Engine behavior.
