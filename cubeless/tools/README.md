# External operations and verification

These scripts use ordinary external Python (tested here with CPython 3.12). They do not import `unreal`, start an MCP server, or supply plugin implementation. The plugin's editor tools remain self-contained under `Plugins/CubelessMCP_UE58/`.

`official_mcp_client.py` connects to the official UE5.8 MCP HTTP endpoint. It negotiates `2025-11-25`, sends the assigned session and negotiated-version headers, and does not request SSE progress. This matches the installed UE5.8.3 `ModelContextProtocolServer.cpp` non-progress JSON path. Request errors never trigger automatic mutation retry. Endpoint is explicit; no legacy port/env defaults or credential changes are used.

## Read-only discovery

```powershell
python -B cubeless/tools/official_mcp_client.py --endpoint http://127.0.0.1:8001/mcp --output cubeless/records/verification/run01/discovery.json
```

The endpoint above is an example for the isolated verification editor; choose the actual official endpoint. A successful discovery is not plugin behavior proof. The script records initialize/list/schema evidence and does not mutate assets.

## Blueprint mutation contract smoke

```powershell
python -B cubeless/tools/pipelines/blueprint_mutation_smoke.py --endpoint http://127.0.0.1:8001/mcp --expected-project C:/path/to/Isolated.uproject --phase author --output cubeless/records/verification/run01/author.json
```

The author phase checks native project/engine/dirty-state diagnostics, discovers current schemas, and uses only a UUID-owned `/Game/CubelessMCPTests/Run_<id>/BP_MutationContract` fixture. It verifies invalid name and duplicate creation rejection, creates two Branch nodes, connects exec pins, reads back their connection, moves a node, reads back position, deletes that node, reads back deletion, compiles/saves once, reads back asset identity and audits saved package dependencies. Graph failure cases reject a stale position precondition and exec-linked deletion without opt-in, then independently read back unchanged state. Every stage records actual results and timing; uncertain transport mutations stop without retries.

This ports the representative `Python/scripts/smoke_blueprint_mutation_contract_live.py` flow (OPS56-VERIFY-002). It is not parity for all legacy graph tooling. Cold reload and cleanup are deliberately explicit phases, with JSON evidence retained even on failure. The existing legacy smoke did not prove save/reload; these are added gates.

After safely restarting only the isolated editor on the same project, run:

```powershell
python -B cubeless/tools/pipelines/blueprint_mutation_smoke.py --endpoint http://127.0.0.1:8001/mcp --expected-project C:/path/to/Isolated.uproject --phase reload --parent-report cubeless/records/verification/run01/author.json --output cubeless/records/verification/run01/reload.json
```

Reload refuses the author editor's instance ID, and checks Blueprint identity and surviving/deleted nodes from disk in a new editor instance. The script itself never stops/starts editors. Keep the fixture until independent no-MCP packaging validation finishes. Afterwards run `--phase cleanup` with the author report and a fresh output path. Cleanup deletes only the exact owned asset; an empty virtual folder may remain. Cleanup is not rollback and is not automatically attempted after unknown mutation outcomes.

All phases refuse to overwrite evidence. Inspect operation journal and actual fixture state before recovering a failed run. A fresh author invocation gets a new UUID fixture; it does not retry or resume the old mutation.

## Independent report checks

```powershell
python -B cubeless/tools/verification/pipeline_report.py cubeless/records/verification/run01/author.json --phase author
python -B cubeless/tools/verification/test_pipeline_contracts.py
```

The report verifier rejects missing stages, failed save/compile, stale input/environment fingerprint, unknown outcomes and same-instance reload. The tests use synthetic fixtures and timeout counterexamples only; their pass is not live Unreal evidence. No phase of this harness alone proves MCP-free packaged execution.

`archive_legacy_documents.py` is a one-time byte-preserving archival utility. It reads the source and Git objects, writes `.source` variants and refuses existing output manifests. See `cubeless/codex/archive/ue56/README.md` for its bounded scope and exclusions.
