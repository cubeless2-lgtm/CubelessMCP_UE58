"""Fixed PCG7 dispatcher retaining original object/array public inputs.

Calls four existing construction methods and three original Python composite/policy methods. Construction health,
journal, partial outcomes, helper/source admission and compile/save stay there.
No caller-selected function/module/code or alternate server is evaluated.
"""
import base64
import binascii
import copy
import hashlib
import json
import os
from pathlib import Path
import re

from cubeless_mcp_ue58.operations import pcg_spec, state
from cubeless_mcp_ue58.operations.state import OperationError
from .results import reflected_result

MARKER="__CUBELESS_PCG_RESULT__"
MAX_ARGUMENT_BYTES=2*1024*1024
# Match the existing native nonce payload bound; do not extend that contract.
MAX_RESULT_BYTES=16*1024*1024
METHODS=("create_pcg_graph_from_spec","set_pcg_attribute_selector","compile_or_notify_pcg_graph","save_pcg_graph",
    "validate_pcg_source_independence","promote_pcg_temp_graph","pcg_actor_smoke_test")
RESULT_FIELDS=("success","operation_id","stage","error_code","error","changes_json",
    "compile_json","save_json","validation_json","retry_json","data_json")
FIELDS={
    METHODS[0]:("graph_spec","overwrite_existing","allow_overwrite_non_temp","save","dry_run","operation_id"),
    METHODS[1]:("graph_path","node_id","selector_property_name","selector_type","attribute_name",
        "selected_property_name","domain_name","point_property","extra_property","extra_names",
        "reset_extra_names","save","dry_run","operation_id"),
    METHODS[2]:("graph_path","operation_id"),METHODS[3]:("graph_path","operation_id"),
    METHODS[4]:("graph_path","forbidden_dependency_prefixes","allowed_dependency_prefixes"),
    METHODS[5]:("source_graph_path","target_graph_path","forbidden_dependency_prefixes","allowed_dependency_prefixes",
        "overwrite_existing","allow_non_temp_source","dry_run"),
    METHODS[6]:("graph_path","actor_label","selected_only","cleanup","generate","dry_run")}
DEFAULTS={
    METHODS[0]:{"overwrite_existing":False,"allow_overwrite_non_temp":False,"save":True,"dry_run":False,"operation_id":None},
    METHODS[1]:{"selector_type":"attribute","attribute_name":"","selected_property_name":"","domain_name":"",
        "point_property":"","extra_property":"","extra_names":None,"reset_extra_names":True,"save":True,"dry_run":False,"operation_id":None},
    METHODS[2]:{"operation_id":None},METHODS[3]:{"operation_id":None},
    METHODS[4]:{"forbidden_dependency_prefixes":None,"allowed_dependency_prefixes":None},
    METHODS[5]:{"forbidden_dependency_prefixes":None,"allowed_dependency_prefixes":None,
        "overwrite_existing":False,"allow_non_temp_source":False,"dry_run":True},
    METHODS[6]:{"actor_label":"","selected_only":False,"cleanup":True,"generate":True,"dry_run":True}}


def _unique_object(pairs):
    value={}
    for key,item in pairs:
        if key in value:
            raise OperationError("raw_pcg_arguments_invalid","Duplicate JSON object keys are not admitted")
        value[key]=item
    return value


def _nonfinite(value):
    raise OperationError("raw_pcg_arguments_invalid","Nonfinite JSON literals are not admitted")


def _arguments(method,encoded):
    if method not in METHODS or type(encoded) is not str or len(encoded)>4*((MAX_ARGUMENT_BYTES+2)//3):
        raise OperationError("raw_pcg_arguments_invalid","Exact PCG method and bounded base64 JSON required")
    try:
        raw=base64.b64decode(encoded,validate=True)
        if len(raw)>MAX_ARGUMENT_BYTES:
            raise OperationError("raw_pcg_arguments_invalid","Raw PCG arguments exceed2MiB")
        supplied=json.loads(raw.decode("utf-8"),object_pairs_hook=_unique_object,parse_constant=_nonfinite)
    except (binascii.Error,UnicodeError,ValueError,RecursionError) as error:
        raise OperationError("raw_pcg_arguments_invalid","Strict base64/UTF8/JSON arguments required") from error
    if type(supplied) is not dict or set(supplied)-set(FIELDS[method]):
        raise OperationError("raw_pcg_arguments_invalid","Only the exact named public PCG arguments are accepted")
    required=[key for key in FIELDS[method] if key not in DEFAULTS[method]]
    if any(key not in supplied for key in required):
        raise OperationError("raw_pcg_arguments_invalid","A required public PCG argument is missing")
    arguments={**DEFAULTS[method],**supplied}
    if "operation_id" in arguments and type(arguments["operation_id"]) not in (str,type(None)):
        raise OperationError("raw_pcg_arguments_invalid","Existing journal operation_id must be string or null")
    if method==METHODS[0]:
        if type(arguments["graph_spec"]) is not dict:
            raise OperationError("pcg_invalid_graph_spec","graph_spec must be an object")
        booleans=("overwrite_existing","allow_overwrite_non_temp","save","dry_run")
        strings=()
    elif method==METHODS[1]:
        booleans=("reset_extra_names","save","dry_run")
        strings=("graph_path","node_id","selector_property_name","selector_type","attribute_name",
            "selected_property_name","domain_name","point_property","extra_property")
        extra=arguments["extra_names"]
        if extra is not None and (type(extra) is not list or any(type(item) is not str for item in extra)):
            raise OperationError("pcg_selector_request_invalid","extra_names must be a string array or null")
    elif method in (METHODS[4],METHODS[5]):
        booleans=() if method==METHODS[4] else ("overwrite_existing","allow_non_temp_source","dry_run")
        strings=("graph_path",) if method==METHODS[4] else ("source_graph_path","target_graph_path")
        for name in ("forbidden_dependency_prefixes","allowed_dependency_prefixes"):
            value=arguments[name]
            if value is not None and (type(value) is not list or any(type(item) is not str for item in value)):
                raise OperationError("raw_pcg_arguments_invalid",name+" must be a string array or null")
    elif method==METHODS[6]:
        booleans=("selected_only","cleanup","generate","dry_run");strings=("graph_path","actor_label")
    else:
        booleans=();strings=("graph_path",)
    if any(type(arguments[key]) is not bool for key in booleans):
        raise OperationError("pcg_boolean_invalid","PCG options require exact booleans")
    if any(type(arguments[key]) is not str for key in strings):
        raise OperationError("pcg_name_invalid","Original public PCG string arguments require strings")
    return arguments


def _serialized_input(method,arguments):
    # Pure adaptation is completed inside the admission try block. JSON numeric
    # overflow (e.g.1e999), invalid Unicode or recursion cannot reach a domain.
    try:
        if method==METHODS[0]:
            value=json.dumps(arguments["graph_spec"],ensure_ascii=False,allow_nan=False)
        elif method==METHODS[1] and arguments["extra_names"] is not None:
            value=json.dumps(arguments["extra_names"],ensure_ascii=False,allow_nan=False)
        else:
            return None
        value.encode("utf-8")
        return value
    except (ValueError,TypeError,UnicodeError,RecursionError) as error:
        raise OperationError("raw_pcg_arguments_invalid","Public PCG object/array must serialize to finite lossless JSON before dispatch") from error


def _invoke(method,arguments,serialized):
    # Public objects/arrays retain their whole values through raw Registry IO.
    # JSON text appears only at the unchanged internal domain/helper boundary.
    if method==METHODS[0]:
        return pcg_spec.create_pcg_graph_from_spec(
            serialized,
            arguments["overwrite_existing"],arguments["allow_overwrite_non_temp"],
            arguments["save"],arguments["dry_run"],arguments["operation_id"])
    if method==METHODS[1]:
        extra=arguments["extra_names"]
        return pcg_spec.set_pcg_attribute_selector(arguments["graph_path"],arguments["node_id"],
            arguments["selector_property_name"],arguments["selector_type"],arguments["attribute_name"],
            arguments["selected_property_name"],arguments["domain_name"],arguments["point_property"],
            arguments["extra_property"],serialized,
            arguments["reset_extra_names"],arguments["save"],arguments["dry_run"],arguments["operation_id"])
    if method==METHODS[2]:
        return pcg_spec.compile_or_notify_pcg_graph(arguments["graph_path"],arguments["operation_id"])
    if method==METHODS[3]:
        return pcg_spec.save_pcg_graph(arguments["graph_path"],arguments["operation_id"])
    if method in (METHODS[4],METHODS[5]):
        from cubeless_mcp_ue58.operations import pcg_promotion
        return getattr(pcg_promotion,method)(**arguments)
    if method==METHODS[6]:
        from cubeless_mcp_ue58.operations import pcg_actor_smoke
        return pcg_actor_smoke.pcg_actor_smoke_test(**arguments)
    raise OperationError("raw_pcg_method_invalid","Only the seven fixed PCG methods are dispatchable")


def _envelope(record):
    reflected=reflected_result(record)
    fields={key:getattr(reflected,key) for key in RESULT_FIELDS}
    if type(fields["success"]) is not bool or any(type(fields[key]) is not str for key in RESULT_FIELDS[1:]):
        raise OperationError("raw_pcg_result_invalid","Exact existing reflected result field types required")
    return {"returnValue":fields}


def _journal_reference_record(method,arguments,serialized,record):
    """Only an oversized, completed finite create may elide one debug copy.

    This changes data.native_raw_result from inline text to a versioned local
    journal reference. It is not an unchanged nested return contract. All other
    fields and the durable full record are preserved; no journal is rewritten.
    Failure to prove any condition returns the original record unchanged.
    """
    if (method!=METHODS[0] or type(record) is not dict or record.get("success") is not True
            or record.get("stage")!="complete" or record.get("error_code")!="" or record.get("error")!=""
            or type(arguments) is not dict or arguments.get("dry_run") is not False
            or type(record.get("retry")) is not dict or record["retry"].get("replayed") is not False):
        return record
    data=record.get("data")
    if (type(data) is not dict or data.get("construction_route")!="finite_native_fresh"
            or data.get("global_construction_gate_changed") is not False
            or any(data.get(key) is not True for key in ("native_dispatched","native_result_returned","change_state_known",
                "operation_complete","created_new_asset","persistent_target_factory_attempted","persistent_target_mutation_performed"))
            or data.get("outcome_unknown") is not False or data.get("mutation_state_unknown") is not False
            or data.get("in_flight",False) is not False or type(data.get("native_raw_result")) is not str
            or type(data.get("native_result_data")) is not dict or type(data.get("native_outcome")) is not dict
            or any(type(row) is not dict or row.get("change_state_known") is not True for row in record.get("changes",[]))):
        return record
    outcome=data["native_outcome"]
    if (any(outcome.get(key) is not True for key in ("success","change_state_known","postcondition_passed"))
            or outcome.get("operation")!=method or outcome.get("stage")!="complete"):
        return record
    try:
        emitted=copy.deepcopy(record)
        # Existing normalization is pure; match mutate's actual argument fingerprint.
        values=pcg_spec._spec(serialized,arguments["overwrite_existing"],arguments["allow_overwrite_non_temp"],
            arguments["save"],arguments["dry_run"])
        argument_identity=json.dumps({"name":method,"arguments":values},sort_keys=True)
        expected=hashlib.sha256(argument_identity.encode("utf-8")).hexdigest()
        record_identity=json.dumps(emitted,ensure_ascii=False,sort_keys=True,allow_nan=False)
        operation_id=emitted["operation_id"]
        if type(operation_id) is not str or re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_.-]{0,95}",operation_id) is None:
            return record
        if state._active_operation is not None:
            return record
        # Exactly one existing Paths.project_saved_dir() metadata query, only here.
        # Never infer SavedDir from module location or query a target/asset/GC state.
        path=state._journal_path(operation_id)
        if (type(path) is not type(Path()) or not path.is_absolute() or path.name!=operation_id+".json"
                or path.is_symlink() or path.resolve(strict=True)!=path or state._active_operation is not None):
            return record
        # Bounded by the already-held result/arguments plus the existing input
        # budget for journal metadata; no native result/input budget is raised.
        bound=len(json.dumps({"arguments":values,"result":emitted},ensure_ascii=False,sort_keys=True).encode("utf-8"))+MAX_ARGUMENT_BYTES
        before=path.stat()
        if not 0<before.st_size<=bound:
            return record
        with path.open("rb") as stream:
            raw_journal=stream.read(bound+1)
            held=os.fstat(stream.fileno())
        after=path.stat()
        identity=lambda item:(item.st_dev,item.st_ino,item.st_size,item.st_mtime_ns)
        if len(raw_journal)!=before.st_size or identity(before)!=identity(held) or identity(before)!=identity(after):
            return record
        journal=json.loads(raw_journal.decode("utf-8"),object_pairs_hook=_unique_object,parse_constant=_nonfinite)
        if (type(journal) is not dict or journal.get("schema_version")!=1 or journal.get("state")!="complete"
                or journal.get("operation_id")!=operation_id or journal.get("operation")!=method
                or type(journal.get("process_id")) is not int or journal["process_id"]!=os.getpid()
                or json.dumps({"name":method,"arguments":journal.get("arguments")},sort_keys=True)!=argument_identity
                or journal.get("fingerprint")!=expected
                or json.dumps(journal.get("result"),ensure_ascii=False,sort_keys=True,allow_nan=False)!=record_identity
                or state._active_operation is not None
                or json.dumps(record,ensure_ascii=False,sort_keys=True,allow_nan=False)!=record_identity):
            return record
        raw=emitted["data"]["native_raw_result"]
        native=json.loads(raw,object_pairs_hook=_unique_object,parse_constant=_nonfinite)
        if (type(native) is not dict or native.get("data")!=emitted["data"]["native_result_data"]
                or {key:value for key,value in native.items() if key!="data"}!=emitted["data"]["native_outcome"]
                or any(emitted["data"].get(key)!=value for key,value in native["data"].items())
                or journal["result"]["data"]["native_raw_result"]!=raw):
            return record
        raw_bytes=raw.encode("utf-8")
        emitted["data"]["native_raw_result"]={
            "representation":"completed_operation_journal_reference","version":1,"operation_id":operation_id,
            "journal_path":str(path),"journal_sha256":hashlib.sha256(raw_journal).hexdigest(),"journal_bytes":len(raw_journal),
            "json_pointer":"/result/data/native_raw_result","raw_encoding":"utf-8",
            "raw_sha256":hashlib.sha256(raw_bytes).hexdigest(),"raw_bytes":len(raw_bytes),
            "proof":"Completed same-process/same-operation/same-arguments journal and full returned result were read and matched; original raw remains there unchanged.",
            "compatibility":"Oversized successful finite-create debug alias is a reference, not inline text. Canonical data/native_result_data/native_outcome and all other fields remain complete."
        }
        return emitted
    except Exception:
        # Optional lossless evidence reference must never replace a failed,
        # missing, raced or incompatible proof with a fabricated success.
        return record


def emit_result(method,encoded_args_b64,nonce):
    """One fixed native-owned statement invokes one fixed domain method."""
    if type(method) is not str or method not in METHODS:
        raise OperationError("raw_pcg_method_invalid","Native owner must select an exact fixed PCG method")
    if type(nonce) is not str or re.fullmatch(r"[0-9a-f]{32}",nonce) is None:
        raise OperationError("raw_pcg_nonce_invalid","Native owner nonce must be lowercase UUID hex")
    try:
        arguments=_arguments(method,encoded_args_b64)
        serialized=_serialized_input(method,arguments)
    except Exception as error:
        record=state.fail(state.new_result(),error)
        record["data"].update(operation=method,admission_rejected=True,domain_dispatched=False,
            mutation_attempted=False,mutation_performed=False,outcome_unknown=False,
            mutation_state_unknown=False,raw_named_toolset_argument_validation_failed=True)
    else:
        # Unexpected domain/export/print failures escape with the native captured
        # statement/log prefix uncertainty, never a fabricated zero-work result.
        record=_invoke(method,arguments,serialized)
    observed=record["data"]
    if type(observed.get("outcome_unknown")) is bool:
        observed["mutation_state_unknown"]=observed["outcome_unknown"]
    envelope=_envelope(record)
    payload=json.dumps(envelope,ensure_ascii=False,allow_nan=False,separators=(",",":")).encode("utf-8")
    if len(payload)>MAX_RESULT_BYTES and method==METHODS[0] and record.get("success") is True:
        emitted=_journal_reference_record(method,arguments,serialized,record)
        if emitted is not record:
            projected=copy.deepcopy(envelope)
            projected["returnValue"]["data_json"]=json.dumps(emitted["data"],ensure_ascii=False,sort_keys=True)
            candidate=json.dumps(projected,ensure_ascii=False,allow_nan=False,separators=(",",":")).encode("utf-8")
            if len(candidate)<=MAX_RESULT_BYTES:
                payload=candidate
    print(MARKER+nonce+":"+base64.b64encode(payload).decode("ascii"))
