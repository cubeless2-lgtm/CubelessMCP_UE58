# MCP Follow-Ups

The user approved MCP C++/API changes if analysis is blocked and no workaround
exists. For this pass, workarounds were enough, so these are listed only.

UnrealMCP already has useful read-only Blueprint graph tools:

- `list_blueprint_graphs`
- `list_blueprint_nodes`
- `find_blueprint_nodes`

They were enough to confirm the UDS cloud-density gate without editing UDS
Blueprint assets.

## High Value

### Blueprint Graph Filtering

`list_blueprint_graphs` returns very large results for UDS/UDW. Add optional
filters:

- `name_contains`
- `max_graphs`
- `include_node_counts`
- `category_contains` if category metadata is reachable

This would avoid huge graph dumps for assets like `Ultra_Dynamic_Sky`.

### Blueprint Function Call Helper

Python `actor.call_method("Function Name")` worked for UDS Blueprint functions.
Expose a safe MCP command:

- actor name/path/label
- function name
- optional args
- return value
- dirty package report

This would make read-only or low-risk Blueprint diagnostics much cleaner.

### Runtime UDS Cloud Repair Helper

Add a non-vendor helper command or script:

1. Find current UDS actor.
2. Confirm `Sky Mode == VOLUMETRIC_CLOUDS`.
3. Call `Current Volumetric Clouds Density`.
4. Write that value to runtime `UDS_VolumetricClouds_MPC.Cloud Density`.
5. Keep `set_asset_defaults=false` and `save=false`.
6. Check `r.VolumetricCloud`, `ShowFlag.VolumetricClouds`, and
   `r.VolumetricRenderTarget`.
7. Report dirty map/package state.

This should live in Cubeless/MCP tooling, not in UDS content.

## Medium Value

### MPC Read/Write Result Improvements

`set_material_parameter_collection_values` already reports asset and runtime
updates well. Keep it. A small improvement would be to include a stable
`dirty_map_count` alongside `dirty_after_set`, because runtime/MID changes may
dirty a loaded map without dirtying the MPC asset.

### Material Parameter Node Summaries

`list_material_nodes` can be noisy for parameter nodes. Add summarized fields for
common expression classes:

- scalar/vector/texture parameter name
- default value
- group
- sort priority
- collection path for collection parameters

`list_material_collection_parameter_nodes` already solves the MPC-specific case.

### Screenshot Dirty-State Explanation

`capture_viewport_bookmark_screenshot` correctly reports dirty packages before
and after capture. Add a field clarifying whether the screenshot command itself
changed dirty state. It already reports added/removed counts; a simple boolean
would make it easier to read in long workflows.

## Low Value

### Actor Property Snapshot

`get_actor_properties` currently returns only basic transform/class data for
Blueprint actors. Add an optional reflected editable property snapshot with:

- include/exclude name filters
- max properties
- display name and internal name where possible
- read-only/editable flags

This would have made UDS property triage faster, but Python direct inspection
was enough for this pass.
