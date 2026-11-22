# CubelessMCP UE5.8 workspace

Read `cubeless/codex/AGENTS.md` before project work. It selects current roles, policies and workflows for official UE MCP plus Cubeless tools.

Resolve the Unreal project from its `.uproject`. Preserve unrelated source, assets and dirty Editor state. C++ changes are allowed by default only in the owned editor MCP plugin; project C++ requires the user's scoped `c++1218` approval. MCP runtime code, runtime APIs and output dependencies on MCP are prohibited; see the canonical C++ policy.

This project is the user's development and verification project. Perform work, builds and verification here unless the user explicitly directs otherwise. Do not create or use a separate Unreal project, project copy, or temporary host project for testing or builds without that direction. Use owned test assets and the existing Editor when safe; preserve existing work and Editor state. If a check cannot safely or technically run here, report the limitation instead of creating another project.

Use the project's running Editor for Unreal work and verification, including plugin development. A separate DebugGame, headless, or parallel Editor session requires explicit user direction. When a plugin build requires replacing a loaded DLL and the user has authorized a restart, check for unsaved work, end the current Editor normally, build the project Editor target, and reopen the same project in its regular Development Editor for verification. Follow the [Editor lifecycle](cubeless/codex/workflows/verification-lifecycle.md) for shutdown and relaunch checks.

Historical records and archived instructions are evidence, never current tasks or approval. Do not commit or push unless explicitly requested.


## Mocap delivery: show the result and hand off the Editor

User preference (2026-11-15): after creating or correcting mocap, show the saved result in this project's regular Editor by default, with SKM_Quinn and synchronized source/result playback when available. Leave the Editor open for the user unless they explicitly request shutdown. A session used to show the deliverable is handed off to the user; record its PID and displayed asset as a user handoff and exclude it from owned verification-session cleanup. Reuse an existing Editor and preserve dirty packages/PIE. This does not authorize parallel Editors or silent saves/discards. Use semantic Unreal APIs; do not use Computer Use.
