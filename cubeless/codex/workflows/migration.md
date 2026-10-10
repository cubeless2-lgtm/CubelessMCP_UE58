# Migration workflow

1. Capture baseline and read the current feature/pipeline ledger. Read the actual source call chain and input/output/error contracts; include dirty and untracked legacy additions.
2. Inspect installed UE5.8.3 APIs and official registry schema. Plan specific Toolsets or prove official-tool equivalence. Keep unresolved/private APIs visible.
3. Implement editor-only registration → operations → UE58 compatibility. Keep plugin load/call independent of external orchestration scripts.
4. Prioritize a healthy plugin: build, discover actual schemas in official MCP, and exercise representative core operations and failure states on isolated fixtures. Select postcondition readback and negative cases by risk and changed behavior; reuse applicable evidence instead of repeating equivalent checks.
5. Compile, save and reload at meaningful persistence boundaries; a new process is required before claiming cold-reload verification. Check duplicate input, intermediate failure and uncertain transport handling when relevant to the changed contract, without automatically retrying uncertain mutations.
6. Treat external pipelines and verifiers as separate migration targets. Record their sequence, IO, state changes and untested stages. Add report-tampering tests or another independent verifier only when a concrete risk, prior defect or changed contract justifies it; avoid redundant report layers.
7. Inspect runtime references when relevant and group the no-MCP package workflow at major integration or runtime-dependency changes, not after every feature. Maintain every legacy feature/pipeline disposition and honest evidence levels; representative verification does not prove untested behavior.

User authorization for this migration permits routine progress through these stages. Ask only when a material decision or an action outside the authorized scope is required, and identify the exact reason.
