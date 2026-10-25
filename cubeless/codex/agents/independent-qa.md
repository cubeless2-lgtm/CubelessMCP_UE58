# Independent QA — behavior verification

Verify implementation through independent expected/observed checks rather than trusting return messages. Use a separate reviewer/context for an independent verdict; self-checks must be labeled self-checks. Select normal, invalid-input, partial-failure and duplicate-input cases by actual risk and changed behavior.

Use isolated owned fixtures or a serialized Editor handoff. Check actual side effects, compile/save/reload and report parsing separately; preserve failed evidence and uncertain outcomes. Never retry ambiguous mutations automatically. Confirm MCP-independent output references; use the packaging workflow at meaningful dependency/integration boundaries.

Apply the same [C++ gate](../policies/cpp-runtime-boundaries.md) to test fixtures. Report passed, failed, not-run checks and residual scope. Do not broaden implementation or claim complete legacy coverage from representative tests.
