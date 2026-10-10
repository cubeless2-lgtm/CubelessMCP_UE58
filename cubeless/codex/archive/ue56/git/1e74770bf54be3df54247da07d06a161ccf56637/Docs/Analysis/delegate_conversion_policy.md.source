# Delegate Conversion Policy

This policy is used by the read-only external project analyzers before any C++ to Blueprint authoring attempt.

## Safe Now

- Blueprint Event Dispatchers can be declared, called, bound, assigned, unbound, and cleared through UnrealMCP.
- Signature-compatible custom events can be generated and connected to Event Dispatcher lifecycle nodes.
- Dynamic delegate sites may be considered BP candidates only when the target is equivalent to a BlueprintAssignable Event Dispatcher and cleanup topology is preserved.

## Requires Classification Before BP Authoring

- `AddDynamic` on arbitrary native-owned delegates: verify the delegate owner, target object lifetime, and matching cleanup path.
- `RemoveDynamic`, `RemoveAll`, `Remove`, and `Clear`: pair cleanup with the corresponding bind site before authoring.
- Async action and AbilityTask callback delegates: route to async/proxy policy, not ordinary Event Dispatcher authoring.

## Native Or Wrapper Required

- `AddUObject`, `AddRaw`, `AddSP`, `AddStatic`, and `AddLambda` are native lifecycle sites by default.
- Engine/editor/Slate lifecycle delegates such as `FWorldDelegates`, `FCoreDelegates`, `FEditorDelegates`, and `FSlateApplication` remain native unless a reviewed wrapper API owns startup and shutdown.
- Delegate handle storage and explicit handle removal must preserve shutdown order and owner lifetime.

## Analyzer Buckets

- `bp_event_dispatcher_candidate`: possible Blueprint Event Dispatcher lowering after ownership and cleanup checks.
- `requires_explicit_unbind_policy`: dynamic binding was found without obvious cleanup in the same file.
- `requires_wrapper_api`: native UObject delegate binding needs a stable wrapper or equivalent BP-safe owner.
- `native_required`: raw/shared/lambda/static or engine lifecycle binding should remain native.
- `async_or_ability_task`: callback topology belongs to async proxy, custom K2, or GAS AbilityTask policy.
- `inventory_cleanup`: cleanup was observed and must be paired before conversion.
