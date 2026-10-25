# Current roles

Roles describe responsibilities, not running agents, fixed model choices or a permanently active team. Routine work may use one operator across roles. Independent review requires another reviewer; never relabel self-review. Subagent launch follows the current user/session delegation rules. Use disjoint source ownership and one shared Editor mutation owner.

| Role | Responsibility | Handoff |
| --- | --- | --- |
| [Ieta](ieta.md) | Planning, tool selection, coordination | Goal, scope, alternatives, ownership, completion gates |
| [Tivret](tivret.md) | Unreal content production | Assets changed, compile/save state, observed behavior |
| [Tivret MCP](tivret-mcp.md) | Cubeless editor tools and registry development | Contracts, changed source, build/schema evidence, gaps |
| [Keilan](keilan.md) | Image and texture source creation | Source files, intended shader use, alpha/channel meanings |
| [Independent QA](independent-qa.md) | Independent behavior verification | Expected/observed results, failures, persistence, limits |
| [Unreal C++ reviewer](unreal-cpp-reviewer.md) | Native safety and dependency review | Concrete findings, approval scope, evidence |
| [API compatibility researcher](mcp-upstream-manager.md) | Support role for installed Epic APIs | Versioned source signatures and constraints |
| [Release verifier](mcp-release-verifier.md) | Release procedure, not a permanent worker | Tested candidate, coverage, package/run evidence, pending items |

All roles follow [C++ and runtime boundaries](../policies/cpp-runtime-boundaries.md). Historical model/token/voice rules are not implicitly loaded. Ieta's planning, Tivret's production and Keilan's image responsibilities are restored; current Korean communication rules apply. Use neutral technical language unless the user requests a character voice.
