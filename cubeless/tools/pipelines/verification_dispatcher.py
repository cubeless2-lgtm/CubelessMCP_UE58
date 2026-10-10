"""Offline UE58 verifier dispatcher; legacy profiles remain explicitly pending.

This client-side program never starts Unreal, builds/cooks, calls MCP, or retries
mutations. Package inspection may run the explicitly supplied UnrealPak -List.
Successful current profiles certify only their stated verifier scope.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time
import uuid


# Source-derived archival data. These argv values are NEVER executable targets.
LEGACY = json.loads(r'''{"source":{"script_id":"script:Python/scripts/analysis/run_verification.py","path":"Python/scripts/analysis/run_verification.py","sha256":"2568476fee409ea43f774035bf673f2a513d6de39cca3f7fc4d254783c3549c9","static_evidence_ref":"rows[id=script:Python/scripts/analysis/run_verification.py]"},"manifest":[{"key":"py_compile_core_modules","label":"py_compile core modules","source_line":51,"argv":["uv","--directory","./Python","run","python","-m","py_compile","unreal_mcp_server.py","tools/tool_discovery.py","tools/tool_policy.py","tools/management_tools.py","tools/mutation_result_contract.py","tools/niagara_tools.py","tools/python_tools.py","tools/pcg_tools.py","tools/texture_generation.py","services/unreal_bridge_connection.py","services/openai_image_service.py","services/unreal_texture_importer.py"],"source_script_ids":[],"target_status":"not_ported_by_this_inventory","final_disposition":null,"proposed_mapping":{"action":"retarget_offline_code_gate","retain":"syntax/undefined-name checks and path discovery","target":"plugin editor Python + cubeless/tools, no Unreal import in offline process","completion_gate":"actual gate outputs; syntax pass is not engine API validation"}},{"key":"ruff_f821","label":"ruff F821","source_line":76,"argv":["uvx","ruff","check","--select","F821","./Python/tools","./Python/services","./Python/scripts","./Python/unreal_mcp_server.py","./codex/scripts/reactive_surface/niagara_scratch_pad_hlsl_to_nodes_ghost_runner.py"],"source_script_ids":[],"target_status":"not_ported_by_this_inventory","final_disposition":null,"proposed_mapping":{"action":"retarget_offline_code_gate","retain":"syntax/undefined-name checks and path discovery","target":"plugin editor Python + cubeless/tools, no Unreal import in offline process","completion_gate":"actual gate outputs; syntax pass is not engine API validation"}},{"key":"local_install_paths_contract","label":"Local engine and tool path portability","source_line":92,"argv":["uv","--directory","./Python","run","python","scripts/analysis/test_local_install_paths.py"],"source_script_ids":["script:Python/scripts/analysis/test_local_install_paths.py"],"target_status":"not_ported_by_this_inventory","final_disposition":null,"proposed_mapping":{"action":"retarget_offline_code_gate","retain":"syntax/undefined-name checks and path discovery","target":"plugin editor Python + cubeless/tools, no Unreal import in offline process","completion_gate":"actual gate outputs; syntax pass is not engine API validation"}},{"key":"tool_registration","label":"Tool registration smoke test","source_line":97,"argv":["uv","--directory","./Python","run","python","scripts/smoke_tool_registration.py"],"source_script_ids":["script:Python/scripts/smoke_tool_registration.py"],"target_status":"not_ported_by_this_inventory","final_disposition":null,"proposed_mapping":{"action":"rewrite_for_official_registry","retain":"full source/runtime catalog parity and startup/error behavior","target":"static source manifest plus separately named live official list/describe/call profile","completion_gate":"do not let offline import substitute for actual registry validation"}},{"key":"tool_parity","label":"Python and native route parity","source_line":102,"argv":["uv","--directory","./Python","run","python","scripts/analysis/mcp_tool_parity_audit.py","--fail-on","issues"],"source_script_ids":["script:Python/scripts/analysis/mcp_tool_parity_audit.py"],"target_status":"not_ported_by_this_inventory","final_disposition":null,"proposed_mapping":{"action":"rewrite_for_official_registry","retain":"full source/runtime catalog parity and startup/error behavior","target":"static source manifest plus separately named live official list/describe/call profile","completion_gate":"do not let offline import substitute for actual registry validation"}},{"key":"management_contract","label":"Runtime management contract","source_line":109,"argv":["uv","--directory","./Python","run","python","scripts/analysis/test_management_tools.py"],"source_script_ids":["script:Python/scripts/analysis/test_management_tools.py"],"target_status":"not_ported_by_this_inventory","final_disposition":null,"proposed_mapping":{"action":"rewrite_for_official_registry","retain":"full source/runtime catalog parity and startup/error behavior","target":"static source manifest plus separately named live official list/describe/call profile","completion_gate":"do not let offline import substitute for actual registry validation"}},{"key":"chaos_pie_runtime_qa_contract","label":"Chaos PIE runtime QA tool contract","source_line":114,"argv":["uv","--directory","./Python","run","python","scripts/analysis/test_chaos_pie_runtime_qa_tools.py"],"source_script_ids":["script:Python/scripts/analysis/test_chaos_pie_runtime_qa_tools.py"],"target_status":"not_ported_by_this_inventory","final_disposition":null,"proposed_mapping":{"action":"retain_gate_and_inspect_domain_contract","retain":"original unique assertions, error behavior and report schema","completion_gate":"map each domain assertion to actual UE5.8 implementation before claiming equivalent"}},{"key":"tool_policy_contract","label":"Tool safety policy contract","source_line":119,"argv":["uv","--directory","./Python","run","python","scripts/analysis/test_tool_policy_contract.py"],"source_script_ids":["script:Python/scripts/analysis/test_tool_policy_contract.py"],"target_status":"not_ported_by_this_inventory","final_disposition":null,"proposed_mapping":{"action":"retain_gate_and_inspect_domain_contract","retain":"original unique assertions, error behavior and report schema","completion_gate":"map each domain assertion to actual UE5.8 implementation before claiming equivalent"}},{"key":"project_cpp_approval_gate_contract","label":"Project C++ approval gate contract","source_line":124,"argv":["uv","--directory","./Python","run","python","scripts/analysis/test_project_cpp_approval_gate.py"],"source_script_ids":["script:Python/scripts/analysis/test_project_cpp_approval_gate.py"],"target_status":"not_ported_by_this_inventory","final_disposition":null,"proposed_mapping":{"action":"rewrite_current_cpp_scope_contract","retain":"game runtime independence and allowed file scope","remove_candidate":"historical approval-token requirement for authorized editor-plugin C++","completion_gate":"descriptor/module/asset/package evidence; user prohibition remains in force"}},{"key":"verification_dispatcher_contract","label":"Verification dispatcher contract","source_line":129,"argv":["uv","--directory","./Python","run","python","scripts/analysis/test_run_verification.py"],"source_script_ids":["script:Python/scripts/analysis/test_run_verification.py"],"target_status":"not_ported_by_this_inventory","final_disposition":null,"proposed_mapping":{"action":"port_dispatcher_self_contract","retain":"exact profile order, fail-fast first exit, subprocess isolation, plan mode side-effect freedom","completion_gate":"mocked failures, paths with spaces, missing tool, report persistence and no child execution in plan mode"}},{"key":"mutation_result_contract","label":"Mutation result contract","source_line":134,"argv":["uv","--directory","./Python","run","python","scripts/analysis/test_mutation_result_contract.py"],"source_script_ids":["script:Python/scripts/analysis/test_mutation_result_contract.py"],"target_status":"not_ported_by_this_inventory","final_disposition":null,"proposed_mapping":{"action":"retain_gate_and_inspect_domain_contract","retain":"original unique assertions, error behavior and report schema","completion_gate":"map each domain assertion to actual UE5.8 implementation before claiming equivalent"}},{"key":"cpp_server_timeout_contract","label":"C++ server timeout static contract","source_line":139,"argv":["uv","--directory","./Python","run","python","scripts/analysis/test_cpp_server_timeout_contract.py"],"source_script_ids":["script:Python/scripts/analysis/test_cpp_server_timeout_contract.py"],"target_status":"not_ported_by_this_inventory","final_disposition":null,"proposed_mapping":{"action":"split_transport_detail_from_behavior_contract","retain":"timeouts, unknown mutation outcomes, parse/correlation/isolation and no unintended retries where applicable","remove_candidate":"old socket framing, legacy port/bridge internal implementation assertions only","completion_gate":"inspect each assertion before final classification; HTTP session/response tests required"}},{"key":"cpp_phase1_transport_contract","label":"C++ Phase 1 transport safety contract","source_line":144,"argv":["uv","--directory","./Python","run","python","scripts/analysis/test_cpp_phase1_transport_safety_contract.py"],"source_script_ids":["script:Python/scripts/analysis/test_cpp_phase1_transport_safety_contract.py"],"target_status":"not_ported_by_this_inventory","final_disposition":null,"proposed_mapping":{"action":"split_transport_detail_from_behavior_contract","retain":"timeouts, unknown mutation outcomes, parse/correlation/isolation and no unintended retries where applicable","remove_candidate":"old socket framing, legacy port/bridge internal implementation assertions only","completion_gate":"inspect each assertion before final classification; HTTP session/response tests required"}},{"key":"cpp_transport_telemetry_contract","label":"C++ Phase 1C transport telemetry contract","source_line":149,"argv":["uv","--directory","./Python","run","python","scripts/analysis/test_cpp_phase1c_transport_telemetry_contract.py"],"source_script_ids":["script:Python/scripts/analysis/test_cpp_phase1c_transport_telemetry_contract.py"],"target_status":"not_ported_by_this_inventory","final_disposition":null,"proposed_mapping":{"action":"split_transport_detail_from_behavior_contract","retain":"timeouts, unknown mutation outcomes, parse/correlation/isolation and no unintended retries where applicable","remove_candidate":"old socket framing, legacy port/bridge internal implementation assertions only","completion_gate":"inspect each assertion before final classification; HTTP session/response tests required"}},{"key":"cpp_event_wake_contract","label":"C++ Phase 1C event wake contract","source_line":154,"argv":["uv","--directory","./Python","run","python","scripts/analysis/test_cpp_phase1c_event_wake_contract.py"],"source_script_ids":["script:Python/scripts/analysis/test_cpp_phase1c_event_wake_contract.py"],"target_status":"not_ported_by_this_inventory","final_disposition":null,"proposed_mapping":{"action":"split_transport_detail_from_behavior_contract","retain":"timeouts, unknown mutation outcomes, parse/correlation/isolation and no unintended retries where applicable","remove_candidate":"old socket framing, legacy port/bridge internal implementation assertions only","completion_gate":"inspect each assertion before final classification; HTTP session/response tests required"}},{"key":"mcp_focused_crash_guard_contract","label":"Focused editor crash guard contract","source_line":159,"argv":["uv","--directory","./Python","run","python","scripts/analysis/test_mcp_crash_guard_contract.py"],"source_script_ids":["script:Python/scripts/analysis/test_mcp_crash_guard_contract.py"],"target_status":"not_ported_by_this_inventory","final_disposition":null,"proposed_mapping":{"action":"retain_gate_and_inspect_domain_contract","retain":"original unique assertions, error behavior and report schema","completion_gate":"map each domain assertion to actual UE5.8 implementation before claiming equivalent"}},{"key":"unreal_texture_import_interchange_contract","label":"Texture import Interchange bypass contract","source_line":164,"argv":["uv","--directory","./Python","run","python","scripts/analysis/test_unreal_texture_importer_interchange_guard.py"],"source_script_ids":["script:Python/scripts/analysis/test_unreal_texture_importer_interchange_guard.py"],"target_status":"not_ported_by_this_inventory","final_disposition":null,"proposed_mapping":{"action":"retain_gate_and_inspect_domain_contract","retain":"original unique assertions, error behavior and report schema","completion_gate":"map each domain assertion to actual UE5.8 implementation before claiming equivalent"}},{"key":"cpp_blueprint_component_property_contract","label":"C++ Blueprint component property safety contract","source_line":169,"argv":["uv","--directory","./Python","run","python","scripts/analysis/test_cpp_blueprint_component_property_safety_contract.py"],"source_script_ids":["script:Python/scripts/analysis/test_cpp_blueprint_component_property_safety_contract.py"],"target_status":"not_ported_by_this_inventory","final_disposition":null,"proposed_mapping":{"action":"retain_gate_and_inspect_domain_contract","retain":"original unique assertions, error behavior and report schema","completion_gate":"map each domain assertion to actual UE5.8 implementation before claiming equivalent"}},{"key":"cpp_blueprint_input_key_graph_compatibility_contract","label":"C++ Blueprint input-key graph compatibility contract","source_line":174,"argv":["uv","--directory","./Python","run","python","scripts/analysis/test_cpp_blueprint_input_key_graph_compatibility_contract.py"],"source_script_ids":["script:Python/scripts/analysis/test_cpp_blueprint_input_key_graph_compatibility_contract.py"],"target_status":"not_ported_by_this_inventory","final_disposition":null,"proposed_mapping":{"action":"retain_gate_and_inspect_domain_contract","retain":"original unique assertions, error behavior and report schema","completion_gate":"map each domain assertion to actual UE5.8 implementation before claiming equivalent"}},{"key":"cpp_blueprint_variable_exposure_contract","label":"C++ Blueprint variable exposure contract","source_line":179,"argv":["uv","--directory","./Python","run","python","scripts/analysis/test_cpp_blueprint_variable_exposure_contract.py"],"source_script_ids":["script:Python/scripts/analysis/test_cpp_blueprint_variable_exposure_contract.py"],"target_status":"not_ported_by_this_inventory","final_disposition":null,"proposed_mapping":{"action":"retain_gate_and_inspect_domain_contract","retain":"original unique assertions, error behavior and report schema","completion_gate":"map each domain assertion to actual UE5.8 implementation before claiming equivalent"}},{"key":"ai_transport_marker_contract","label":"AI transport marker preservation","source_line":184,"argv":["uv","--directory","./Python","run","python","scripts/analysis/test_ai_transport_marker_preservation.py"],"source_script_ids":["script:Python/scripts/analysis/test_ai_transport_marker_preservation.py"],"target_status":"not_ported_by_this_inventory","final_disposition":null,"proposed_mapping":{"action":"split_transport_detail_from_behavior_contract","retain":"timeouts, unknown mutation outcomes, parse/correlation/isolation and no unintended retries where applicable","remove_candidate":"old socket framing, legacy port/bridge internal implementation assertions only","completion_gate":"inspect each assertion before final classification; HTTP session/response tests required"}},{"key":"ai_ue56_domain_contract","label":"UE 5.6 AI domain contract","source_line":189,"argv":["uv","--directory","./Python","run","python","scripts/analysis/test_ai_ue56_domain_contract.py"],"source_script_ids":["script:Python/scripts/analysis/test_ai_ue56_domain_contract.py"],"target_status":"not_ported_by_this_inventory","final_disposition":null,"proposed_mapping":{"action":"retain_gate_and_inspect_domain_contract","retain":"original unique assertions, error behavior and report schema","completion_gate":"map each domain assertion to actual UE5.8 implementation before claiming equivalent"}},{"key":"gas_tools_contract","label":"UE 5.6 GAS tools contract","source_line":194,"argv":["uv","--directory","./Python","run","python","scripts/analysis/test_gas_tools_contract.py"],"source_script_ids":["script:Python/scripts/analysis/test_gas_tools_contract.py"],"target_status":"not_ported_by_this_inventory","final_disposition":null,"proposed_mapping":{"action":"retain_gate_and_inspect_domain_contract","retain":"original unique assertions, error behavior and report schema","completion_gate":"map each domain assertion to actual UE5.8 implementation before claiming equivalent"}},{"key":"optional_domain_plugin_contract","label":"Optional UE 5.6 domain plugin contract","source_line":199,"argv":["uv","--directory","./Python","run","python","scripts/analysis/test_optional_domain_plugin_contract.py"],"source_script_ids":["script:Python/scripts/analysis/test_optional_domain_plugin_contract.py"],"target_status":"not_ported_by_this_inventory","final_disposition":null,"proposed_mapping":{"action":"retain_gate_and_inspect_domain_contract","retain":"original unique assertions, error behavior and report schema","completion_gate":"map each domain assertion to actual UE5.8 implementation before claiming equivalent"}},{"key":"controlrig_authoring_contract","label":"ControlRig authoring tools contract","source_line":204,"argv":["uv","--directory","./Python","run","python","scripts/analysis/test_controlrig_authoring_tools.py"],"source_script_ids":["script:Python/scripts/analysis/test_controlrig_authoring_tools.py"],"target_status":"not_ported_by_this_inventory","final_disposition":null,"proposed_mapping":{"action":"retain_gate_and_inspect_domain_contract","retain":"original unique assertions, error behavior and report schema","completion_gate":"map each domain assertion to actual UE5.8 implementation before claiming equivalent"}},{"key":"niagara_assignment_transport_contract","label":"Niagara assignment transport safety","source_line":209,"argv":["uv","--directory","./Python","run","python","scripts/analysis/test_niagara_assignment_transport_safety.py"],"source_script_ids":["script:Python/scripts/analysis/test_niagara_assignment_transport_safety.py"],"target_status":"not_ported_by_this_inventory","final_disposition":null,"proposed_mapping":{"action":"split_transport_detail_from_behavior_contract","retain":"timeouts, unknown mutation outcomes, parse/correlation/isolation and no unintended retries where applicable","remove_candidate":"old socket framing, legacy port/bridge internal implementation assertions only","completion_gate":"inspect each assertion before final classification; HTTP session/response tests required"}},{"key":"unreal_python_result_file_isolation","label":"Unreal Python result-file isolation","source_line":214,"argv":["uv","--directory","./Python","run","python","scripts/analysis/test_unreal_python_result_file_isolation.py"],"source_script_ids":["script:Python/scripts/analysis/test_unreal_python_result_file_isolation.py"],"target_status":"not_ported_by_this_inventory","final_disposition":null,"proposed_mapping":{"action":"retain_gate_and_inspect_domain_contract","retain":"original unique assertions, error behavior and report schema","completion_gate":"map each domain assertion to actual UE5.8 implementation before claiming equivalent"}},{"key":"tool_discovery_contract","label":"Tool discovery contract","source_line":219,"argv":["uv","--directory","./Python","run","python","scripts/analysis/test_tool_discovery_contract.py"],"source_script_ids":["script:Python/scripts/analysis/test_tool_discovery_contract.py"],"target_status":"not_ported_by_this_inventory","final_disposition":null,"proposed_mapping":{"action":"rewrite_for_official_registry","retain":"full source/runtime catalog parity and startup/error behavior","target":"static source manifest plus separately named live official list/describe/call profile","completion_gate":"do not let offline import substitute for actual registry validation"}},{"key":"tool_exposure_benchmark","label":"Tool exposure benchmark CLI check","source_line":224,"argv":["uv","--directory","./Python","run","python","scripts/analysis/mcp_tool_exposure_benchmark.py","check"],"source_script_ids":["script:Python/scripts/analysis/mcp_tool_exposure_benchmark.py"],"target_status":"not_ported_by_this_inventory","final_disposition":null,"proposed_mapping":{"action":"rewrite_for_official_registry","retain":"full source/runtime catalog parity and startup/error behavior","target":"static source manifest plus separately named live official list/describe/call profile","completion_gate":"do not let offline import substitute for actual registry validation"}},{"key":"unreal_bridge_connection_contract","label":"Unreal bridge transport contract","source_line":230,"argv":["uv","--directory","./Python","run","python","scripts/analysis/test_unreal_bridge_connection.py"],"source_script_ids":["script:Python/scripts/analysis/test_unreal_bridge_connection.py"],"target_status":"not_ported_by_this_inventory","final_disposition":null,"proposed_mapping":{"action":"split_transport_detail_from_behavior_contract","retain":"timeouts, unknown mutation outcomes, parse/correlation/isolation and no unintended retries where applicable","remove_candidate":"old socket framing, legacy port/bridge internal implementation assertions only","completion_gate":"inspect each assertion before final classification; HTTP session/response tests required"}},{"key":"unreal_bridge_loopback_contract","label":"Unreal bridge loopback transport regression","source_line":235,"argv":["uv","--directory","./Python","run","python","scripts/analysis/test_unreal_bridge_loopback.py"],"source_script_ids":["script:Python/scripts/analysis/test_unreal_bridge_loopback.py"],"target_status":"not_ported_by_this_inventory","final_disposition":null,"proposed_mapping":{"action":"split_transport_detail_from_behavior_contract","retain":"timeouts, unknown mutation outcomes, parse/correlation/isolation and no unintended retries where applicable","remove_candidate":"old socket framing, legacy port/bridge internal implementation assertions only","completion_gate":"inspect each assertion before final classification; HTTP session/response tests required"}},{"key":"phase1_practical_canaries_contract","label":"Practical Phase 1 canary safety contract","source_line":240,"argv":["uv","--directory","./Python","run","python","scripts/analysis/test_mcp_rpc_phase1_practical_canaries.py"],"source_script_ids":["script:Python/scripts/analysis/test_mcp_rpc_phase1_practical_canaries.py"],"target_status":"not_ported_by_this_inventory","final_disposition":null,"proposed_mapping":{"action":"retain_gate_and_inspect_domain_contract","retain":"original unique assertions, error behavior and report schema","completion_gate":"map each domain assertion to actual UE5.8 implementation before claiming equivalent"}},{"key":"phase1_practical_orchestrator_contract","label":"Practical Phase 1 orchestrator contract","source_line":245,"argv":["uv","--directory","./Python","run","python","scripts/analysis/test_mcp_rpc_phase1_practical_orchestrator.py"],"source_script_ids":["script:Python/scripts/analysis/test_mcp_rpc_phase1_practical_orchestrator.py"],"target_status":"not_ported_by_this_inventory","final_disposition":null,"proposed_mapping":{"action":"retain_gate_and_inspect_domain_contract","retain":"original unique assertions, error behavior and report schema","completion_gate":"map each domain assertion to actual UE5.8 implementation before claiming equivalent"}},{"key":"phase1b_live_transport_gate_contract","label":"Phase 1B live transport gate offline contract","source_line":250,"argv":["uv","--directory","./Python","run","python","scripts/analysis/test_mcp_rpc_phase1b_live_transport_gate.py"],"source_script_ids":["script:Python/scripts/analysis/test_mcp_rpc_phase1b_live_transport_gate.py"],"target_status":"not_ported_by_this_inventory","final_disposition":null,"proposed_mapping":{"action":"split_transport_detail_from_behavior_contract","retain":"timeouts, unknown mutation outcomes, parse/correlation/isolation and no unintended retries where applicable","remove_candidate":"old socket framing, legacy port/bridge internal implementation assertions only","completion_gate":"inspect each assertion before final classification; HTTP session/response tests required"}},{"key":"phase1_release_orchestrator_contract","label":"Phase 1 release orchestrator offline contract","source_line":255,"argv":["uv","--directory","./Python","run","python","scripts/analysis/test_mcp_rpc_phase1_release_orchestrator.py"],"source_script_ids":["script:Python/scripts/analysis/test_mcp_rpc_phase1_release_orchestrator.py"],"target_status":"not_ported_by_this_inventory","final_disposition":null,"proposed_mapping":{"action":"retain_gate_and_inspect_domain_contract","retain":"original unique assertions, error behavior and report schema","completion_gate":"map each domain assertion to actual UE5.8 implementation before claiming equivalent"}},{"key":"write_tool_inventory_contract","label":"Write tool inventory contract","source_line":260,"argv":["uv","--directory","./Python","run","python","scripts/analysis/test_mcp_write_tool_inventory.py"],"source_script_ids":["script:Python/scripts/analysis/test_mcp_write_tool_inventory.py"],"target_status":"not_ported_by_this_inventory","final_disposition":null,"proposed_mapping":{"action":"retain_gate_and_inspect_domain_contract","retain":"original unique assertions, error behavior and report schema","completion_gate":"map each domain assertion to actual UE5.8 implementation before claiming equivalent"}},{"key":"write_postcondition_observer_contract","label":"Write postcondition observer contract","source_line":265,"argv":["uv","--directory","./Python","run","python","scripts/analysis/test_mcp_write_postcondition_observer.py"],"source_script_ids":["script:Python/scripts/analysis/test_mcp_write_postcondition_observer.py"],"target_status":"not_ported_by_this_inventory","final_disposition":null,"proposed_mapping":{"action":"retain_gate_and_inspect_domain_contract","retain":"original unique assertions, error behavior and report schema","completion_gate":"map each domain assertion to actual UE5.8 implementation before claiming equivalent"}},{"key":"write_tool_inventory_check","label":"Write tool inventory advisory check","source_line":270,"argv":["uv","--directory","./Python","run","python","scripts/analysis/mcp_write_tool_inventory.py","check","--output","MCPGameProject/Saved/MCP/WriteVerification/ci_inventory.json"],"source_script_ids":["script:Python/scripts/analysis/mcp_write_tool_inventory.py"],"target_status":"not_ported_by_this_inventory","final_disposition":null,"proposed_mapping":{"action":"retain_gate_and_inspect_domain_contract","retain":"original unique assertions, error behavior and report schema","completion_gate":"map each domain assertion to actual UE5.8 implementation before claiming equivalent"}},{"key":"write_postcondition_observer_check","label":"Write postcondition observer advisory check","source_line":278,"argv":["uv","--directory","./Python","run","python","scripts/analysis/run_mcp_write_postcondition_observer.py","check","--output","MCPGameProject/Saved/MCP/WriteVerification/ci_observer_plan.json"],"source_script_ids":["script:Python/scripts/analysis/run_mcp_write_postcondition_observer.py"],"target_status":"not_ported_by_this_inventory","final_disposition":null,"proposed_mapping":{"action":"retain_gate_and_inspect_domain_contract","retain":"original unique assertions, error behavior and report schema","completion_gate":"map each domain assertion to actual UE5.8 implementation before claiming equivalent"}},{"key":"niagara_authoring_offline_gates","label":"Niagara authoring offline gates","source_line":286,"argv":["uv","--directory","./Python","run","python","scripts/analysis/run_niagara_authoring_offline_gates.py","check"],"source_script_ids":["script:Python/scripts/analysis/run_niagara_authoring_offline_gates.py"],"target_status":"not_ported_by_this_inventory","final_disposition":null,"proposed_mapping":{"action":"retain_gate_and_inspect_domain_contract","retain":"original unique assertions, error behavior and report schema","completion_gate":"map each domain assertion to actual UE5.8 implementation before claiming equivalent"}},{"key":"response_schema_golden_tests","label":"Response schema golden tests","source_line":292,"argv":["uv","--directory","./Python","run","python","scripts/analysis/run_response_schema_tests.py"],"source_script_ids":["script:Python/scripts/analysis/run_response_schema_tests.py"],"target_status":"not_ported_by_this_inventory","final_disposition":null,"proposed_mapping":{"action":"retain_gate_and_inspect_domain_contract","retain":"original unique assertions, error behavior and report schema","completion_gate":"map each domain assertion to actual UE5.8 implementation before claiming equivalent"}},{"key":"response_schema_audit","label":"Response schema audit check","source_line":297,"argv":["uv","--directory","./Python","run","python","scripts/analysis/python_tool_response_schema_audit.py","check"],"source_script_ids":["script:Python/scripts/analysis/python_tool_response_schema_audit.py"],"target_status":"not_ported_by_this_inventory","final_disposition":null,"proposed_mapping":{"action":"retain_gate_and_inspect_domain_contract","retain":"original unique assertions, error behavior and report schema","completion_gate":"map each domain assertion to actual UE5.8 implementation before claiming equivalent"}}],"profiles":{"full":["py_compile_core_modules","ruff_f821","local_install_paths_contract","tool_registration","tool_parity","management_contract","chaos_pie_runtime_qa_contract","tool_policy_contract","project_cpp_approval_gate_contract","verification_dispatcher_contract","mutation_result_contract","cpp_server_timeout_contract","cpp_phase1_transport_contract","cpp_transport_telemetry_contract","cpp_event_wake_contract","mcp_focused_crash_guard_contract","unreal_texture_import_interchange_contract","cpp_blueprint_component_property_contract","cpp_blueprint_input_key_graph_compatibility_contract","cpp_blueprint_variable_exposure_contract","ai_transport_marker_contract","ai_ue56_domain_contract","gas_tools_contract","optional_domain_plugin_contract","controlrig_authoring_contract","niagara_assignment_transport_contract","unreal_python_result_file_isolation","tool_discovery_contract","tool_exposure_benchmark","unreal_bridge_connection_contract","unreal_bridge_loopback_contract","phase1_practical_canaries_contract","phase1_practical_orchestrator_contract","phase1b_live_transport_gate_contract","phase1_release_orchestrator_contract","write_tool_inventory_contract","write_postcondition_observer_contract","write_tool_inventory_check","write_postcondition_observer_check","niagara_authoring_offline_gates","response_schema_golden_tests","response_schema_audit"],"core":["py_compile_core_modules","ruff_f821","local_install_paths_contract","project_cpp_approval_gate_contract","verification_dispatcher_contract"],"catalog":["tool_registration","tool_parity","management_contract","chaos_pie_runtime_qa_contract","tool_policy_contract","ai_ue56_domain_contract","gas_tools_contract","optional_domain_plugin_contract","tool_discovery_contract","tool_exposure_benchmark"],"transport":["mutation_result_contract","cpp_server_timeout_contract","cpp_phase1_transport_contract","cpp_transport_telemetry_contract","cpp_event_wake_contract","mcp_focused_crash_guard_contract","unreal_texture_import_interchange_contract","cpp_blueprint_component_property_contract","cpp_blueprint_input_key_graph_compatibility_contract","cpp_blueprint_variable_exposure_contract","ai_transport_marker_contract","controlrig_authoring_contract","niagara_assignment_transport_contract","unreal_python_result_file_isolation","unreal_bridge_connection_contract","unreal_bridge_loopback_contract"],"schema":["response_schema_golden_tests","response_schema_audit"],"niagara":["niagara_authoring_offline_gates"],"write-observer":["write_tool_inventory_contract","write_postcondition_observer_contract","write_tool_inventory_check","write_postcondition_observer_check"],"release-offline":["phase1_practical_canaries_contract","phase1_practical_orchestrator_contract","phase1b_live_transport_gate_contract","phase1_release_orchestrator_contract"],"windows-transport":["unreal_bridge_connection_contract","mutation_result_contract","cpp_server_timeout_contract","cpp_phase1_transport_contract","cpp_transport_telemetry_contract","cpp_event_wake_contract","cpp_blueprint_component_property_contract","cpp_blueprint_variable_exposure_contract","unreal_bridge_loopback_contract","phase1_practical_canaries_contract","phase1_practical_orchestrator_contract","phase1b_live_transport_gate_contract","phase1_release_orchestrator_contract","unreal_python_result_file_isolation"]}}''')
LEGACY_MANIFEST = LEGACY["manifest"]
LEGACY_PROFILES = LEGACY["profiles"]
PROJECT_ROOT = Path(__file__).resolve().parents[3]
CURRENT_PROFILES = {
    "bootstrap-offline": ["pipeline_contract_unit", "operation_journal_unit", "package_checker_unit", "marker_verifier_unit", "dispatcher_unit"],
    "blueprint-author-evidence": ["blueprint_author_report"],
    "blueprint-reload-evidence": ["blueprint_author_report", "blueprint_reload_report", "blueprint_reload_link"],
    "blueprint-cleanup-evidence": ["blueprint_author_report", "blueprint_cleanup_report", "blueprint_cleanup_link"],
    "material-author-evidence": ["material_author_report"],
    "material-reload-evidence": ["material_author_report", "material_reload_report"],
    "package-static-evidence": ["package_static_audit"],
    "runtime-marker-evidence": ["runtime_marker_audit"],
}


def digest(path):
    with Path(path).open("rb") as stream:
        value = hashlib.file_digest(stream, "sha256").hexdigest()
    return value


def read_json(path):
    return json.loads(Path(path).read_text(encoding="utf-8-sig"))


def make_parser():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--profile", choices=[*LEGACY_PROFILES, *CURRENT_PROFILES], default="full")
    parser.add_argument("--plan", choices=["text", "json"], help="Render only; never create files or run children")
    parser.add_argument("--output", type=Path, help="New durable report; required outside plan mode")
    for name in ("author-report", "reload-report", "cleanup-report", "stage", "unrealpak", "fixture-report", "baseline", "launch-report"):
        parser.add_argument("--" + name, type=Path)
    parser.add_argument("--receipt", type=Path, action="append", default=[])
    return parser


def build_plan(args, *, root=PROJECT_ROOT):
    root = Path(root).resolve()
    legacy = args.profile in LEGACY_PROFILES
    if args.profile not in {**LEGACY_PROFILES, **CURRENT_PROFILES}:
        raise ValueError("Unknown verification profile: " + args.profile)
    output = args.output.resolve() if args.output else None
    child_dir = output.parent / (output.stem + "-children") if output else None
    gates = []
    if legacy:
        by_key = {row["key"]: row for row in LEGACY_MANIFEST}
        for key in LEGACY_PROFILES[args.profile]:
            source = by_key[key]
            gates.append({"key": key, "kind": "pending_legacy_gate", "mapping_status": "pending",
                          "reason": "Legacy assertion equivalence is not implemented and verified; current partial gates do not close this gate",
                          "legacy_source": source, "argv": None, "required_inputs": [], "missing_inputs": []})
    else:
        verify = root / "cubeless/tools/verification"
        unit_scripts = {
            "pipeline_contract_unit": (verify / "test_pipeline_contracts.py", []),
            "operation_journal_unit": (root / "Plugins/CubelessMCP_UE58/Content/Python/tests/test_operation_journal.py", []),
            "package_checker_unit": (verify / "test_package_independence.py", []),
            "marker_verifier_unit": (verify / "validate_runtime_marker.py", ["self-test"]),
            "dispatcher_unit": (verify / "test_verification_dispatcher.py", []),
        }
        for key in CURRENT_PROFILES[args.profile]:
            gate = {"key": key, "kind": "subprocess", "mapping_status": "current_scope_only", "argv": None,
                    "required_inputs": [], "missing_inputs": [], "cwd": str(root), "result_file": None,
                    "scope": "Synthetic/offline checks only; no engine behavior evidence"}
            script, extras = None, []
            result = child_dir / (key + ".json") if child_dir else Path("<new-report-children>") / (key + ".json")
            if key in unit_scripts:
                script, extras = unit_scripts[key]
            elif key.startswith("material_") and key.endswith("_report"):
                phase = key.split("_")[1]
                value = getattr(args, phase + "_report")
                gate["required_inputs"] = [{"option": "--" + phase + "-report", "path": str(value.resolve()) if value else None, "type": "file"}]
                script = verify / "material_pipeline_report.py"
                extras = [str(value.resolve()) if value else "<missing-report>", "--phase", phase, "--output", str(result)]
                if phase == "reload":
                    author = args.author_report
                    gate["required_inputs"].append({"option": "--author-report", "path": str(author.resolve()) if author else None, "type": "file"})
                    extras += ["--parent-report", str(author.resolve()) if author else "<missing-author>"]
                gate.update(result_file=str(result), expected_schema="cubeless_material_report_verification_v1", expected_boolean="passed",
                            scope="Independent Material " + phase + " readback/stage contract including actual author bytes for reload; source authenticity requires live evidence")
            elif key.startswith("blueprint_") and key.endswith("_report"):
                phase = key.split("_")[1]
                value = getattr(args, phase + "_report")
                gate["required_inputs"] = [{"option": "--" + phase + "-report", "path": str(value.resolve()) if value else None, "type": "file"}]
                script = verify / "pipeline_report.py"
                extras = [str(value.resolve()) if value else "<missing-report>", "--phase", phase, "--output", str(result)]
                gate.update(result_file=str(result), expected_schema="cubeless_pipeline_report_verification_v1", expected_boolean="passed",
                            scope="Independent report-contract verifier for Blueprint " + phase + "; source authenticity requires live evidence")
            elif key.endswith("_link"):
                phase = key.split("_")[1]
                gate.update(kind="blueprint_lifecycle_link", phase=phase,
                            scope="Author report hash, exact fixture/project/run identity and recorded parent link")
                gate["required_inputs"] = [{"option": "--" + name + "-report", "path": str(getattr(args, name + "_report").resolve()) if getattr(args, name + "_report") else None, "type": "file"} for name in ("author", phase)]
            elif key == "package_static_audit":
                script = verify / "check_package_independence.py"
                gate["required_inputs"] = [{"option": "--stage", "path": str(args.stage.resolve()) if args.stage else None, "type": "directory"},
                                            {"option": "--unrealpak", "path": str(args.unrealpak.resolve()) if args.unrealpak else None, "type": "file"}]
                gate["required_inputs"] += [{"option": "--receipt", "path": str(p.resolve()), "type": "file"} for p in args.receipt]
                if not args.receipt:
                    gate["missing_inputs"].append("--receipt (at least one actual UBT receipt is required by this profile)")
                if output and args.stage and (output.is_relative_to(args.stage.resolve()) or child_dir.is_relative_to(args.stage.resolve())):
                    gate["missing_inputs"].append("Report and child evidence must be outside the staged game directory")
                extras = ["--stage", str(args.stage.resolve()) if args.stage else "<missing-stage>", "--unrealpak", str(args.unrealpak.resolve()) if args.unrealpak else "<missing-unrealpak>"]
                for value in args.receipt:
                    extras += ["--receipt", str(value.resolve())]
                extras += ["--output", str(result)]
                gate.update(result_file=str(result), expected_schema="cubeless_package_independence_v2",
                            scope="Fresh staged filenames, UBT receipts and all Pak listings; no full serialized-reference or runtime proof")
            elif key == "runtime_marker_audit":
                script = verify / "validate_runtime_marker.py"
                extras = ["verify"]
                for name in ("fixture_report", "baseline", "launch_report"):
                    value = getattr(args, name)
                    flag = "--" + name.replace("_", "-")
                    gate["required_inputs"].append({"option": flag, "path": str(value.resolve()) if value else None, "type": "file"})
                    extras += [flag, str(value.resolve()) if value else "<missing-input>"]
                extras += ["--output", str(result)]
                gate.update(result_file=str(result), expected_schema="cubeless_runtime_marker_verification_v1", expected_boolean="success",
                            scope="Fresh SaveGame header, fixture/baseline/executable hashes and supplied launch evidence; no plugin exclusion proof")
            if script:
                gate["argv"] = [sys.executable, "-B", str(script), *extras]
                gate["required_inputs"].insert(0, {"option": "verifier", "path": str(script), "type": "file"})
            for item in gate["required_inputs"]:
                path = Path(item["path"]) if item["path"] else None
                exists = path and (path.is_dir() if item["type"] == "directory" else path.is_file())
                if not exists:
                    gate["missing_inputs"].append(item["option"] + " is missing or has the wrong file type")
            gates.append(gate)
    return {"schema": "cubeless_verification_dispatch_plan_v1", "profile": args.profile,
            "profile_kind": "legacy_pending" if legacy else "current_limited_scope", "project_root": str(root),
            "legacy_manifest_source": LEGACY["source"], "legacy_gate_count": len(LEGACY_MANIFEST), "legacy_profile_count": len(LEGACY_PROFILES),
            "fail_fast": True, "automatic_retries": 0, "engine_launch": False, "asset_mutation": False,
            "full_migration_complete": False, "gate_count": len(gates), "gates": gates,
            "report_path": str(output) if output else None, "child_directory": str(child_dir) if child_dir else None}


def snapshot_inputs(gate):
    return [{**item, "sha256": digest(item["path"]) if item["type"] == "file" else None}
            for item in gate["required_inputs"] if item["path"]]


def verify_lifecycle_link(gate):
    paths = [Path(item["path"]) for item in gate["required_inputs"]]
    author, later = map(read_json, paths)
    errors = []
    for name in ("run_id", "asset_path", "project_file"):
        if later.get(name) != author.get(name):
            errors.append("Lifecycle identity differs: " + name)
    if gate["phase"] == "reload":
        if later.get("author_report_sha256") != digest(paths[0]):
            errors.append("Reload author fingerprint does not match supplied author bytes")
        if later.get("author_editor", {}).get("instance_id") != author.get("editor", {}).get("instance_id"):
            errors.append("Reload parent editor identity differs from supplied author")
    # Cleanup originally records author hash too; reject absent/mismatched provenance.
    elif later.get("author_report_sha256") != digest(paths[0]):
        errors.append("Cleanup author fingerprint does not match supplied author bytes")
    return {"schema": "cubeless_blueprint_lifecycle_link_v1", "passed": not errors, "errors": errors}


def check_child_result(gate):
    if not gate.get("result_file"):
        return None
    payload = read_json(gate["result_file"])
    if payload.get("schema") != gate["expected_schema"]:
        raise ValueError("Verifier output schema mismatch")
    field = gate.get("expected_boolean")
    if field and payload.get(field) is not True:
        raise ValueError("Verifier output did not independently pass: " + field)
    if gate["key"].startswith("material_") and payload.get("full_material_migration_complete") is not False:
        raise ValueError("Material verifier unexpectedly enlarged its limited proof scope")
    if gate["key"] == "package_static_audit":
        if payload.get("status") != "static_pass" or payload.get("static_status") != "pass":
            raise ValueError("Package static audit is incomplete or failed")
        if payload.get("full_migration_complete") is not False or payload.get("packaging_independence_proven") is not False:
            raise ValueError("Package verifier unexpectedly enlarged its limited proof scope")
    return {"path": gate["result_file"], "sha256": digest(gate["result_file"]), "result": payload}


def run_plan(plan):
    """Execute only fixed local verifiers. Refuse all pending/missing input upfront."""
    output = Path(plan["report_path"])
    child_dir = Path(plan["child_directory"])
    for gate in plan["gates"]:
        if gate["key"] == "package_static_audit":
            stage = next((item["path"] for item in gate["required_inputs"] if item["option"] == "--stage"), None)
            if stage and (output.is_relative_to(Path(stage)) or child_dir.is_relative_to(Path(stage))):
                raise ValueError("Evidence must be outside the staged game directory; no files were created")
    if output.exists() or child_dir.exists():
        raise FileExistsError("Refusing to overwrite report or child evidence")
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("x", encoding="utf-8") as stream:
        stream.write("{}\n")  # Reserve ownership before any child can execute.
    report = {"schema": "cubeless_verification_dispatch_report_v1", "run_id": uuid.uuid4().hex,
              "started_ns": time.time_ns(), "status": "running", "exit_code": None, "plan": plan,
              "full_migration_complete": False, "legacy_gates_closed": [], "packaging_independence_proven": False,
              "engine_launched": False, "mutations_performed": False, "automatic_retries": 0,
              "dispatcher_sha256": digest(__file__), "python_executable": sys.executable, "python_version": sys.version,
              "gates": [{"key": gate["key"], "status": "not_run"} for gate in plan["gates"]], "first_failed_gate": None}

    def flush():
        temp = output.with_name(output.name + "." + report["run_id"] + ".tmp")
        with temp.open("w", encoding="utf-8") as stream:
            json.dump(report, stream, indent=2, ensure_ascii=False)
            stream.write("\n")
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temp, output)

    def finish(status, code):
        report.update(status=status, exit_code=code, ended_ns=time.time_ns())
        report["elapsed_seconds"] = (report["ended_ns"] - report["started_ns"]) / 1e9
        flush()
        return code, report

    flush()
    pending = [gate["key"] for gate in plan["gates"] if gate["kind"] == "pending_legacy_gate" or gate["missing_inputs"]]
    if pending:
        for gate, row in zip(plan["gates"], report["gates"]):
            if gate["key"] in pending:
                row.update(status="pending", reasons=gate.get("missing_inputs") or [gate["reason"]])
            else:
                row["reason"] = "Whole-profile preflight incomplete; no child executed"
        report["incomplete_gates"] = pending
        return finish("incomplete", 2)
    try:
        child_dir.mkdir(exist_ok=False)
        for gate, row in zip(plan["gates"], report["gates"]):
            row.update(status="running", started_ns=time.time_ns(), scope=gate["scope"])
            report["first_failed_gate"] = gate["key"]
            flush()
            try:
                before = snapshot_inputs(gate)
                row["inputs_before"] = before
                flush()
                if gate["kind"] == "blueprint_lifecycle_link":
                    result = verify_lifecycle_link(gate)
                    row["result"] = result
                    code = 0 if result["passed"] else 1
                else:
                    stdout_path = child_dir / (gate["key"] + ".stdout.txt")
                    stderr_path = child_dir / (gate["key"] + ".stderr.txt")
                    row.update(argv=gate["argv"], cwd=gate["cwd"], stdout=str(stdout_path), stderr=str(stderr_path))
                    # No shell, stdin interaction, retry, editor launcher, or process termination.
                    with stdout_path.open("xb") as stdout, stderr_path.open("xb") as stderr:
                        child = subprocess.run(gate["argv"], cwd=gate["cwd"], stdin=subprocess.DEVNULL,
                                               stdout=stdout, stderr=stderr, check=False)
                    code = child.returncode
                    row["child_exit_code"] = code
                    row["stdout_sha256"], row["stderr_sha256"] = digest(stdout_path), digest(stderr_path)
                    if code == 0:
                        row["independent_result"] = check_child_result(gate)
                row["inputs_after"] = snapshot_inputs(gate)
                if row["inputs_after"] != before:
                    row["error"] = "Input file changed during validation; evidence is indeterminate"
                    code = code or 1
                row.update(status="passed" if code == 0 else "failed", exit_code=code)
            except OSError as error:
                code = 127
                row.update(status="failed", exit_code=code, error=str(error), error_type=type(error).__name__)
            except Exception as error:
                code = 1
                row.update(status="failed", exit_code=code, error=str(error), error_type=type(error).__name__)
            row["ended_ns"] = time.time_ns()
            row["elapsed_seconds"] = (row["ended_ns"] - row["started_ns"]) / 1e9
            flush()
            if code:
                for later in report["gates"]:
                    if later["status"] == "not_run":
                        later["reason"] = "Stopped after first failed gate; no automatic retry"
                return finish("failed", code)
            report["first_failed_gate"] = None
        return finish("passed_limited_scope", 0)
    except KeyboardInterrupt:
        report["error"] = "Interrupted; do not treat unfinished verifier evidence as a pass"
        return finish("interrupted", 130)
    except Exception as error:
        report["error"] = str(error)
        return finish("failed", 1)


def main(argv=None):
    parser = make_parser()
    args = parser.parse_args(argv)
    if not args.plan and args.output is None:
        parser.error("--output is required for execution; use --plan for a no-write preview")
    plan = build_plan(args)
    if args.plan:
        if args.plan == "json":
            print(json.dumps(plan, ensure_ascii=False, indent=2))
        else:
            print(f"{plan['profile']}: {plan['profile_kind']}; {plan['gate_count']} gates; preview only")
            for gate in plan["gates"]:
                print(f"{gate['key']}: {gate['mapping_status']}" + ("; " + "; ".join(gate["missing_inputs"]) if gate["missing_inputs"] else ""))
        return 0
    try:
        code, report = run_plan(plan)
    except (FileExistsError, ValueError) as error:
        parser.error(str(error))
    print(json.dumps({"status": report["status"], "exit_code": code, "report": plan["report_path"],
                      "full_migration_complete": False, "first_failed_gate": report["first_failed_gate"]}))
    return code


if __name__ == "__main__":
    raise SystemExit(main())
