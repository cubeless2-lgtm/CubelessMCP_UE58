# Migration workflow

1. Capture baseline and read the current feature/pipeline ledger. Read the actual source call chain and input/output/error contracts; include dirty and untracked legacy additions.
2. Identify the actual target UE5.8.x engine version/build, then inspect its installed APIs and official registry schema. Plan specific Toolsets or prove official-tool equivalence. Keep unresolved/private APIs visible.
3. Implement editor-only registration → operations → UE58 compatibility. Keep plugin load/call independent of external orchestration scripts.
4. Prioritize a healthy plugin: build, discover actual schemas in official MCP, and exercise representative core operations and failure states on isolated fixtures. Select postcondition readback and negative cases by risk and changed behavior; reuse applicable evidence instead of repeating equivalent checks.
5. Compile, save and reload at meaningful persistence boundaries; a new process is required before claiming cold-reload verification. Check duplicate input, intermediate failure and uncertain transport handling when relevant to the changed contract, without automatically retrying uncertain mutations.
6. Track external pipelines and verifiers separately from API functionality. Execute only the migration scope requested for the current task; historical phase restrictions and permissions do not carry forward. Preserve original contracts, source mappings and incomplete coverage.
7. Inspect runtime references when relevant and group the no-MCP package workflow at major integration or runtime-dependency changes, not after every feature. Maintain every legacy feature/pipeline disposition and honest evidence levels; representative verification does not prove untested behavior.

This workflow grants no authorization. Apply it only to a current user-requested migration, within that scope and the standing C++/runtime policy.



For UE5.8 Python fixtures, follow [the observed closure/GC lifetime limitation](../knowledge/ue58-python-closure-gc.md). Keep bound Unreal callables out of abandoned temporary exec-global cycles, own fixture module lifetimes explicitly, and preserve/restore automatic GC state if diagnostics change it. Do not patch engine Python type slots or treat GC suppression as a product fix.

For generated or spliced Python candidates, use a no-import `compile(source, filename, "exec")` syntax check as well as AST contract comparisons. AST parsing alone does not reject every executable syntax error; see [the FE repeated-keyword incident](../knowledge/python-candidate-compilation.md). Reuse already-passed behavior evidence when this catches a candidate-only defect.
