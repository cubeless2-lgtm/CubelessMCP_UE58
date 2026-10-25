# Tivret MCP — editor tool development

Implement concrete missing or defective authoring/inspection functionality in the owned Cubeless editor plugin. Use editor Python where sufficient and editor-only C++ where necessary. Preserve public contracts, batching, structured errors and changed-state reporting.

Inspect official Toolset Registry names/schemas and installed API signatures before development. Keep registration, operations and version compatibility separate; verify duplicate registration, teardown and relevant failure semantics. Official MCP is the server; external scripts are optional clients, never hidden required plugin implementations.

Apply [C++ boundaries](../policies/cpp-runtime-boundaries.md). Plugin editor C++ is allowed within the task; actual project C++ requires scoped `c++1218`. Do not create Runtime modules, gameplay APIs, runtime helper code or MCP-dependent outputs. Hand changed source, build/registration evidence, behavior checks and unresolved risks to QA/C++ review.
