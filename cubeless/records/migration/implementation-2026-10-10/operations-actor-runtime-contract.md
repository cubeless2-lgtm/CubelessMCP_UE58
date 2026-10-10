# Actor no-MCP runtime proof contract

This is a separate proof slice for the saved Actor M fixture. It does not modify the original fixture, D/L runtime proof sources, game C++, or engine source. Root owns preparation execution, editor processes, cooking and the game launch. The operations agent supplies source and offline checks only.

## Inputs and ownership

`actor_runtime_manifest.py` reruns the existing independent verifier on `actor-author-M.json` and `actor-reload-M3.json`, binding the exact original map fixture report. It also validates `runtime-fixture-B.json` and its saved marker blueprint hashes. The manifest plans exactly three copies: the saved Actor map, marker Actor Blueprint, and marker SaveGame Blueprint. No plugin directory is copied.

The only project is `cubeless/workspaces/UE58ActorRuntimeProof/CubelessActorRuntimeProof.uproject`. The manifest fixes a fresh `/Game/CubelessMCPTests/ActorRuntime_<uuid>/Map_ActorRuntimeProof`. Its `--prepare` branch may be run by root to create a previously absent workspace, Editor-only Python/EditorScriptingUtilities descriptor, explicit default map, Pak-only packaging configuration, and hash-checked copies. It refuses an existing workspace or report. Partial preparation is not automatically retried.

Five owned actors survive the original smoke: StaticMeshActor, PointLight, SpotLight, DirectionalLight, and an alias PointLight. This is four distinct native classes. The created CameraActor was deleted and must remain absent. The original object names, label change, transforms, and WorldSettings identity remain independent comparison inputs.

## Author and cold phases

`create_actor_runtime_fixture.py --manifest <manifest> --report <new-report>` checks exact project/config, disabled MCP/Toolset plugins, no PIE, no preexisting dirty packages, and original/copy hashes. Public APIs load the source map and inspect the five actors plus WorldSettings. A single existing marker Blueprint instance is spawned into the copied world, with per-stage attempt/returned-object evidence. One `EditorLoadingAndSavingUtils.save_map` writes a **new map**. No Blueprint compile or source-map save is requested in this author phase. Source and copied input hashes must remain unchanged. The author report deliberately leaves the dependency graph pending for a fresh process.

`validate_actor_runtime_fixture.py --manifest <manifest> --fixture-report <author> --report <new-report>` requires a different process and exact parent/config/new-map bytes. It uses native public Actor getters, not Cubeless tools. The saved-map path is compared by an exact source/new world prefix mapping, preserving every actor's subobject name; unrelated engine-generated actor observations remain raw and are not asserted to be stable across editor sessions. The five owned actors, WorldSettings, and added marker must match. A deleted Camera, duplicate name, marker impostor, nonfinite transform, or foreign persistent-level path fails verification.

The cold phase traverses the actual saved registry hard/soft package and current management graph. It requires new-map → marker Actor → marker SaveGame hard edges, all three expected packages, and exclusion of both old maps. It retains observed script terminals without requiring `/Script/Engine`: the actual cold-A graph included `/Script/NavigationSystem` but no Engine terminal despite independently observed Engine classes/parents. AssetRegistry package queries are not an exhaustive native-class/module enumeration. Package category queries include editor-only references when Game/EditorOnly flags are both false. Native script dependencies, dynamic string loads, generated AssetManager rules, and cooked membership remain separate scopes.

The two marker Blueprints are then compiled in memory through `BlueprintEditorLibrary.compile_blueprint`, whose installed implementation uses `EBlueprintCompileOptions::SkipSave`. Public graph errors and resulting status are checked. No save occurs; all six original/copied input files and the new map remain byte-identical.

## Source/API evidence and next gates

Installed UE58 sources verified for these calls include `Actor.h:1567–1568,1588–1589,1708–1710` (reflected location/rotation/scale), `EditorActorSubsystem.h:228` (spawn), `FileHelpers.h:64,75` (load/save new map), and `BlueprintEditorLibrary.cpp:594–602` (SkipSave compile). The scripts record engine source hashes at execution. Previously confirmed Blueprint parent lookup uses `BlueprintEditorLibrary.get_blueprint_parent_class`, avoiding the unexposed `Blueprint.parent_class` property.

Twenty-one offline contract tests cover parent evidence reconstruction, path/map tampering, exact actor class/transform identity, NaN/bool rejection, missing/duplicate actors, Camera resurrection, marker substitution/transform mismatch, exact map remapping, missing/forbidden dependency edges, and the observed absence of an Engine registry terminal. They do not prove UE execution or Shipping behavior. The failed cold-A report remains unchanged; a separate cold process must execute the corrected predicate.

After actual author and cold reports, root must cook/stage a Shipping Pak containing exactly the three required test packages, exclude both old maps and MCP/plugin payloads, then run it and verify a fresh marker using the unchanged B marker contract. The marker demonstrates execution of the map's Blueprint path; it does not prove rendered mesh/lighting quality or runtime transform measurement. Full Actor parity and whole-migration completion remain excluded.
