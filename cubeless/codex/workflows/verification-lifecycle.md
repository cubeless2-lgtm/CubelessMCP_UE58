# Owned verification Editor lifecycle

This project is the development and verification target. Use a separate project only at the user's explicit direction. A preexisting Editor belongs to the user throughout verification.

## Reuse a running Editor first

1. Identify the target `.uproject` and inspect live UnrealEditor processes, including Win64 configuration suffixes (such as DebugGame) and Cmd variants, and their listening ports. On Windows use `Get-CimInstance Win32_Process` and `Get-NetTCPConnection -State Listen` on the host to match the listener's owning PID. If the sandbox cannot see host processes, use the normal read-only approval path. If an Editor is starting, wait for that PID within a bounded deadline. An inaccessible inventory, an unknown owner, a different project, or a failed MCP probe requires reconciliation before any launch.
2. Probe the observed local endpoint using the expected project and PID. From the project root, use the following command with the actual `$project`, `$endpoint`, `$editorPid`, and a fresh `$probeReceipt` under `cubeless/records/verification/`.

```powershell
python -B cubeless/tools/official_mcp_client.py --endpoint $endpoint --expected-project $project --expected-pid $editorPid --output $probeReceipt
```

3. The probe closes its own MCP session after recording diagnostics. Before sending any writes, open the actual working session with the same expected project and PID and verify identity again in that session. For production pipelines, pass `--expected-pid $editorPid`; their shared Session checks identity before admitting domain work. For direct Python callers, construct `OfficialMCPClient(endpoint, expected_project=project, expected_pid=editor_pid)`, call `initialize()`, then `verify_editor_identity()`, and keep that client for the work. Guarded mutations require verification again after close or reinitialization. HTTP redirects are refused. Check dirty packages, PIE/SIE and pending work before any write. Identity verification grants no ownership or shutdown permission. Discovery without the identity arguments only inventories tools.

## Start only when needed

When no Editor is running and the chosen port is free, request launch approval through the active execution policy as required. Identify the exact engine executable, project and port in the request. Use a fresh `$launchReceipt` in an existing directory under `cubeless/records/verification/`. Run this as a separate launch-only action:

```powershell
& ./cubeless/tools/start_verification_editor.ps1 -EditorExe $editorExe -Project $project -Port $port -Receipt $launchReceipt
```

The launcher checks Engine/Build/Build.version for UE 5.8.x before starting any process, then checks the inventory again and records the actual PID and UTC start time. It opens the engine Entry map in unattended mode to suppress interactive startup questions such as the Zen maintenance wait prompt. A launched receipt confirms process creation only. Wait for that PID to own the selected listener, then run the read-only probe above with the recorded PID and a fresh probe receipt. Track launch refusal, process exit, MCP initialization/discovery failure and identity mismatch separately. If a call times out or the receipt is incomplete, inspect the process, listener and Editor log before deciding what happened. Never start a second Editor as a connection retry.

## Finish an owned verification session

Do not use `-ExecutePythonScript` plus unconditional keep-alive as an unattended indefinite session. A one-shot script should finish without keep-alive. A multi-stage verification session may keep alive only while its owning runner is active. That runner must have a bounded execution deadline and a finally cleanup path after evidence publication. A timeout does not authorize force-kill or uncertain mutation replay.

On success, failure or interruption: stop issuing new writes, finish/cancel owned jobs and inspect actual state. Record unresolved jobs and partial/unknown outcomes. Check exact PID, project, PIE/SIE and dirty packages immediately before shutdown. If there is dirty/unknown work, preserve the session, record PID/reason/next action and notify the user. Never save or discard automatically merely to make cleanup pass.

For an owned `-ExecutePythonScript` keep-alive Editor, the external helper `cubeless/tools/finish_verification_editor.py` accepts only isolated projects below `cubeless/workspaces`; do not use it against this project or bypass its checks. For an owned Editor in this project, prefer a one-shot script that exits normally, or use its normal window close after reconciling jobs and checking the exact PID, project, PIE/SIE and dirty packages. Never close the user's preexisting Editor. For a qualifying isolated project, invoke the helper with explicit `--endpoint`, `--expected-project`, `--expected-pid` and fresh `--output`; it rediscovers the Python tool, checks identity and clean state, then clears `set_keep_python_script_alive(False)`. It is not a general process killer.

Caller must confirm that exact PID exits and inspect the shutdown log. An HTTP disconnect can be an expected exit but is not proof; inspect process state without blindly retrying the mutation. For an Editor launched without a keep-alive script, close its owned window normally after the same checks. A hidden Editor can have a zero .NET `MainWindowHandle`. In that case match the receipt PID and UTC start time, identify its unique project main window, and send normal `WM_CLOSE` only to that window. Preserve the process if identity or cleanliness is uncertain. Record cleanup as complete only after actual exit.

Historical bootstraps/reports stay immutable. Do not rerun them as current launch templates. Carry this lifecycle into each new verification runner rather than patching archived experiments en masse.

Ship this workflow with `cubeless/tools/start_verification_editor.ps1`, `official_mcp_client.py` and `finish_verification_editor.py`. The isolated-project finish helper requires a running official MCP endpoint with Cubeless diagnostics and Python execution tools. Establish ownership from the launch record, reconcile pending work before cleanup and confirm process exit separately.
