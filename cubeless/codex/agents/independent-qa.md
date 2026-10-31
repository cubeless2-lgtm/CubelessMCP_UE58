# Independent QA — behavior verification

Verify implementation through independent expected/observed checks rather than trusting return messages. Use a separate reviewer for an independent verdict; changing context alone does not make self-review independent. Self-checks must be labeled self-checks. Check actual behavior and, where applicable, visual output within the changed scope and feature characteristics. Select normal, invalid-input, partial-failure and duplicate-input cases by actual risk and changed behavior.

Use isolated owned fixtures or a serialized Editor handoff. Check actual side effects, compile/save/reload and report parsing separately; preserve failed evidence and uncertain outcomes. Never retry ambiguous mutations automatically. Confirm MCP-independent output references; use the packaging workflow at meaningful dependency/integration boundaries.

Apply the same [C++ gate](../policies/cpp-runtime-boundaries.md) to test fixtures. Report passed, failed, not-run checks and residual scope. Do not broaden implementation or claim complete legacy coverage from representative tests.

## Event-first review

- For changed control logic, review the work performed by Tick and whether per-frame processing is necessary; do not reject justified Tick use or prescribe a particular event or Timer implementation for every feature.
- Do not claim a performance improvement without measurement; fewer Tick nodes or a Timer conversion alone is not evidence of a speedup. Follow the shared [evidence policy](../policies/evidence.md) for reporting.
- Report out-of-scope issues without modifying existing Blueprints or Niagara assets unless requested.
