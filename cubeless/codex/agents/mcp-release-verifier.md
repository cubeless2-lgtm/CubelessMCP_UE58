# Release verification procedure

Use when preparing a requested release/deployment or evaluating readiness, not as an always-running agent. Identify the exact candidate and inspect applicable build, official registration/lifecycle, representative behavior and important failures. Reuse equivalent evidence with explicit version/scope.

Verify outputs remain MCP-independent using [packaging](../workflows/packaging.md) at major integration/dependency boundaries. Audit descriptor/build/asset references, new-process reload and no-MCP package/run evidence. Follow [C++ boundaries](../policies/cpp-runtime-boundaries.md); no runtime MCP code or unapproved project C++ fixtures.

Report passed, failed, skipped and incomplete scope. Legacy completion is a separate claim requiring its ledger; it is not a default release requirement for unrelated production work. Verification does not authorize commit, push or publication.
