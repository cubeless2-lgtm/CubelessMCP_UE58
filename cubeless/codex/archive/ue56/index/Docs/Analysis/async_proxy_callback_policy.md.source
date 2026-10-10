# Async Proxy Callback Policy

This policy defines the current UnrealMCP stance for read-only external project
intake when C++ async Blueprint patterns are found.

## Current Scope

- Inventory async proxy classes derived from `UBlueprintAsyncActionBase`.
- Inventory cancellable async proxy classes derived from `UCancellableAsyncAction`.
- Inventory Gameplay Ability tasks derived from `UAbilityTask`.
- Inventory custom async K2 nodes derived from `UK2Node_AsyncAction`.
- Record `BlueprintAssignable` callback delegates, Blueprint internal factory
  functions, `Activate()` methods, delegate `Broadcast()` calls, and cleanup or
  cancellation signals.

## Authoring Rule

Async proxy conversion remains inventory-only until UnrealMCP can explicitly
model callback exec pins, delegate payload pins, factory invocation, activation
ordering, cancellation, cleanup, and object lifetime.

Gameplay Ability tasks and custom K2 async nodes require native or domain-
specific policy before Blueprint graph conversion. They should not be treated as
ordinary function call nodes.

## Safe Now

- Static intake and reporting.
- Ranking async classes by callback topology risk.
- Using the inventory to choose the next UnrealMCP reinforcement target.

## Unsafe Without Reinforcement

- Recreating async action nodes as ordinary Blueprint function calls.
- Generating callback exec pins without matching delegate payload signatures.
- Lowering AbilityTask lifecycle or prediction flow into generic Blueprint graph
  nodes.
- Recreating custom K2 async expansion behavior without native policy.

