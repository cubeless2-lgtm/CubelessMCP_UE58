"""Python port of UE56 solver-intent policy; one UE58 scalar-text batch, no RPC.

The legacy result retains adapter='unrealmcp_native' as a historical contract
identifier. It is not evidence of a Native call in this implementation. Current
execution identity is supplied separately by the operations layer. The text-only
helper preserves FJsonValue/FString/FChar locale and double-format semantics; it
does not run the validator or query, load or modify any Engine asset.
"""
import hashlib
import json
import math

VALIDATOR = "python_pure_solver_intent_validation"
METHOD = "validate_controlrig_solver_intents"
MAX_REQUEST_UTF16_UNITS = 1 << 20
MAX_JSON_DEPTH = 64
MAX_JSON_VALUE_SLOTS = 65536
MAX_TEXT_RESULT_UTF16_UNITS = 16 << 20
TEXT_LIBRARY = "CubelessSolverTextSemanticsLibrary"
TEXT_METHOD = "evaluate_solver_text_semantics"
_ASCII_LOWER = str.maketrans("ABCDEFGHIJKLMNOPQRSTUVWXYZ", "abcdefghijklmnopqrstuvwxyz")
_REQUIRED_FIELDS = ("name", "side", "root_bone", "mid_bone", "end_bone",
    "target_control", "pole_control", "weight_curve")
_FORBIDDEN_UNIT_FRAGMENTS = ("FRigUnit_SetTransform", "FRigUnit_SetBoneTransform",
    "FRigUnit_SetControlTransform", "FRigUnit_TwoBoneIK", "FRigUnit_FABRIK",
    "FRigUnit_MultiFABRIK", "FRigUnit_SpringIK", "FRigUnit_Aim",
    "FRigUnit_ParentConstraint", "FRigUnit_Spline")
_LEGACY_MCP_TOKENS = ("/script/unrealmcp", "/unrealmcp", "unrealmcp", "mcpunreal",
    "mcp_unreal", "/script/cubelesscontrolrigtools", "cubelesscontrolrigtools",
    "cubelesscontrolrigtoolseditor")
_CURRENT_MCP_TOKENS = ("/script/cubelessmcp_ue58", "/cubelessmcp_ue58", "cubelessmcp_ue58")


class InputError(ValueError):
    def __init__(self, message, code="controlrig_solver_intent_input_invalid", field_name="request"):
        super().__init__(message)
        self.code = code
        self.field_name = field_name


def binding_status():
    try:
        ready = callable(_binding())
    except InputError:
        ready = False
    return {METHOD: ready}


def _utf16_units(text):
    try:
        return len(text.encode("utf-16-le")) // 2
    except UnicodeEncodeError as error:
        raise InputError("request contains an unpaired Unicode surrogate") from error


def _bounded_text(text):
    if "\x00" in text:
        raise InputError("request must not contain an embedded NUL")
    if _utf16_units(text) > MAX_REQUEST_UTF16_UNITS:
        raise InputError("request exceeds the 1Mi UTF16-unit input boundary")


def _bounded_encoded_json(text):
    _bounded_text(text)
    depth = 0
    quoted = False
    escaped = False
    for char in text:
        if quoted:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                quoted = False
        elif char == '"':
            quoted = True
        elif char in "[{":
            depth += 1
            if depth > MAX_JSON_DEPTH:
                raise InputError("request exceeds the JSON depth64 boundary")
        elif char in "]}":
            depth -= 1
    # Syntax errors and duplicate-key last-value behavior remain json.loads rules.


def _reject_constant(value):
    raise InputError("request must contain finite JSON numbers, not " + value)


def _json_copy(value):
    """Bound arbitrary Python input before serializing; never call user hooks."""
    active = set()
    slots = [0]

    def visit(item, depth, field):
        slots[0] += 1
        if slots[0] > MAX_JSON_VALUE_SLOTS:
            raise InputError("request exceeds the 65536 JSON-value-slot boundary", field_name=field)
        kind = type(item)
        if kind is str:
            _bounded_text(item)
            return item
        if item is None or kind is bool:
            return item
        if kind in (int, float):
            try:
                finite = math.isfinite(float(item))
            except (OverflowError, ValueError):
                finite = False
            if not finite:
                raise InputError("request must contain finite double-compatible JSON numbers", field_name=field)
            return item
        if kind not in (dict, list):
            raise InputError("request must use JSON object/array/string/number/boolean/null values", field_name=field)
        if depth > MAX_JSON_DEPTH:
            raise InputError("request exceeds the JSON depth64 boundary", field_name=field)
        if id(item) in active:
            raise InputError("request must not contain a Python reference cycle", field_name=field)
        active.add(id(item))
        try:
            if kind is list:
                return [visit(child, depth + 1, field + "[" + str(index) + "]")
                    for index, child in enumerate(item)]
            copied = {}
            for key, child in item.items():
                if type(key) is not str:
                    raise InputError("request JSON object keys must be strings", field_name=field)
                _bounded_text(key)
                copied[key] = visit(child, depth + 1, field + "." + key)
            return copied
        finally:
            active.remove(id(item))

    return visit(value, 1, "request")


def _reject_mcp_references(value, field="request"):
    # Match the original dict-value/list traversal order; keys are not values.
    if type(value) is str:
        candidate = value.strip().strip("\"'").replace("\\", "/").lower()
        if any(token in candidate for token in _LEGACY_MCP_TOKENS + _CURRENT_MCP_TOKENS):
            error = InputError("Refusing to persist MCP-only reference in '" + field + "': " + value
                + ". UnrealMCP is an editor authoring bridge; finished assets must not depend on it.",
                "mcp_only_reference", field)
            error.reason = "mcp_only_reference"
            raise error
    elif type(value) is dict:
        for key, child in value.items():
            _reject_mcp_references(child, field + "." + key)
    elif type(value) is list:
        for index, child in enumerate(value):
            _reject_mcp_references(child, field + "[" + str(index) + "]")


def prepare_request(request):
    """Original public object-or-JSON-string parse, dependency guard and bounds."""
    if type(request) is str:
        _bounded_encoded_json(request)
        parsed = json.loads(request, parse_constant=_reject_constant)
    else:
        parsed = request
    if type(parsed) is not dict:
        raise InputError("request must be a JSON object")
    parsed = _json_copy(parsed)
    # Check both the public carrier and the normalized object. String and object
    # inputs can have different JSON escaping sizes; neither is unbounded.
    _bounded_encoded_json(json.dumps(request, ensure_ascii=False, allow_nan=False, separators=(",", ":")))
    _bounded_encoded_json(json.dumps(parsed, ensure_ascii=False, allow_nan=False, separators=(",", ":")))
    _reject_mcp_references(parsed)
    return parsed


def _ascii_lower(value):
    # TChar::ToLower changes ASCII only (installed Char.h78-90).
    return value.translate(_ASCII_LOWER)


def _binding():
    try:
        import unreal
        library = getattr(unreal, TEXT_LIBRARY, None)
        method = getattr(library, TEXT_METHOD, None) if library is not None else None
    except ImportError:
        method = None
    if not callable(method):
        raise InputError("The UE58 text-only semantics helper is unavailable; no Python locale approximation was used",
            "controlrig_solver_text_api_unavailable")
    return method


def _scalar_key(value):
    if value is None:
        return ("null", None)
    if type(value) is bool:
        return ("bool", value)
    if type(value) in (int, float):
        return ("number", float(value).hex())
    return ("string", value)


class _TextSemantics:
    """One bounded Native text batch; never solver policy, asset or world work."""
    def __init__(self, request):
        scalars = [None, ""]
        index = {_scalar_key(value): i for i, value in enumerate(scalars)}
        stack = [request]
        while stack:
            value = stack.pop()
            if type(value) is dict:
                stack.extend(reversed(list(value.values())))
            elif type(value) is list:
                stack.extend(reversed(value))
            else:
                key = _scalar_key(value)
                if key not in index:
                    index[key] = len(scalars)
                    scalars.append(value)
        encoded = json.dumps(scalars, ensure_ascii=False, allow_nan=False, separators=(",", ":"))
        _bounded_encoded_json(encoded)
        raw = _binding()(encoded)
        if type(raw) is not str or _utf16_units(raw) > MAX_TEXT_RESULT_UTF16_UNITS:
            raise InputError("Text helper returned an invalid or oversized receipt", "controlrig_solver_text_receipt_invalid")
        report = json.loads(raw, parse_constant=_reject_constant)
        required_false = ("asset_query", "asset_mutation", "compile_attempted", "save_attempted",
            "solver_policy_executed", "journal_created")
        if (type(report) is not dict or report.get("operation") != TEXT_METHOD
                or report.get("backend") != "UE58_public_FJsonValue_FString_FChar_text_only"
                or any(report.get(key) is not False for key in required_false)):
            raise InputError("Text helper receipt identity or no-asset-work scope is invalid", "controlrig_solver_text_receipt_invalid")
        if report.get("success") is not True:
            raise InputError(str(report.get("error") or "Text semantics batch failed"), "controlrig_solver_text_batch_failed")
        rows = report.get("rows")
        if (type(rows) is not list or len(rows) != len(scalars)
                or type(report.get("scalar_count")) is not int or report["scalar_count"] != len(scalars)):
            raise InputError("Text helper row count is invalid", "controlrig_solver_text_receipt_invalid")
        for row in rows:
            if (type(row) is not dict or type(row.get("has_string")) is not bool
                    or type(row.get("stable_identifier")) is not bool
                    or any(type(row.get(key)) is not str for key in ("trimmed", "lower", "package", "package_lower"))
                    or row.get("boolean") is not None and type(row["boolean"]) is not bool):
                raise InputError("Text helper returned a malformed scalar row", "controlrig_solver_text_receipt_invalid")
        self.rows = rows
        self.index = index
        self.trimmed = {row["trimmed"]: row for row in rows if row["has_string"]}
        self.receipt = {key: value for key, value in report.items() if key != "rows"}
        self.receipt.update(input_sha256=hashlib.sha256(encoded.encode("utf-8")).hexdigest(),
            output_sha256=hashlib.sha256(raw.encode("utf-8")).hexdigest(), rows_preserved_in_native_receipt_not_parent_journal=True,
            actual_text_helper_calls=1, solver_policy_implemented_in_python=True,
            is_native_solver_operation=False)

    def row(self, value):
        return self.rows[self.index[_scalar_key(value)]]

    def string(self, object_value, field):
        value = object_value.get(field) if type(object_value) is dict else None
        return self.row(value)["trimmed"] if type(value) not in (dict, list) else ""

    def try_bool(self, object_value, field):
        value = object_value.get(field) if type(object_value) is dict else None
        return self.row(value)["boolean"] if type(value) not in (dict, list) else None

    def trim(self, value):
        return self.row(value)["trimmed"]

    def stable_name(self, value):
        return self.trimmed[value]["stable_identifier"]

    def normalized_package(self, value):
        return self.trimmed[value]["package"]


def _object(parent, field):
    value = parent.get(field) if type(parent) is dict else None
    return value if type(value) is dict else None


def _array(parent, field):
    value = parent.get(field) if type(parent) is dict else None
    return value if type(value) is list else None


def _allowed_path(path):
    folded = _ascii_lower(path)
    return any(folded == root or folded.startswith(root + "/")
        for root in ("/game/_mcp_sample", "/game/_mcp_temp"))


def _issue(issues, code, message, field, severity="error"):
    issues.append(dict(severity=severity, code=code, message=message, field_path=field))


def _collect_specs(values, names, text):
    if values is None:
        return
    for value in values:
        name = text.trim(value) if type(value) is str else text.string(value, "name") if type(value) is dict else ""
        if name:
            names.add(name)


def _collect_named(root, field, spec_field, text):
    names = set()
    hierarchy = _object(root, "hierarchy")
    _collect_specs(_array(hierarchy, field), names, text)
    _collect_specs(_array(hierarchy, spec_field), names, text)
    return names


def _collect_bones(root, text):
    names = set()
    stack = [root]
    while stack:
        node = stack.pop()
        _collect_specs(_array(node, "bones"), names, text)
        hierarchy = _object(node, "hierarchy")
        _collect_specs(_array(hierarchy, "bones"), names, text)
        _collect_specs(_array(hierarchy, "bone_specs"), names, text)
        # Reverse push preserves the original profile-before-skeleton_profile order.
        skeleton = _object(node, "skeleton_profile")
        profile = _object(node, "profile")
        if skeleton is not None:
            stack.append(skeleton)
        if profile is not None:
            stack.append(profile)
    return names


def validate_prepared_request(params):
    """Validate only the detached, bounded object returned by prepare_request."""
    text = _TextSemantics(params)
    root = _object(params, "recipe")
    if root is None:
        root = params
    issues, warnings, validated = [], [], []
    seen_names = set()
    target = text.string(root, "target_control_rig_path") or text.string(params, "target_control_rig_path")
    package = text.normalized_package(target)
    if package and not _allowed_path(package):
        _issue(issues, "target_outside_sample_temp_scope",
            "solver intent validation targets must stay under /Game/_MCP_Temp or /Game/_MCP_Sample.", "target_control_rig_path")
    gate = _object(root, "solver_gate")
    if gate is None:
        gate = _object(params, "solver_gate")
    if gate is None:
        _issue(issues, "solver_gate_required",
            "solver_gate is required and must explicitly mark this as validation-only without a sample.", "solver_gate")
    else:
        if text.string(gate, "status") != "validation_only_no_sample":
            _issue(issues, "solver_gate_status", "solver_gate.status must be validation_only_no_sample.", "solver_gate.status")
        if text.try_bool(gate, "allows_build") is not False:
            _issue(issues, "solver_gate_build", "solver_gate.allows_build must be false.", "solver_gate.allows_build")
        if text.try_bool(gate, "allows_asset_mutation") is not False:
            _issue(issues, "solver_gate_asset_mutation", "solver_gate.allows_asset_mutation must be false.", "solver_gate.allows_asset_mutation")
        if text.try_bool(gate, "requires_sample_live_proof") is not True:
            _issue(issues, "solver_gate_live_proof", "solver_gate.requires_sample_live_proof must be true before any solver unit build.", "solver_gate.requires_sample_live_proof")
    # Do not deduplicate these checks when root is params: duplicate issues are
    # a real part of the original result, including verification.save twice.
    for scope in (root, params):
        if text.try_bool(scope, "build") is True:
            _issue(issues, "validation_only_no_build", "solver intent validation cannot request build=true.", "build")
    for scope in (root, params):
        if text.try_bool(scope, "save") is True:
            _issue(issues, "validation_only_no_save", "solver intent validation cannot request save=true.", "verification.save")
    for scope in (root, params):
        if text.try_bool(_object(scope, "verification"), "save") is True:
            _issue(issues, "validation_only_no_save", "solver intent validation cannot request save=true.", "verification.save")
    for scope in (root, params):
        if text.try_bool(scope, "execute") is True:
            _issue(issues, "validation_only_no_execute", "solver intent validation cannot execute solver work.", "execute")
    for unit in _array(_object(root, "rigvm"), "units") or []:
        struct = text.string(unit, "struct_name")
        if struct and any(_ascii_lower(fragment) in _ascii_lower(struct) for fragment in _FORBIDDEN_UNIT_FRAGMENTS):
            _issue(issues, "solver_unit_materialized", struct + " is a materialized solver unit; validation-only accepts solver_intents only.", "rigvm.units")
    intents = _array(root, "solver_intents")
    if intents is None:
        intents = _array(params, "solver_intents")
    if not intents:
        _issue(issues, "solver_intents_required", "solver_intents must be a non-empty list.", "solver_intents")
    bones = _collect_bones(root, text)
    controls = _collect_named(root, "controls", "control_specs", text)
    curves = _collect_named(root, "curves", "curve_specs", text)
    if not bones:
        _issue(warnings, "hierarchy_bones_not_provided", "No skeleton/profile bones were provided; bone-chain existence cannot be proven.", "profile.bones", "warning")
    if not controls:
        _issue(warnings, "hierarchy_controls_not_provided", "No hierarchy controls were provided; validation is limited to solver intent shape.", "hierarchy.control_specs", "warning")
    if not curves:
        _issue(warnings, "hierarchy_curves_not_provided", "No hierarchy curves were provided; weight_curve existence cannot be proven.", "hierarchy.curve_specs", "warning")
    global_issue_count = len(issues)
    for index, intent in enumerate(intents or []):
        field = "solver_intents[" + str(index) + "]"
        if type(intent) is not dict:
            _issue(issues, "solver_intent_object", "solver intent entries must be objects.", field)
            continue
        intent_issue_count = len(issues)
        kind = text.string(intent, "kind")
        if kind != "two_bone_ik_limb":
            _issue(issues, "solver_intent_kind_not_allowed", (kind or "<missing>") + " is not a validation-only solver intent candidate.", field + ".kind")
            continue
        unit = text.string(intent, "unit_struct") or text.string(intent, "candidate_unit_struct")
        if unit and unit != "FRigUnit_TwoBoneIKSimplePerItem":
            _issue(issues, "solver_intent_unit_struct", "two_bone_ik_limb must map only to FRigUnit_TwoBoneIKSimplePerItem.", field + ".unit_struct")
        missing = False
        for required in _REQUIRED_FIELDS:
            if not text.string(intent, required):
                missing = True
                _issue(issues, "solver_intent_required_field", "two_bone_ik_limb requires " + required + ".", field + "." + required)
        name = text.string(intent, "name")
        if name:
            if not text.stable_name(name):
                _issue(issues, "solver_intent_name", "solver intent name must be a stable identifier.", field + ".name")
            if name in seen_names:
                _issue(issues, "solver_intent_duplicate_name", "duplicate solver intent name: " + name, field + ".name")
            seen_names.add(name)
        side = _ascii_lower(text.string(intent, "side"))
        if side and side not in ("left", "right"):
            _issue(issues, "solver_intent_side", "two_bone_ik_limb side must be one of left, right.", field + ".side")
        chain = [text.string(intent, part) for part in ("root_bone", "mid_bone", "end_bone")]
        if all(chain) and len(set(chain)) != 3:
            _issue(issues, "solver_intent_bone_chain", "root_bone, mid_bone, and end_bone must be three distinct bones.", field + ".root_bone")
        if bones:
            for part, value in zip(("root_bone", "mid_bone", "end_bone"), chain):
                if value and value not in bones:
                    _issue(issues, "solver_intent_missing_bone", part + " not found in provided skeleton/profile bones: " + value, field + "." + part)
        for part in ("target_control", "pole_control"):
            value = text.string(intent, part)
            if controls and value and value not in controls:
                _issue(issues, "solver_intent_missing_control", part + " not found: " + value, field + "." + part)
        weight = text.string(intent, "weight_curve")
        if curves and weight and weight not in curves:
            _issue(issues, "solver_intent_missing_curve", "weight_curve not found: " + weight, field + ".weight_curve")
        if not missing and global_issue_count == 0 and len(issues) == intent_issue_count:
            validated.append(dict(name=name, kind=kind, candidate_unit_struct="FRigUnit_TwoBoneIKSimplePerItem",
                validation_only=True, adds_solver_units=False, requires_sample_live_proof=True))
    legacy = dict(success=not issues, status="pass" if not issues else "fail", command=METHOD,
        adapter="unrealmcp_native", read_only=True, validation_only=True, sample_required_for_build=True,
        build_allowed=False, asset_mutation_allowed=False, solver_units_added=False, target_modified=False,
        source_modified=False, saves_assets=False, target_control_rig_path=target, target_package_path=package,
        candidate_kinds=["two_bone_ik_limb"], candidate_unit_structs=["FRigUnit_TwoBoneIKSimplePerItem"],
        validated_intents=validated, issue_count=len(issues), issues=issues, warnings=warnings,
        next_action="capture_sample_temp_live_proof_before_solver_build" if not issues else "fix_solver_intents_and_rerun_validation",
        scope="UnrealMCP-owned validation-only solver intent preflight; no asset load, RigVM graph mutation, compile, save, execute, or solve")
    return {"legacy_result": legacy, "text_semantics": text.receipt,
        "execution_backend": "python_validation_with_UE58_text_semantics_batch"}


def validate_controlrig_solver_intents(request):
    return validate_prepared_request(prepare_request(request))


def legacy_exception_result(error):
    return dict(success=False, message=str(error), error=str(error),
        error_code="controlrig_solver_intent_validation_failed", details={"exception_type": type(error).__name__},
        read_only=True, validation_only=True, build_allowed=False, asset_mutation_allowed=False)
