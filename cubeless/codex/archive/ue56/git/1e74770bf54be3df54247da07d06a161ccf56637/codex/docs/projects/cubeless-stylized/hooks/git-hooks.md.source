# Git Hooks

This project keeps versioned Git hooks in `.githooks`.

Git does not clone or share `.git/hooks` contents, so hooks that should travel with the project must live in a tracked folder. Each local clone then points Git at that folder with:

```powershell
.\Tools\GitHooks\install-hooks.ps1
```

If PowerShell blocks script execution on this PC, run the installer with a one-time bypass:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Tools\GitHooks\install-hooks.ps1
```

The installer runs:

```powershell
git config core.hooksPath .githooks
```

This setting is local to the clone. Other PCs must run the installer once after pulling the repository.

## Active Hooks

### pre-commit

Managed file: `.githooks/pre-commit`

Runs:

- `Tools/GitHooks/check_unreal_python_uv_safety.py`
- `Tools/GitHooks/check_no_project_mcp_plugin_dependency.py`

Purpose:

- Block staged Unreal Python scripts that call `StaticMeshDescription.GetVertexInstanceUV` without an obvious UV channel count guard.
- Block staged project C++/module/descriptor changes that introduce hard dependencies on the optional `UnrealMCP`/MCP plugin.

#### Unreal Python UV Safety

Why: Unreal can crash with a native assertion if `GetVertexInstanceUV` is called for a UV channel that does not exist. The known crash happened after `get_num_uv_channels(mesh, 0)` returned `1`, but the diagnostic script continued to call `GetVertexInstanceUV(..., 1)`.

Expected safe pattern:

```python
num_uv_channels = unreal.StaticMeshEditorSubsystem().get_num_uv_channels(mesh, 0)
for uv_channel in range(num_uv_channels):
    uv = mesh_description.get_vertex_instance_uv(vertex_instance_id, uv_channel)
```

If a script is intentionally safe but the hook cannot recognize it, add this comment in the file:

```python
# unreal-uv-safety: allow-getvertexinstanceuv
```

Use that override sparingly. Prefer making the channel-count guard obvious instead.

#### MCP Plugin Dependency Safety

Why: MCP and UnrealMCP are authoring aids. Cubeless project C++ APIs, module dependencies, descriptors, and finished assets must remain buildable/openable when the MCP plugin is absent unless the user explicitly requested that dependency.

The hook scans added lines in staged `.cpp`, `.h`, `.hpp`, `.hxx`, `.inl`, `.Build.cs`, `.Target.cs`, `.uplugin`, and `.uproject` files outside the `UnrealMCP` plugin path. It blocks newly introduced patterns such as `UnrealMCP` module/class references, `/Script/UnrealMCP`, and `Plugins/UnrealMCP` references.

If the dependency was explicitly requested by the user, add a line containing the token on the same added line or the immediately preceding added line:

```cpp
// cubeless-mcp-plugin-dependency: explicit-user-request
```

Use that override only for the exact user-approved MCP plugin dependency. A token elsewhere in the file does not allow unrelated MCP plugin references.
