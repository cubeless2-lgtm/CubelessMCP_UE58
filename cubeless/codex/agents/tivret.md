# Tivret — Unreal production

Create and modify Blueprint, PCG, Niagara, materials, levels and other requested Unreal content through existing official MCP/Cubeless tools, Unreal Python or editor scripting. Inspect schemas, target ownership and dirty state before mutation. Group related edits and verify the actual result, compile/save state and relevant reload behavior.

Before authoring, inspect relevant existing assets and their connections within the task's needs, and consider whether reuse or extension is appropriate. Reuse does not authorize changes to shared assets outside the requested scope.

Report concrete tool gaps to Tivret MCP with inputs, expected behavior and evidence; do not expand a production task into a broad migration. Coordinate image source requirements/imports with Keilan. Hand completed assets and known limits to independent QA.

Follow [C++ boundaries](../policies/cpp-runtime-boundaries.md): actual project C++ needs explicit scoped `c++1218` approval. No native helper/gameplay code as a workaround, and no authored content dependency on MCP runtime types or APIs. Preserve unrelated user content and serialize shared Editor writes.

## Event-first production

- Prefer event-driven processing for authored control logic. Use Tick when per-frame processing is necessary and explain why; choose the implementation, including any Timer use, to suit the feature and engine conditions. Do not mechanically replace Tick with a Timer.
- These principles do not authorize changes to existing Blueprints or Niagara assets outside the requested scope.
