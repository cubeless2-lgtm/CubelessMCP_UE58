"""Apply bounded M evidence to the existing migration ledger, not completion."""
import hashlib
import json
from pathlib import Path
import time

ROOT = Path(__file__).resolve().parents[4]
HERE = Path(__file__).resolve().parent
V = ROOT / 'cubeless/records/verification/implementation-2026-10-10'
def read(path): return json.loads(path.read_text(encoding='utf-8'))
def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest()

author = read(V / 'actor-author-M.json')
cold = read(V / 'actor-reload-M3.json')
assert author['status'] == cold['status'] == 'pass'
assert read(V / 'actor-author-M-independent.json')['passed']
assert read(V / 'actor-reload-M3-independent.json')['passed']
assert read(V / 'material-long-strings-reload-M2.json')['status'] == 'pass'
names = ['get_actors_in_level', 'find_actors_by_name', 'spawn_actor', 'create_actor',
         'delete_actor', 'get_actor_properties', 'set_actor_transform']
handlers = dict(zip(['GetActorsInLevel', 'FindActorsByName', 'SpawnActor', 'DeleteActor',
                     'GetActorProperties', 'SetActorTransform'],
                    ['get_actors_in_level', 'find_actors_by_name', 'spawn_actor', 'delete_actor',
                     'get_actor_properties', 'set_actor_transform']))
ledger_path = HERE / 'migration-ledger.json'
ledger = read(ledger_path)
evidence = ['native-actor-first-slice-M.md', 'actor-pipeline-M.md'] + [
    '../../verification/implementation-2026-10-10/' + name for name in
    ['build-actor-M.log', 'registered-schemas-and-diagnostics-M.json', 'actor-map-fixture-M.json',
     'actor-author-M.json', 'actor-author-M-independent.json', 'actor-reload-M3.json',
     'actor-reload-M3-independent.json', 'actual-codex-actor-saved-read-M2.json']]
changed = []
for row in ledger['rows']:
    name = row.get('legacy_name', '') or ''
    actor = name if name in names else handlers.get(name.split('::Handle')[-1]) if 'FUnrealMCPEditorCommands::Handle' in name else None
    if actor:
        row.update(planned_disposition=1, current_disposition=4, final_disposition=None,
                   implementation='registered_and_bounded_actor_author_M_cold_M3_verified',
                   target_tools=['cubeless_mcp_ue58.registration.actor_toolset.CubelessActorTools.' + actor],
                   blocker='Full parity remains incomplete: saved single non-WP persistent level and five exact native classes only for mutations; attached/Blueprint/external/streaming actor mutations unsupported. Loaded reads exclude unloaded WP descriptors. Runtime proof pending.')
        row['verification'].update(registered='M_official_registry_7_toolsets_35_tools_actual_Codex_read',
                                  called='author_M_49_stages_cold_M3_13_stages',
                                  behavior='defaults_alias_name_vs_label_transform_delete_noop_replay_errors_independent_readbacks_pass_bounded_fixture',
                                  compile_save_reload='no_compile_requested; explicit_map_save_once; distinct_process_cold_M3_hash_unchanged',
                                  runtime_independence='not_run_for_actor_M')
        row['evidence'] = list(dict.fromkeys(row['evidence'] + evidence)); changed.append(row['id'])
    if row.get('planned_disposition') == 1 and not actor and row.get('target_tools') and any(
            token in ' '.join(row['target_tools']) for token in ('CubelessBlueprint', 'CubelessMaterial', 'CubelessBatch')):
        row['verification']['runtime_independence'] = 'bounded_saved_cohort_noMCP_Shipping_D_and_installed_Distribution_L_pass; not_all_legacy_options_or_render'
        row['evidence'] = list(dict.fromkeys(row['evidence'] + [
            '../../verification/implementation-2026-10-10/current-runtime-proof-D.json',
            '../../verification/implementation-2026-10-10/installed-current-runtime-proof-L.json']))
        if row.get('blocker') == 'compile/save/reload not yet run':
            row['blocker'] = 'Selected compile/save/cold/runtime fixtures passed; full legacy options, failure modes and all asset classes remain unverified.'
ledger['updated_ns'] = time.time_ns()
ledger_path.write_text(json.dumps(ledger, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')

baseline_path = ROOT / 'cubeless/records/migration/planning-2026-10-10/target-baseline.json'
baseline = read(baseline_path); base_root = Path(baseline['root']); differences = []
for item in baseline['files']:
    p = base_root / item['path']; now = digest(p) if p.is_file() else None
    if now != item['sha256']:
        allowed = item['path'] == 'UnrealMCP5_8_3/UnrealMCP5_8_3.uproject' and now == '51e0ddba2776a8c6297ea9da3d8ff7b9a255b5fbbc5c5a22659ad691be69ed6a'
        differences.append(dict(path=item['path'], before=item['sha256'], after=now, authorized_migration_change=allowed))
preservation = dict(valid_evidence=True, resolved_baseline_root=str(base_root),
                    protected_files_checked=len(baseline['files']), differences=differences,
                    unrelated_differences=sum(not x['authorized_migration_change'] for x in differences),
                    baseline_sha256=digest(baseline_path), finished_ns=time.time_ns(),
                    scope='Original protected disk bytes; new plugin/cubeless outputs not counted')
with (V / 'target-preservation-after-M.json').open('x', encoding='utf-8') as stream:
    json.dump(preservation, stream, indent=2)
assert preservation['unrelated_differences'] == 0
print(json.dumps({'actor_rows_updated': len(changed), 'preserved': preservation['protected_files_checked'],
                  'unrelated_differences': preservation['unrelated_differences']}))
