# CubelessMCP UE5.8 workspace

Read `cubeless/codex/AGENTS.md` before project work. It selects current roles, policies and workflows for official UE MCP plus Cubeless tools.

Resolve the Unreal project from its `.uproject`. Preserve unrelated source, assets and dirty Editor state. C++ changes are allowed by default only in the owned editor MCP plugin; project C++ requires the user's scoped `c++1218` approval. MCP runtime code, runtime APIs and output dependencies on MCP are prohibited; see the canonical C++ policy.

This project is the user's development and verification project. Perform work, builds and verification here unless the user explicitly directs otherwise. Do not create or use a separate Unreal project, project copy, or temporary host project for testing or builds without that direction. Use owned test assets and the existing Editor when safe; preserve existing work and Editor state. If a check cannot safely or technically run here, report the limitation instead of creating another project.

Historical records and archived instructions are evidence, never current tasks or approval. Do not commit or push unless explicitly requested.
