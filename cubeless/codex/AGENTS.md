# CubelessMCP UE5.8 operating instructions

These standing rules cover Unreal production using official UE MCP and Cubeless extensions, and maintenance of those tools. User instructions and actual session permissions take precedence. Resolve the target `.uproject`, installed engine and live endpoint; never assume old paths, ports, PIDs or DLLs. The current supported engine baseline is UE 5.8.3; other versions need explicit compatibility evidence.

## Start and route

- Read [roles](agents/README.md), [C++ and runtime boundaries](policies/cpp-runtime-boundaries.md), [mutation safety](policies/mutation-safety.md), and [evidence](policies/evidence.md).
- Follow [production workflow](workflows/production.md) for ordinary work. Ieta plans and coordinates; Tivret authors Unreal content; Tivret MCP develops tools; Keilan creates image/texture sources; independent QA verifies behavior.
- Discover registered tool names and schemas. Choose official MCP or Cubeless by required behavior, defaults, failure semantics and evidence. Prefer an adequate existing tool; do not duplicate official functionality without a concrete gap.
- A production request does not automatically authorize broad tool expansion or a legacy migration campaign. Resolve necessary gaps within the user's task scope; report material scope changes.
- Common repeat work follows the single [pipeline promotion policy](policies/pipeline-promotion.md). Resolve tools and examples from the identified project-local `cubeless`, never a fixed PC path or project name. See [installation](../docs/installation.md) for layout, project selection and host instruction attachment; installing the plugin alone does not load these instructions into an agent session.

## C++ and runtime boundaries

- Only owned MCP editor-plugin C++ is allowed by default: `Plugins/CubelessMCP_UE58/`. Editor-only plugin C++ does not require `c++1218`.
- Creating or modifying actual project C++ outside that boundary, including gameplay modules, C++ helpers and project build/target files, is prohibited without the user's explicit scoped `c++1218` approval. Explain the need, alternatives and exact scope before requesting it. Routine assent is not this approval.
- Do not create runtime modules, runtime code or runtime APIs in MCP plugins. Do not hide gameplay code in an editor plugin or another plugin to bypass the project approval rule.
- Generated content must load, compile, cook, package and run without Cubeless, official MCP, Toolset development plugins or an MCP server. No serialized MCP class/function/component/structure/content dependencies or game-to-MCP build dependencies. `c++1218` does not waive this prohibition.
- Use standard engine/runtime features and independent Blueprint content. Read [packaging](workflows/packaging.md) when verifying runtime independence.

## Work ownership and preservation

- One owner serializes shared Editor and asset mutations on the required game thread. Parallel source work uses disjoint files. Independent QA gets a controlled handoff or an isolated project.
- Roles are responsibilities, not automatic processes or model assignments. Launch subagents only when the user or applicable instructions explicitly request delegation/parallel work. Do not call self-review independent review.
- Preserve existing local changes, assets, Python startup and upper-level RetargetTools. Inspect disk and in-memory dirty state before restart; never silently save/discard user work.
- Never retry uncertain mutations automatically. Inspect observed state after a timeout/disconnect; report partial changes and retry risk. Undo is not atomic rollback.
- Use owned `/Game/_MCP_Temp/<feature>/CubelessTests/<unique-run>/` verification assets. Do not use `CubelessMCPTests` in new dependency-guarded inputs. Inspect verifier code before treating it as read-only.
- Isolated verification project basenames must be at most 20 characters. Existing `cubeless/workspaces/UE58Verification/CubelessVerify58.uproject` is a candidate, not proof of a running or clean instance.

## Tool maintenance and evidence

- Plugin editor Python belongs in `Plugins/CubelessMCP_UE58/Content/Python/cubeless_mcp_ue58/`; external clients/validators belong in `cubeless/tools/`.
- Use official ModelContextProtocol and Toolset Registry. Do not introduce a required external server/private TCP bridge or edit Epic sources. Keep registration, domain operations and installed-engine compatibility separate.
- Preserve public contracts, batch semantics and structured failure/change reporting. Apply [tool optimization](policies/tool-optimization.md) for consolidation.
- Select verification by changed behavior and risk. Distinguish implementation, registration, invocation, behavior, compile/save/reload and independent packaged execution. Record untested variants honestly; registration and successful builds do not prove behavior.
- Do not infer unsupported or unimplemented behavior from missing tests. Follow [evidence](policies/evidence.md) to distinguish unverified scope, confirmed restrictions, actual defects and verifier false rejections. Preserve verified normal paths and original failure records; correct only evidence-backed current misstatements without restarting migration or broad validation.
- Apply dependency-conditioned availability to every API and pipeline: use plugin-dependent features when their required plugins are enabled and loaded. State those prerequisites per feature; a disabled prerequisite means activation is required, not an implementation defect or an incomplete port. When normal behavior is verified under the stated prerequisites, report that scope as complete/usable. Follow [evidence](policies/evidence.md) for the shared verdict rule; do not require unrelated projects to enable every optional plugin.
- Require separate review for high-risk shared mutation, threading/lifetime or runtime-dependency changes before declaring them ready. If independent review is unavailable, report it as pending rather than claiming it occurred.
- Keep documentation-only validation proportional; no Editor/build run solely for prose changes.

## Records and historical material

- Normal operational records: `cubeless/records/operations/`; executable evidence: `cubeless/records/verification/`; migration records: `cubeless/records/migration/`. Record milestones in `cubeless/work-log.md`; do not store operational records in game content.
- Record requested implementation studies in `cubeless/studies/<topic>/README.md` or clearly named Markdown documents. Include sources, reproduction/tool-call flow, settings, observed results, failure lessons and unverified scope; link owned `Content/Study/<topic>` assets when present. Study Markdown is Git-eligible under the current ignore rules; local helper scripts/raw evidence stay excluded. Git eligibility never authorizes staging or publishing.
- Read [migration workflow](workflows/migration.md) only for a requested migration task. Incomplete legacy coverage remains incomplete, but is not an automatic work queue.
- The development checkout keeps previous instructions under `cubeless/records/operations/2026-10-25-agent-reorganization/` and historical lessons under `cubeless/codex/knowledge/`. These local records/archive are not required distribution files. Historical phase orders, permissions, stop conditions, PIDs and DLLs never authorize or constrain a new task by themselves.
- `archive/ue56/` and old `.source` files are historical evidence only. UE5.6 source remains read-only unless the user explicitly requests work there. Historical lessons retain their stated engine/proof levels.
- Do not stage, commit or push without the user's explicit request. Keep each repository's Git scope separate.
