# Unreal C++ Convention Checklist

This page is the working convention baseline for CubelessStylized C++ review and small implementation work. It is not a mass-format mandate.

## Source Priority

1. Epic official Unreal C++ coding standard.
2. Unreal Engine and Lyra local style when it clearly shows the expected engine-side pattern.
3. CubelessStylized project-specific rules in `AGENTS.md` and this file.
4. External community checklists as supporting references only.

When sources disagree, prefer the higher-priority source unless the local module has a strong compatibility reason.

## Adoption Rules

- Do not mass-format existing C++ just to apply this convention.
- Apply convention checks to new C++ and files already touched for a real change.
- Keep third-party plugin style stable unless correctness, crash prevention, build compatibility, or project integration requires a change.
- Use `.editorconfig` only for safe editor defaults until a module-specific formatter plan is approved.
- Trial `.clang-format` only on a small sample or temporary copy before adopting it for real source files.

## Naming and File Shape

- Use Unreal type prefixes: `U` for UObject classes, `A` for actors, `F` for structs and non-UObject types, `S` for Slate widgets, `I` for interfaces, and `E` for enums.
- Prefix bool members and variables with `b`.
- Use PascalCase for types, functions, properties, and enum values unless matching an existing API.
- Keep `*.generated.h` as the last include in reflected headers.
- Keep includes explicit and local. Do not rely on transitive includes when adding new code.
- Keep comments sparse and useful. Explain lifecycle, ownership, thread handoff, or non-obvious Unreal constraints.

## UObject and Reflection Safety

- Use `UPROPERTY` for UObject references that need GC tracking, serialization, Blueprint exposure, editor editing, or asset persistence.
- Prefer `TObjectPtr` for owned or tracked UObject members in UE5 code.
- Use `TWeakObjectPtr` for references that should not extend object lifetime, especially editor UI, async callbacks, cached actors, and transient selections.
- Do not store raw UObject pointers across frames, async work, Slate callbacks, or editor shutdown unless lifetime is externally guaranteed and documented.
- Bind and unbind delegates deliberately. Review shutdown paths, `EndPlay`, `BeginDestroy`, module shutdown, and editor restart behavior.
- Avoid relying on constructor-only state for Blueprint-editable components when instance defaults, re-instancing, or Hot Reload can invalidate assumptions.

## Modules and Build Files

- Keep runtime modules free of editor-only dependencies.
- Put editor-only functionality behind editor modules or the correct `WITH_EDITOR` boundary.
- Keep `Build.cs` dependencies narrow and intentional. Do not add broad dependencies to fix missing includes without checking ownership.
- Watch for circular plugin/module dependencies, especially between project code, UnrealMCP, editor tools, and GFur.
- Build-related reviews cover `.Build.cs`, `.Target.cs`, plugin descriptors, and module startup/shutdown side effects.

## Slate, UMG, and Editor UI

- Slate windows and widgets must have clear lifetime ownership. Avoid strong reference cycles between windows, callbacks, and owning modules.
- Use weak references for editor objects captured by Slate callbacks or delayed timers.
- Check editor shutdown, module unload, Live Coding, and window-close paths before approving Slate changes.
- UI timers and ticker handles must be removed or made harmless during shutdown.
- Keep user-facing Ieta Slate behavior scoped to the documented invocation and editor-startup paths.

## Async, Socket, and UnrealMCP Work

- Do not touch UObjects from background threads. Marshal UObject or editor work back to the game thread with the appropriate Unreal mechanism.
- Socket loops and background workers need cancellation, timeout, and shutdown behavior.
- Do not block the editor/game thread on network or long file operations.
- Report connection state transitions and failure causes clearly enough for the user to act.
- MCP commands that modify assets must validate inputs, constrain destructive paths, and report saved assets, failures, and residual risks.

## Verification Expectations

- Small project C++ changes usually need a targeted UnrealBuildTool build for `StylizedCubelessEditor Win64 Development`.
- Editor plugin, Slate, MCP, or startup behavior changes need an editor restart or at least a targeted editor smoke test when feasible.
- PIE behavior changes need a PIE smoke test and a short log check.
- MCP bridge changes need a bridge command test and clear reporting of the checked port/path.
- Build files or module boundary changes need a clean enough UBT build to prove dependency correctness.
- If verification is skipped, the final report must say exactly what was not run and why.

## Review Output

For `이에타 C++ 리뷰`, findings come first. Each finding should include severity, file/line reference, why it matters in Unreal, and the expected fix direction. Summaries and compliments stay secondary.
