"""Release an owned -ExecutePythonScript verification Editor; never force kill/save."""
import argparse
import json
from pathlib import Path
from official_mcp_client import OfficialMCPClient, tool_payload


def shutdown_code(project, pid):
    return "\n".join([
        "import unreal, json, os",
        "from pathlib import Path",
        "d = json.loads(unreal.CubelessNativeToolset.get_editor_diagnostics())",
        "assert d.get('success') is True, 'Diagnostics failed'",
        f"assert os.getpid() == {pid} and d.get('process_id') == {pid}, 'PID mismatch'",
        f"assert Path(d['project_file']).resolve() == Path({str(project)!r}).resolve(), 'Project mismatch'",
        "assert d.get('dirty_packages') == [], 'Unsaved packages: preserve Editor and report'",
        "assert d.get('pie_or_sie_active') is False, 'PIE/SIE active'",
        "assert unreal.EditorPythonScripting.get_keep_python_script_alive(), 'Not an owned keep-alive script session'",
        "print('VERIFICATION_FINISH ' + json.dumps(d))",
        "unreal.EditorPythonScripting.set_keep_python_script_alive(False)",
    ])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--endpoint', required=True)
    parser.add_argument('--expected-project', required=True, type=Path)
    parser.add_argument('--expected-pid', required=True, type=int)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    owned = Path(__file__).resolve().parents[1] / 'workspaces'
    project = args.expected_project.resolve()
    if not project.is_relative_to(owned.resolve()) or project.suffix != '.uproject':
        parser.error('Only isolated cubeless/workspaces verification projects are allowed')
    if args.expected_pid <= 0 or args.output.exists():
        parser.error('Positive owned PID and fresh output required')
    client = OfficialMCPClient(args.endpoint, 20)
    report = {'expected_project': str(project), 'expected_pid': args.expected_pid,
              'status': 'not_requested', 'process_exit': 'must_confirm_externally'}
    try:
        client.initialize()
        description = tool_payload(client.call('describe_toolset', {'toolset_name': 'CubelessMCPUE58Editor.CubelessPythonExecutionTools'}))
        assert any(t['name'].endswith('.execute_unreal_python') for t in description['tools'])
        report['status'] = 'request_sent'
        report['result'] = tool_payload(client.call_toolset('CubelessMCPUE58Editor.CubelessPythonExecutionTools', 'execute_unreal_python',
            {'code': shutdown_code(project, args.expected_pid), 'mode': 'ExecuteFile', 'interchange_policy': 'none'}, mutation=True))
        report['status'] = 'release_requested' if report['result'].get('python_execution_success') is True else 'refused_or_failed'
    except Exception as exc:
        report['error'] = str(exc)
        report['status'] = 'unknown_check_process_no_retry' if report['status'] == 'request_sent' else 'preflight_failed'
    finally:
        client.close()
        args.output.parent.mkdir(parents=True, exist_ok=True)
        with args.output.open('x', encoding='utf-8') as f:
            json.dump(report, f, indent=2)
    print(json.dumps({'status': report['status'], 'output': str(args.output)}))


if __name__ == '__main__':
    main()
