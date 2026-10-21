"""Fixed original live-AI Engine domain, independent of MCP/client/registry.

Only public Unreal APIs and stdlib. Each tick performs bounded fixed work and
returns; waits are observed world progress plus monotonic deadlines, never
sleep or a blocking future. No code/command strings are executed.
"""
import copy
import hashlib
import math
from pathlib import Path
import threading
import time
import unreal

DUMMY = "run_ai_dummy_patrol_chase_smoke"
BT = "run_ai_behavior_tree_execution_smoke"
ROLES = ("Floor", "Dummy", "Player", "Controller")
MAX_ACTORS = 512
FORBIDDEN = ("/script/cubeless", "/script/unrealmcp", "/engine/pythontypes", "/game/unrealmcp")


def file_pin(path):
    path = Path(path)
    if not path.exists():
        return {"exists": False, "bytes": 0, "sha256": ""}
    if not path.is_file() or path.stat().st_size > 128 * 1024 * 1024:
        raise RuntimeError("Selected source file is outside the bounded regular-file scope")
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return {"exists": True, "bytes": path.stat().st_size, "sha256": digest.hexdigest()}


def vec(value):
    return [float(value.x), float(value.y), float(value.z)]


def distance(left, right):
    return math.sqrt(sum((float(a) - float(b)) ** 2 for a, b in zip(vec(left), vec(right))))


def actor_row(actor):
    rotation = actor.get_actor_rotation()
    return {"path": actor.get_path_name(), "class": actor.get_class().get_path_name(),
        "label": actor.get_actor_label(), "location": vec(actor.get_actor_location()),
        "rotation": {"pitch": float(rotation.pitch), "yaw": float(rotation.yaw), "roll": float(rotation.roll)},
        "scale": vec(actor.get_actor_scale3d())}


def dirty():
    return sorted({obj.get_path_name() for obj in (
        list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()) +
        list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()))})


class Workflow:
    def __init__(self, name, config, run_id, observe, terminal, shutdown):
        if name not in (DUMMY, BT):
            raise ValueError("Only the two original fixed live workflows are supported")
        self.name, self.config, self.run_id = name, copy.deepcopy(config), run_id
        self.observe, self.terminal, self.shutdown_sink = observe, terminal, shutdown
        self.phase, self.started, self.phase_started = "preflight", time.monotonic(), time.monotonic()
        self.owner_thread = threading.get_ident()
        self.owned = {}; self.runtime = {}; self.loaded = []; self.handles = {}
        self.editor_world = self.game_world = None
        self.enter_requested = self.end_requested = self.unknown = False
        self.wait_started = self.game_time_started = None
        self.external = []; self.selection = []; self.dirty_before = []
        self.source_files = {}; self.audit_passed = False
        self.report = {"operation": name, "run_id": run_id, "success": False, "status": "BLOCKED",
            "config": copy.deepcopy(config), "steps": [], "summary": {}, "report_path": "",
            "workflow_finished": False, "outcome_unknown": False, "workflow_recovery_required": False,
            "mutation_attempted": False, "mutation_performed": False, "changes": [],
            "automatic_retry": False, "cleanup_attempted": False, "cleanup_outcome_unknown": False,
            "evidence_standard": {"world_mode": "SIE", "content_scope": "Owned actors in a disposable current test level",
                "serialized_mcp_dependency": "Engine/project asset references only; Editor adapter never serialized",
                "detection_contract": "distance <= sight_radius AND AIController.line_of_sight_to; not AIPerception",
                "movement_contract": "actual ticking displacement after pathfindingFalse requests; request enum alone is insufficient",
                "runtime_entrypoint": "AIController.run_behavior_tree" if name == BT else "AIController.move_to_location/actor",
                "blackboard_entrypoint": "AIController.use_blackboard plus BlackboardComponent key writes" if name == BT else "not_requested",
                "implicit_engine_work_measured": False, "client_disconnect_cancellation_verified": False,
                "global_gc_cleanup_equivalence_verified": False, "rendering_or_shipping_verified": False}}

    def start(self):
        self.handles["shutdown"] = unreal.register_python_shutdown_callback(self._shutdown)
        self.handles["ticker"] = unreal.register_ticker_callback(self.tick, delay=0.0)
        self.observe(self.report, "ticker_registered", returned=True)
        return self

    def _step(self, stage, body, mutation=False, *, coalesce=False):
        # A durable intent must succeed before dispatch. If return/checkpoint is
        # lost, this phase is never entered again and no subsequent mutation runs.
        closing = self.phase in ("end", "await_end", "cleanup", "final_sources")
        reused = coalesce and not mutation and self.report["steps"] and self.report["steps"][-1]["name"] == stage
        if not reused and len(self.report["steps"]) >= (128 if closing else 112):
            raise RuntimeError("Fixed evidence-step budget exhausted before next action")
        self.report["stage"] = stage
        if reused:
            row = self.report["steps"][-1]
            row["last_returned_result"] = row.get("result")
            row.update(status="pending", returned=False, iterations=row.get("iterations", 1) + 1)
        else:
            row = {"name": stage, "status": "pending", "mutation_attempted": mutation, "returned": False}
            self.report["steps"].append(row)
        if mutation:
            self.report["mutation_attempted"] = True
            self.report["changes"].append({"kind": stage, "attempted": True, "changed": None,
                "change_state_known": False, "outcome": "pending_return"})
        try:
            self.observe(self.report, stage, intent=mutation)
        except Exception:
            # A failed ownership checkpoint forbids even a subsequent cleanup
            # dispatch. The started journal cannot certify later world state.
            self.unknown = True
            raise
        try:
            value = body()
        except Exception:
            if mutation:
                self.unknown = True
            raise
        row.update(status="returned", returned=True, result=value)
        if mutation:
            self.report["changes"][-1].update(outcome="returned_fixed_world_action_effects_not_exhaustively_instrumented")
        try:
            self.observe(self.report, stage, returned=True)
        except Exception:
            # Engine returned, but durable ownership checkpoint failed. Stop;
            # do not repeat body or attempt an unjournaled cleanup.
            self.unknown = True
            raise
        return value

    def _actors(self):
        rows = list(self.actor_subsystem.get_all_level_actors())
        if len(rows) > MAX_ACTORS:
            raise RuntimeError("Editor actor inventory exceeds the fixed bound")
        return rows

    def _sources(self):
        if not hasattr(self, "source_specs"):
            self.source_specs = [(key, self.config[key]) for key in ("pawn_asset_path", "skeletal_mesh_path", "anim_blueprint_path",
                "behavior_tree_path", "blackboard_path") if self.config.get(key)] + [("floor_mesh", "/Engine/BasicShapes/Cube")]
            self.source_index = 0
        key, path = self.source_specs[self.source_index]
        asset = unreal.EditorAssetLibrary.load_asset(path)  # At most one explicit native load per tick.
        self.loaded.append(asset)
        setattr(self, "cube" if key == "floor_mesh" else key.removesuffix("_path"), asset)
        if key == "floor_mesh" and not isinstance(asset, unreal.StaticMesh):
            raise RuntimeError("Required native Engine Cube StaticMesh unavailable")
        # Resolve optional generated classes before source auditing and before any
        # actor allocation. A missing generated class is the original known
        # Character fallback, not a failed actor spawn to be retried.
        generated = None
        class_route = "not_requested"
        if key in ("pawn_asset_path", "anim_blueprint_path"):
            class_route = "asset_not_found" if not asset else "no_generated_class"
            if isinstance(asset, unreal.Blueprint):
                generated = unreal.EditorAssetLibrary.load_blueprint_class(path)
                class_route = "EditorAssetLibrary.load_blueprint_class" if generated else "no_generated_class"
                if generated:
                    self.loaded.append(generated)
            if key == "pawn_asset_path":
                if generated and not unreal.MathLibrary.class_is_child_of(generated, unreal.Pawn.static_class()):
                    raise RuntimeError("Requested generated class is not a Pawn; no fixture actor was allocated")
                self.pawn_class = generated
                self.pawn_class_route = class_route
            else:
                self.anim_class = generated
                self.anim_class_route = class_route
        self.source_index += 1
        return {"explicit_asset_load_receipt": {"role": key, "requested_path": path, "loaded": bool(asset),
            "actual_path": asset.get_path_name() if asset else "", "class": asset.get_class().get_path_name() if asset else "",
            "generated_class_route": class_route, "generated_class": generated.get_path_name() if generated else ""},
            "loads_complete": self.source_index == len(self.source_specs), "implicit_postload_effects_measured": False}

    def _source_audit(self):
        # Dependency lookup is one package per tick. File hashing reads at most
        # oneMiB per tick with an explicitly retained/closed file handle.
        if not hasattr(self, "audit_queue"):
            self.registry = unreal.AssetRegistryHelpers.get_asset_registry()
            self.audit_options = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True,
                include_hard_package_references=True, include_game_package_references=True,
                include_editor_only_package_references=True, include_searchable_names=False,
                include_soft_management_references=True, include_hard_management_references=True)
            self.audit_queue = sorted({obj.get_path_name().split(".", 1)[0] for obj in self.loaded if obj})
            self.audit_visited = set(); self.audit_edges = []; self.audit_file = None
        if self.audit_file is not None:
            job = self.audit_file; block = job["stream"].read(1024 * 1024)
            if block:
                job["hash"].update(block); job["read"] += len(block)
                return {"source_file": str(job["path"]), "hashed_bytes": job["read"], "complete": False}
            job["stream"].close(); stat = job["path"].stat()
            if stat.st_size != job["size"] or stat.st_mtime_ns != job["mtime_ns"] or job["read"] != job["size"]:
                raise RuntimeError("Source file changed while hashing")
            self.source_files[str(job["path"])] = {"exists": True, "bytes": job["size"], "sha256": job["hash"].hexdigest()}
            self.audit_file = None
            return {"source_file": str(job["path"]), "hashed_bytes": job["read"], "file_complete": True, "complete": False}
        if self.audit_queue:
            package = self.audit_queue.pop(0)
            if package in self.audit_visited:
                return {"package": package, "already_observed": True, "complete": False}
            if len(self.audit_visited) >= 128 or any(token in package.lower() for token in FORBIDDEN):
                raise RuntimeError("Source dependency graph bound or forbidden runtime reference")
            self.audit_visited.add(package)
            if package.startswith("/Script/"):
                return {"package": package, "native_script_package": True, "complete": False}
            if not package.startswith(("/Game/", "/Engine/")):
                raise RuntimeError("Source dependency outside Game/Engine/native Script scope")
            deps = self.registry.get_dependencies(package, self.audit_options)
            if deps is None:
                raise RuntimeError("AssetRegistry has no complete dependency row: " + package)
            if len(deps) > 256 or len(self.audit_edges) + len(deps) > 512 or len(self.audit_queue) + len(deps) > 1024:
                raise RuntimeError("Source dependency edges exceed the fixed bound")
            for item in sorted({str(item) for item in deps}):
                if any(token in item.lower() for token in FORBIDDEN):
                    raise RuntimeError("Source has a forbidden runtime dependency: " + item)
                self.audit_edges.append([package, item]); self.audit_queue.append(item)
            root = unreal.Paths.project_content_dir() if package.startswith("/Game/") else unreal.Paths.engine_content_dir()
            suffix = package.removeprefix("/Game/") if package.startswith("/Game/") else package.removeprefix("/Engine/")
            disk = Path(unreal.Paths.convert_relative_path_to_full(root)).resolve() / (suffix + ".uasset")
            if not disk.is_file():
                raise RuntimeError("Referenced source package is not persisted: " + package)
            stat = disk.stat()
            if stat.st_size > 128 * 1024 * 1024:
                raise RuntimeError("Source regular file exceeds bounded hash scope")
            self.audit_file = {"path": disk, "stream": disk.open("rb"), "hash": hashlib.sha256(),
                "size": stat.st_size, "mtime_ns": stat.st_mtime_ns, "read": 0}
            return {"package": package, "dependency_count": len(deps), "source_file": str(disk), "complete": False}
        if not all(file_pin(path) == pin for path, pin in self.config_files.items()):
            raise RuntimeError("Config changed during explicit source loading before fixture mutation")
        if self.name == BT:
            tree, board = getattr(self, "behavior_tree", None), getattr(self, "blackboard", None)
            if not isinstance(tree, unreal.BehaviorTree) or not isinstance(board, unreal.BlackboardData):
                raise RuntimeError("Exact saved native BehaviorTree and Blackboard are required before fixture mutation")
            if tree.get_editor_property("root_node") is None or tree.get_editor_property("blackboard_asset") != board:
                raise RuntimeError("Blank/wrong-Blackboard tree is not a live task execution fixture")
        self.audit_passed = True
        self.report["dependency_audit"] = {"passed": True, "scanned_packages": sorted(self.audit_visited), "edges": self.audit_edges,
            "scope": "Current AssetRegistry hard/soft/editor/game/management package graph; no private RAM cache or cook equivalence",
            "source_files": copy.deepcopy(self.source_files), "missing_rows": [], "violations": []}
        return {"passed": True, "complete": True, "scanned_package_count": len(self.audit_visited), "source_file_count": len(self.source_files)}

    def _preflight(self):
        self.level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        self.worlds = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        self.actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        if not self.level or not self.worlds or not self.actor_subsystem or self.level.is_in_play_in_editor() or self.worlds.get_game_world():
            raise RuntimeError("Editor subsystems/current idle session cannot be certified")
        self.editor_world = self.worlds.get_editor_world()
        package = self.editor_world.get_path_name().split(".", 1)[0] if self.editor_world else ""
        owned_parts = package.removeprefix("/Game/_MCP_Temp/").split("/")
        current_owned_test = (package.startswith("/Game/_MCP_Temp/") and len(owned_parts) >= 4
            and owned_parts[1] == "CubelessTests"
            and all(part and all(char.isalnum() or char == "_" for char in part) for part in owned_parts))
        if not package.startswith(("/Temp/", "/Game/CubelessMCPTests/")) and not current_owned_test:
            raise RuntimeError("Live scope requires an already-open disposable /Temp/, /Game/CubelessMCPTests/ or /Game/_MCP_Temp/<feature>/CubelessTests/<run>/<level> level")
        self.map_package = package
        self.config_files = {str(Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()
            / "Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini"): file_pin(
                Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()
                / "Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini")}
        labels = {self.config["label_prefix"] + "_" + role for role in ROLES}
        self.existing = [obj for obj in self._actors() if obj.get_actor_label() in labels]
        if self.existing and not self.config["cleanup_existing"]:
            raise RuntimeError("Fixture labels exist and cleanup_existing=False; no fixture mutation")
        if any(any(token in obj.get_class().get_path_name().lower() for token in FORBIDDEN) for obj in self.existing):
            raise RuntimeError("Existing matching actors reference unsupported plugin/Python runtime classes")
        self.external = sorted((actor_row(obj) for obj in self._actors() if obj not in self.existing), key=lambda row: row["path"])
        self.selection = [obj for obj in self.actor_subsystem.get_selected_level_actors() if obj not in self.existing]
        self.dirty_before = dirty()
        if any(path != package for path in self.dirty_before):
            raise RuntimeError("Unrelated dirty packages exist before the live workflow")
        return {"editor_world": self.editor_world.get_path_name(), "map_package": package,
            "external_actors": self.external, "dirty_packages": self.dirty_before,
            "config_file_pins": copy.deepcopy(self.config_files),
            "config_scope": "Selected persisted EditorPerProjectUserSettings file only; not a complete private Editor settings/cache inventory",
            "matching_existing_actor_paths": [obj.get_path_name() for obj in self.existing]}

    def _destroy_existing(self):
        rows = []
        for obj in self.existing:
            path = obj.get_path_name()
            if not self.actor_subsystem.destroy_actor(obj):
                raise RuntimeError("Existing exact-label actor destruction returned false: " + path)
            rows.append(path)
        if any(obj.get_actor_label() in {self.config["label_prefix"] + "_" + role for role in ROLES} for obj in self._actors()):
            raise RuntimeError("Existing fixture label remains after exact destruction")
        return {"deleted": rows, "global_gc_dispatched": False}

    def _spawn(self):
        selected = getattr(self, "pawn_class", None)
        pawn_class = selected or unreal.Character.static_class()
        route = getattr(self, "pawn_class_route", "not_requested")
        preparation = {"pawn_class_route": route, "pawn_class_used": pawn_class.get_path_name(),
            "pawn_class_fallback": not bool(selected), "pawn_spawn_fallback": False,
            "spawn_automatic_retry": False, "spawn_receipts": []}
        self.report["fixture_preparation"] = preparation
        specs = (("Floor", unreal.StaticMeshActor.static_class(), [180, 0, -55]),
            ("Dummy", pawn_class, [-350, 0, 100]), ("Player", unreal.Character.static_class(), [1500, 0, 100]),
            ("Controller", unreal.AIController.static_class(), [-350, 0, 100]))
        for role, cls, position in specs:
            receipt = {"role": role, "class": cls.get_path_name(), "attempted": True, "returned": False}
            preparation["spawn_receipts"].append(receipt)
            try:
                actor = self.actor_subsystem.spawn_actor_from_class(cls, unreal.Vector(*position), unreal.Rotator(), transient=False)
            except Exception as error:
                receipt.update(error=str(error)[:700], outcome_unknown=True, retry_performed=False)
                raise
            receipt.update(returned=True, returned_actor=bool(actor), retry_performed=False)
            if not actor:
                # A returned null is distinguished from an exception, but does
                # not prove that construction callbacks left no other changes.
                receipt.update(outcome_unknown=True, returned_null=True)
                raise RuntimeError("Owned spawn returned None; allocated/callback prefix retained without fallback retry: " + role)
            self.owned[role] = actor
            self.report.setdefault("owned_editor_actor_paths", {})[role] = actor.get_path_name()
            receipt.update(path=actor.get_path_name(), outcome_unknown=False)
            self.report["mutation_performed"] = True  # Actual returned new actor, not inferred from a move request.
            actor.set_actor_label(self.config["label_prefix"] + "_" + role)
        floor = self.owned["Floor"]
        component = floor.get_component_by_class(unreal.StaticMeshComponent)
        if not component or not component.set_static_mesh(self.cube):
            raise RuntimeError("Owned floor mesh assignment failed")
        component.set_collision_profile_name("BlockAll")
        floor.set_actor_scale3d(unreal.Vector(12, 7, 0.1))
        visual = self._apply_visuals()
        return {"owned_actors": {role: actor_row(actor) for role, actor in self.owned.items()},
            **preparation, "dummy_visuals": visual, "transient_spawn": False,
            "explicit_engine_cube_load": True, "mcp_dependency_audit_pass": self.audit_passed}

    def _apply_visuals(self):
        visual = {"skeletal_mesh_path": self.config.get("skeletal_mesh_path", ""),
            "anim_blueprint_path": self.config.get("anim_blueprint_path", ""),
            "mesh_applied": False, "anim_class_applied": False, "warnings": [], "writes": [],
            "anim_class_route": getattr(self, "anim_class_route", "not_requested"),
            "setter_fallback_retry_performed": False,
            "observation_scope": "Current owned component identity and selected asset/class properties; not complete SDK purity"}
        self.report["dummy_visuals"] = visual
        actor = self.owned["Dummy"]
        mesh = actor.get_component_by_class(unreal.SkeletalMeshComponent)
        if not mesh:
            visual["warnings"].append("No SkeletalMeshComponent on actor")
            return visual

        def current():
            if (not unreal.SystemLibrary.is_valid(actor) or not unreal.SystemLibrary.is_valid(mesh)
                    or mesh.get_owner() != actor or actor.get_component_by_class(unreal.SkeletalMeshComponent) != mesh):
                self.unknown = True
                raise RuntimeError("Owned visual actor/component changed; no further visual writer or fallback")

        # Only two fixed public writers. Omitted/missing/wrong-type optional
        # visuals are warnings; a setter exception retains unknown effects and
        # stops, rather than silently running the old property-write fallback.
        source_mesh = getattr(self, "skeletal_mesh", None)
        source_anim = getattr(self, "anim_class", None)
        if visual["skeletal_mesh_path"] and source_mesh is None:
            visual["warnings"].append("Skeletal mesh asset not found")
        if source_mesh is not None and not isinstance(source_mesh, unreal.SkeletalMesh):
            visual["warnings"].append("Visual setup failed: requested asset is not a SkeletalMesh")
            source_mesh = None
        if visual["anim_blueprint_path"] and source_anim is None:
            visual["warnings"].append("Anim Blueprint generated class not found")
        if source_anim is not None and not unreal.MathLibrary.class_is_child_of(source_anim, unreal.AnimInstance.static_class()):
            visual["warnings"].append("Anim class apply failed: generated class is not an AnimInstance")
            source_anim = None
        for kind, wanted in (("mesh", source_mesh), ("anim_class", source_anim)):
            if wanted is None:
                continue
            current()
            before = mesh.get_skinned_asset() if kind == "mesh" else mesh.get_editor_property("anim_class")
            current()
            row = {"kind": kind, "requested": wanted.get_path_name(),
                "before": before.get_path_name() if before else "", "attempted": True, "returned": False,
                "automatic_retry": False}
            visual["writes"].append(row)
            try:
                if kind == "mesh":
                    mesh.set_skinned_asset_and_update(wanted)
                else:
                    mesh.set_anim_instance_class(wanted)
            except Exception as error:
                row.update(outcome_unknown=True, error=str(error)[:700])
                visual["warnings"].append("Visual setup failed: " + str(error)[:700])
                self.unknown = True
                raise RuntimeError("Visual setter raised after dispatch; no property fallback/retry: " + kind) from error
            row["returned"] = True
            current()
            actual = mesh.get_skinned_asset() if kind == "mesh" else mesh.get_editor_property("anim_class")
            current()
            applied = actual == wanted
            row.update(actual=actual.get_path_name() if actual else "", selected_property_matches=applied,
                outcome_unknown=False, other_SDK_effects_fully_observed=False)
            visual["mesh_applied" if kind == "mesh" else "anim_class_applied"] = applied
            if not applied:
                visual["warnings"].append("Visual setter returned without the requested " + kind + "; actual property retained, no retry")
        return visual

    def _runtime_actors(self):
        world = self.worlds.get_game_world()
        if not self.level.is_in_play_in_editor() or not world:
            return False
        actors = list(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor))
        if len(actors) > MAX_ACTORS:
            raise RuntimeError("Play-world actor inventory exceeds fixed bound")
        found = {}
        for role in ROLES:
            matched = [obj for obj in actors if obj.get_actor_label() == self.config["label_prefix"] + "_" + role]
            if len(matched) != 1:
                return False
            found[role] = matched[0]
        if not isinstance(found["Dummy"], unreal.Pawn) or not isinstance(found["Player"], unreal.Character) or not isinstance(found["Controller"], unreal.AIController):
            raise RuntimeError("Actual play-world native pawn/controller typing differs")
        self.game_world, self.runtime = world, found
        return True

    def _wait(self, seconds, next_phase):
        self.wait_started = time.monotonic()
        self.game_time_started = float(unreal.GameplayStatics.get_time_seconds(self.game_world))
        self.wait_seconds, self.next_phase = seconds, next_phase
        self.phase = "wait_world"

    def _patrol(self):
        dummy, controller = self.runtime["Dummy"], self.runtime["Controller"]
        controller.possess(dummy)
        start = dummy.get_actor_location(); target = unreal.Vector(250, 0, 100)
        request = controller.move_to_location(target, acceptance_radius=self.config["acceptance_radius"],
            stop_on_overlap=True, use_pathfinding=False, project_destination_to_navigation=False,
            can_strafe=False, allow_partial_path=True)
        return {"dummy_location_before": vec(start), "patrol_target": vec(target), "distance_before": distance(start, target),
            "move_request": str(request), "move_status": str(controller.get_move_status()), "use_pathfinding": False}

    def _detect(self):
        player, dummy, controller = self.runtime["Player"], self.runtime["Dummy"], self.runtime["Controller"]
        player.set_actor_location(unreal.Vector(650, 0, 100), False, True)
        before = distance(dummy.get_actor_location(), player.get_actor_location())
        los = bool(controller.line_of_sight_to(player)); detected = before <= self.config["sight_radius"] and los
        request = controller.move_to_actor(player, acceptance_radius=self.config["acceptance_radius"], stop_on_overlap=True,
            use_pathfinding=False, can_strafe=False, allow_partial_path=True) if detected else None
        return {"distance_to_player_before_chase": before, "line_of_sight": los, "detected": detected,
            "move_request": str(request), "move_status": str(controller.get_move_status())}

    def _bt_start(self):
        tree, board = getattr(self, "behavior_tree", None), getattr(self, "blackboard", None)
        if not isinstance(tree, unreal.BehaviorTree) or not isinstance(board, unreal.BlackboardData):
            raise RuntimeError("Exact saved native BehaviorTree and Blackboard are required")
        if tree.get_editor_property("root_node") is None or tree.get_editor_property("blackboard_asset") != board:
            raise RuntimeError("Blank/wrong-Blackboard tree is not a live task execution fixture")
        dummy, player, controller = self.runtime["Dummy"], self.runtime["Player"], self.runtime["Controller"]
        player.set_actor_location(unreal.Vector(650, 0, 100), False, True); controller.possess(dummy)
        before = distance(dummy.get_actor_location(), player.get_actor_location())
        board_component = controller.use_blackboard(board)  # Source-proven component-or-None bool/out packing.
        if not board_component:
            raise RuntimeError("UseBlackboard did not return its actual runtime component")
        board_component.set_value_as_vector(self.config["patrol_location_key"], unreal.Vector(250, 0, 100))
        board_component.set_value_as_object(self.config["target_actor_key"], player)
        ran = bool(controller.run_behavior_tree(tree))
        board_component = controller.get_editor_property("blackboard") or board_component
        board_component.set_value_as_vector(self.config["patrol_location_key"], unreal.Vector(250, 0, 100))
        board_component.set_value_as_object(self.config["target_actor_key"], player)
        self.board_component = board_component
        return {"run_behavior_tree_result": ran, "blackboard_component": board_component.get_path_name(),
            "distance_to_player_before": before, "blackboard_values_set": self._keys_set()}

    def _keys_set(self):
        board = self.board_component
        return board.get_value_as_object(self.config["target_actor_key"]) == self.runtime["Player"] and distance(
            board.get_value_as_vector(self.config["patrol_location_key"]), unreal.Vector(250, 0, 100)) <= 1.0

    def _sample(self):
        dummy, player, controller = self.runtime["Dummy"], self.runtime["Player"], self.runtime["Controller"]
        result = {"dummy_location_after": vec(dummy.get_actor_location()), "player_location": vec(player.get_actor_location()),
            "distance_to_player_after": distance(dummy.get_actor_location(), player.get_actor_location()),
            "distance_to_patrol_after": distance(dummy.get_actor_location(), unreal.Vector(250, 0, 100)),
            "move_status": str(controller.get_move_status())}
        if self.name == BT:
            brain = controller.get_editor_property("brain_component")
            result.update(blackboard_values_still_set=self._keys_set(), brain_component=brain.get_path_name() if brain else "",
                brain_running=bool(brain and brain.is_running()), brain_active=bool(brain and brain.is_active()),
                brain_tick_enabled=bool(brain and brain.is_component_tick_enabled()))
        return result

    def _cleanup(self):
        self.report["cleanup_attempted"] = True
        destroyed = []
        if self.level.is_in_play_in_editor() or self.worlds.get_game_world():
            raise RuntimeError("Owned session is not independently ended; actor destruction refused")
        if self.config["cleanup_after"]:
            for role, actor in self.owned.items():
                path = actor.get_path_name()
                if not self.actor_subsystem.destroy_actor(actor):
                    raise RuntimeError("Owned actor cleanup returned false: " + path)
                destroyed.append(path)
            if any(obj in self.owned.values() for obj in self._actors()):
                raise RuntimeError("An owned fixture actor remains after cleanup")
        self.actor_subsystem.set_selected_level_actors(self.selection)
        actual = sorted((actor_row(obj) for obj in self._actors() if obj not in self.owned.values()), key=lambda row: row["path"])
        if actual != self.external or any(path != self.map_package for path in dirty()):
            raise RuntimeError("External Editor actors or unrelated dirty packages changed")
        return {"destroyed_owned_paths": destroyed, "cleanup_after_requested": self.config["cleanup_after"],
            "external_actor_state_preserved": True, "selection_restored": True, "dirty_packages_after": dirty(),
            "original_map_dirty_restored": False, "global_gc_dispatched": False}

    def _final_sources(self):
        if not hasattr(self, "final_files"):
            self.final_files = list({**self.source_files, **self.config_files}.items()); self.final_file = None
        if self.final_file is not None:
            job = self.final_file; block = job["stream"].read(1024 * 1024)
            if block:
                job["hash"].update(block); job["read"] += len(block)
                return {"path": str(job["path"]), "hashed_bytes": job["read"], "complete": False}
            job["stream"].close()
            pin = {"exists": True, "bytes": job["read"], "sha256": job["hash"].hexdigest()}
            if pin != job["expected"]:
                raise RuntimeError("Saved source asset/config content changed during the live workflow")
            self.final_file = None
            return {"path": str(job["path"]), "preserved": True, "complete": False}
        if self.final_files:
            name, expected = self.final_files.pop(0); path = Path(name)
            if not expected["exists"]:
                if path.exists():
                    raise RuntimeError("Previously absent config file was created")
                return {"path": name, "preserved_absence": True, "complete": False}
            if not path.is_file() or path.stat().st_size != expected["bytes"]:
                raise RuntimeError("Saved source file disappeared or resized")
            self.final_file = {"path": path, "stream": path.open("rb"), "hash": hashlib.sha256(), "read": 0, "expected": expected}
            return {"path": name, "complete": False}
        self.report["saved_source_and_config_preserved"] = True
        return {"saved_source_and_config_preserved": True, "complete": True}

    def _close_files(self):
        job = getattr(self, "audit_file", None)
        if job is not None:
            job["stream"].close(); self.audit_file = None
        job = getattr(self, "final_file", None)
        if job is not None:
            job["stream"].close(); self.final_file = None

    def _close_handles(self):
        errors = []
        try:
            self._close_files()
        except Exception as error:
            errors.append("file_handle_close: " + str(error)[:500])
        for key, remove in (("ticker", unreal.unregister_ticker_callback),
                ("shutdown", unreal.unregister_python_shutdown_callback)):
            handle = self.handles.pop(key, None)
            if handle is not None:
                try:
                    remove(handle)
                except Exception as error:
                    errors.append(key + " callback removal: " + str(error)[:500])
        if errors:
            self.report.update(success=False, status="BLOCKED", outcome_unknown=True,
                workflow_recovery_required=True, error_code="ai_live_callback_cleanup_unknown",
                reason="; ".join(errors), callback_cleanup_errors=errors)
        return not errors

    def _complete(self):
        self.report["workflow_finished"] = True
        self.report["elapsed_seconds"] = round(time.monotonic() - self.started, 6)
        self.phase = "terminal"
        self._close_handles()
        self.report.update(mutation_state_unknown=bool(self.report.get("outcome_unknown")),
            read_command_outcome_unknown=False,
            do_not_retry=bool(self.report.get("outcome_unknown") or self.report.get("workflow_recovery_required")))
        self.terminal(self.report)

    def abandon_start(self):
        # Callback registration/checkpoint failed before any tick can mutate a
        # world. Remove only these registered callbacks; never perform cleanup.
        self.phase = "terminal"
        self._close_handles()

    def _shutdown(self):
        # No late actor/session/result completion calls. PyCore removes its
        # ticker callbacks separately; preserve only last durable known prefix.
        self.unknown = True
        self.phase = "terminal"
        try:
            self._close_files()
        finally:
            self.shutdown_sink(self.report)

    def tick(self, delta_seconds):
        if self.phase == "terminal":
            return False
        try:
            if threading.get_ident() != self.owner_thread:
                self.unknown = True
                raise RuntimeError("Ticker left the original Editor tool-call thread")
            now = time.monotonic()
            if now - self.started > 180 and self.phase not in ("end", "await_end", "cleanup", "final_sources"):
                raise RuntimeError("Fixed live workflow deadline reached")
            phase = self.phase  # No mutation-bearing phase is retried on exceptions.
            if phase == "preflight":
                self._step("preflight", self._preflight); self.phase = "sources"
            elif phase == "sources":
                loaded = self._step("load_requested_source", self._sources, True)
                if loaded["loads_complete"]:
                    self.phase = "source_audit"
            elif phase == "source_audit":
                checked = self._step("source_dependency_and_disk_audit", self._source_audit, coalesce=True)
                if checked["complete"]:
                    self.phase = "existing_cleanup"
            elif phase == "existing_cleanup":
                if self.existing:
                    self._step("cleanup_existing_labels", self._destroy_existing, True)
                self.phase = "spawn"
            elif phase == "spawn":
                self._step("setup_editor_fixture", self._spawn, True); self.phase = "enter"
            elif phase == "enter":
                self._step("start_sie", lambda: self.level.editor_play_simulate(), True)
                self.enter_requested = True; self.phase_started = now; self.phase = "await_sie"
            elif phase == "await_sie":
                if self._runtime_actors():
                    self._step("verified_owned_game_world", lambda: {"world": self.game_world.get_path_name(),
                        "actors": {role: actor_row(obj) for role, obj in self.runtime.items()}})
                    self._wait(self.config["setup_wait_seconds"], "patrol" if self.name == DUMMY else "bt_start")
                elif now - self.phase_started > 30:
                    raise RuntimeError("SIE request did not produce an observed owned ticking fixture")
            elif phase == "wait_world":
                game_elapsed = float(unreal.GameplayStatics.get_time_seconds(self.game_world)) - self.game_time_started
                if now - self.wait_started >= self.wait_seconds and game_elapsed > 0:
                    self._step("world_progress_wait", lambda: {"monotonic_seconds": now - self.wait_started,
                        "game_seconds": game_elapsed, "required_wait_seconds": self.wait_seconds})
                    self.phase = self.next_phase
                elif now - self.wait_started > self.wait_seconds + 30:
                    raise RuntimeError("Requested wait expired without actual game-world progress")
            elif phase == "patrol":
                self.patrol_request = self._step("patrol_command", self._patrol, True)
                self._wait(self.config["patrol_wait_seconds"], "patrol_sample")
            elif phase == "patrol_sample":
                self.patrol_sample = self._step("patrol_sample", self._sample)
                self.phase = "detect"
            elif phase == "detect":
                self.detected = self._step("detect_and_chase_command", self._detect, True)
                self._wait(self.config["chase_wait_seconds"], "chase_sample")
            elif phase == "chase_sample":
                sample = self._step("chase_sample", self._sample)
                change = self.detected["distance_to_player_before_chase"] - sample["distance_to_player_after"]
                summary = {"patrol_reached": self.patrol_sample["distance_to_patrol_after"] <= self.config["patrol_pass_distance"],
                    "player_detected": self.detected["detected"], "chase_distance_delta": change,
                    "chase_followed": change >= self.config["required_chase_delta"], "mcp_dependency_audit_pass": self.audit_passed}
                self.report["summary"] = summary
                self.criteria_pass = summary["patrol_reached"] and summary["player_detected"] and summary["chase_followed"] and summary["mcp_dependency_audit_pass"]
                self.phase = "end"
            elif phase == "bt_start":
                self.bt_command = self._step("bt_execution_command", self._bt_start, True)
                self._wait(self.config["bt_wait_seconds"], "bt_sample")
            elif phase == "bt_sample":
                sample = self._step("bt_execution_sample", self._sample)
                change = self.bt_command["distance_to_player_before"] - sample["distance_to_player_after"]
                summary = {"run_behavior_tree": self.bt_command["run_behavior_tree_result"],
                    "blackboard_values_set": self.bt_command["blackboard_values_set"] and sample["blackboard_values_still_set"],
                    "bt_movement_delta": change, "bt_movement_pass": sample["distance_to_player_after"] <= self.config["target_pass_distance"] or change >= self.config["required_bt_chase_delta"],
                    "mcp_dependency_audit_pass": self.audit_passed}
                self.report["summary"] = summary
                self.criteria_pass = summary["run_behavior_tree"] and summary["blackboard_values_set"] and summary["bt_movement_pass"] and summary["mcp_dependency_audit_pass"]
                self.phase = "end"
            elif phase == "end":
                if self.enter_requested and not self.end_requested:
                    self._step("end_sie", lambda: self.level.editor_request_end_play(), True); self.end_requested = True
                self.phase_started = now; self.phase = "await_end"
            elif phase == "await_end":
                if not self.level.is_in_play_in_editor() and not self.worlds.get_game_world():
                    self._step("verify_sie_ended", lambda: {"session_active": False, "game_world": None})
                    self.runtime.clear(); self.game_world = None
                    self.phase = "cleanup"
                elif now - self.phase_started > 30:
                    self.unknown = True
                    raise RuntimeError("EndPlay request remains unverified; actor cleanup suppressed")
            elif phase == "cleanup":
                self._step("cleanup_owned_fixture", self._cleanup, True)
                self.report["summary"]["lifecycle_cleanup_pass"] = True
                self.phase = "final_sources"
            elif phase == "final_sources":
                checked = self._step("saved_source_file_fence", self._final_sources, coalesce=True)
                if not checked["complete"]:
                    return True
                self.report.update(success=bool(getattr(self, "criteria_pass", False)) and not self.report.get("reason"),
                    status="PASS" if getattr(self, "criteria_pass", False) and not self.report.get("reason") else "FAIL")
                self._complete(); return False
            else:
                raise RuntimeError("Unknown fixed phase")
            return True
        except Exception as error:
            self.report.update(success=False, status="BLOCKED", reason=str(error)[:1000],
                error_code="ai_live_phase_failed", failed_stage=self.phase)
            if self.unknown or self.phase in ("end", "await_end", "cleanup", "final_sources"):
                self.report.update(outcome_unknown=True, workflow_recovery_required=True, mutation_performed=None,
                    cleanup_skipped_due_to_sie_end_failure=self.phase in ("end", "await_end"))
                try:
                    self._complete()
                except Exception:
                    self.phase = "terminal"
                return False
            # A known criteria/preflight/read failure can cooperatively end the
            # owned session once, then verify before any actor cleanup. Failed
            # mutating API returns/exceptions always take the unknown path above.
            self.criteria_pass = False
            if self.enter_requested:
                self.phase = "end"; return True
            if self.owned:
                self.phase = "cleanup"; return True
            try:
                self._complete()
            except Exception:
                self.phase = "terminal"
            return False
