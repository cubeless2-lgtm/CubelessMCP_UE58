# Mutation safety

Confirm target project, engine build, PIE/SIE and dirty package state before writing. Protect the user's open session; favor an isolated verification project. Create test assets beneath a unique `/Game/_MCP_Temp/<feature>/CubelessTests/<unique-run>/` root and reject existing target names unless an explicitly supported idempotent operation proves ownership.

Serialize asset mutations. Batch related edits, compile once after a batch and save only affected packages after successful checks. Do not run arbitrary UObject work on background threads. Engine async APIs require explicit status, cooperative cancellation and safe completion on the required thread.

Record preconditions and each completed stage. On timeout, disconnect or ambiguous result, mark outcome unknown and stop mutation retries. Reconnect and inspect target state before deciding whether a new operation is safe. Cancellation acknowledgement is not proof the engine stopped. Report partial modifications even when the request fails.

Cleanup is a separate operation restricted to the exact run's test assets. It must preserve reports and must never hide the original failure. Asset creation is not transactional rollback. Do not describe transactions or Undo as all-or-nothing protection.
