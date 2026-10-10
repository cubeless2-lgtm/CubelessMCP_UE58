# Evidence levels and completion

Use explicit statuses: `unassessed`, `source_reviewed`, `archived`, `implemented`, `registered`, `executed`, `behavior_verified`, `blocked`. Archive is not implementation. Registration is not behavior verification. For mutation also report compile, save, reload, dependency inspection and no-MCP package/run separately.

Evidence records include source paths/hashes/commit, engine build, project, test input fingerprint, toolset/schema, stage timing, observed output, expected checks, side effects and reports. Mark absent checks `not_run`; do not convert them into passes. Choose representative success and failure cases by risk and changed behavior, prioritizing build, registration, core operations and truthful failure state. Report-tampering tests and additional independent verification need a concrete risk, prior defect or changed contract; do not duplicate equivalent evidence routinely.

Keep complete feature and pipeline tracking without requiring every verification layer for every feature. Reuse applicable evidence with its source version and scope; save/reload at meaningful persistence boundaries and group packaging at major integration or runtime-dependency changes. An untested variant remains explicitly untested even when a representative case passes.

Legacy ledger final dispositions are: (1) Cubeless implemented+registered+required verified behavior; (2) official tool proven equivalent in IO/defaults/errors/side effects/verification; (3) old transport-only functionality removed with evidence; (4) incomplete. Planned disposition is distinct from final disposition. File counts, routes, Python definitions and pipeline candidates overlap and must not be added as unique feature totals.

Record performance with call count, compile/save count, workload and wall time. Source LOC and person-hour estimates are not measured engine performance. Past UE5.6 evidence stays labeled UE5.6 until independently rerun in UE5.8.
