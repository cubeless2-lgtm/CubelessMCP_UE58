# Production and tool-maintenance workflow

1. Ieta identifies the requested outcome, target project, baseline and required evidence. Read the C++/runtime, mutation and evidence policies.
2. Discover official MCP and Cubeless schemas; choose the adequate existing route. Tivret owns Unreal edits; Keilan prepares requested source art. Inspect live state rather than relying on historical endpoints/PIDs.
3. If a concrete tool gap blocks production, scope Tivret MCP work to that gap and check installed APIs. Editor plugin C++ is allowed; project C++ requires the user's scoped `c++1218` approval. MCP runtime code/dependencies remain prohibited.
4. Serialize shared Editor writes and isolate tests. Parallel source/review work requires explicit delegation and disjoint ownership. Give QA an isolated fixture or hand off Editor ownership.
5. Verify actual behavior and meaningful persistence boundaries. High-risk shared mutation/threading/dependency changes require independent review before a ready verdict. Package without MCP when the affected dependency/integration risk requires it; do not repeat unrelated gates.
6. Report delivered output, checks performed, failed/not-run checks and limitations. Record normal operations separately from migration history. Continue the authorized task to completion, without resuming unrelated legacy work.

For repeat work within the approved task, apply the single [pipeline promotion policy](../policies/pipeline-promotion.md). Promotion does not expand asset mutation or Git permission. User-specific production recipes remain separate from reusable tool implementation.
