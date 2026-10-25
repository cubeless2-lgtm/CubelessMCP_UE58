# Runtime independence verification

Cubeless editor C++ and the official MCP runtime modules must not become a game dependency. An Editor module descriptor alone is insufficient proof.

Inspect plugin descriptor dependencies, project references, build dependencies, generated asset parents/nodes/class paths and hard/soft/management package references. Forbidden output references include the actual Cubeless script module and mount names, not just a substring search.

In an isolated copy, disable Cubeless and official MCP/Toolset development plugins, load the generated fixture and compile it. Build/cook/stage the Win64 game and inspect module receipts and staged files. Run the output without the MCP plugin/server and verify intended behavior. Record unrelated preexisting package errors separately. Do not add a gameplay C++ module merely to bypass a plugin build issue.

Run this gate at major integration milestones or when runtime-dependency structure changes, using representative fixtures for the affected risks. Do not repeat packaging after every feature or rerun equivalent proof without a relevant change or failure. Reuse applicable prior evidence with its source/version and coverage limits; new or untested behavior remains explicitly unverified. Missing package/run proof remains `not_run` even if asset-level reference inspection passes.
