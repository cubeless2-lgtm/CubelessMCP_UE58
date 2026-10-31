# Owned verification Editor lifecycle

Before launch, record the exact isolated project, owned process PID/start time, endpoint, bootstrap and log. Never claim ownership of a preexisting user/main Editor or use process-name-wide termination. Main projects are outside cleanup scope.

Do not use `-ExecutePythonScript` plus unconditional keep-alive as an unattended indefinite session. A one-shot script should finish without keep-alive. A multi-stage verification session may keep alive only while its owning runner is active. That runner must have a bounded execution deadline and a finally cleanup path after evidence publication. A timeout does not authorize force-kill or uncertain mutation replay.

On success, failure or interruption: stop issuing new writes, finish/cancel owned jobs and inspect actual state. Record unresolved jobs and partial/unknown outcomes. Check exact PID, project, PIE/SIE and dirty packages immediately before shutdown. If there is dirty/unknown work, preserve the session, record PID/reason/next action and notify the user. Never save or discard automatically merely to make cleanup pass.

For an owned `-ExecutePythonScript` keep-alive Editor, use the external helper `cubeless/tools/finish_verification_editor.py` with explicit `--endpoint`, `--expected-project`, `--expected-pid` and fresh `--output`. It only accepts isolated projects below `cubeless/workspaces`, rediscovers the Python tool, checks identity and clean state inside the target Editor, then clears `set_keep_python_script_alive(False)`. Installed UE5.8 EditorPythonExecuter requests normal Editor exit on its next tick. The helper is not a general process killer and must not be used before owned pending work is reconciled.

Caller must confirm that exact PID exits and inspect the shutdown log. An HTTP disconnect can be an expected exit but is not proof; inspect process state without blindly retrying the mutation. For an Editor launched without a keep-alive script, use its normal owned window close after the same checks, not a force kill. Preserve other Editor PIDs. Record cleanup as complete only after actual exit.

Historical bootstraps/reports stay immutable. Do not rerun them as current launch templates. Carry this lifecycle into each new verification runner rather than patching archived experiments en masse.

Distribution: this workflow and `cubeless/tools/finish_verification_editor.py` must be included together in the source bundle. The helper uses the bundled stdlib-only `official_mcp_client.py`; it starts no server and introduces no game runtime dependency. It requires a running official MCP endpoint with Cubeless diagnostics and Python execution tools available. The caller must establish ownership from the launch record; a matching PID/project and keep-alive flag alone do not establish ownership. Reconcile pending work before invocation, and confirm process exit separately afterward.
