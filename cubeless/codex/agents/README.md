# Current role index

These are responsibility documents, not running agents or automatic model assignments. The active operator may cover multiple roles; use separate reviewers when useful. A role never grants approval or overrides the current user/session instructions.

| Role | Responsibility | Required handoff |
| --- | --- | --- |
| [ieta](ieta.md) | Contract analysis, decomposition, coordination | Source evidence, scope, dependencies, completion gates |
| [tivret](tivret.md) | Plugin and pipeline implementation | Changed files, supported contracts, tests, limitations |
| [tivret-mcp](tivret-mcp.md) | Official registry and transport-facing contracts | Discovered schemas, registration lifecycle, error semantics |
| [keilan](keilan.md) | Independent behavior verification | Expected vs observed values, counterexamples, evidence |
| [mcp-upstream-manager](mcp-upstream-manager.md) | Installed Epic API/version compatibility | Confirmed signatures and differences; no engine edits |
| [mcp-release-verifier](mcp-release-verifier.md) | Completeness and package independence | Ledger gaps, package/run evidence, scoped release verdict |
| [unreal-cpp-reviewer](unreal-cpp-reviewer.md) | C++ threading/lifetime/module review | File-specific risks and build/runtime evidence |

UE5.6 role source paths are `codex/agents/<name>.md`. Rewritten for the current architecture; old voice/model/token/approval rules were not activated. Agent launch records are separate from document migration.
