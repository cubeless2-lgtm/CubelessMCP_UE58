# Blueprint Index

This file is a navigation index for the two large UDS Blueprints. It is not a
full node dump. Use it to decide where to inspect next.

Inspected Blueprints:

- `/Game/UltraDynamicSky/Blueprints/Ultra_Dynamic_Sky`
- `/Game/UltraDynamicSky/Blueprints/Ultra_Dynamic_Weather`

The analysis used read-only UnrealMCP graph inspection.

## UDS Runtime Entry Points

`Ultra_Dynamic_Sky` event graphs:

| Graph | Nodes | Role |
| --- | ---: | --- |
| `EventGraph` | `36` | BeginPlay, Force Startup, Tick, half-rate tick dispatch |
| `End Play` | `6` | Shutdown cleanup |
| `Interface` | `3` | Interface response stubs |
| `Notify Events` | `67` | Editor/runtime notification events |

Important `EventGraph` nodes:

- `BeginPlay Event`
- `Force Startup` custom event
- `Startup Sky` function call
- `Tick Event`
- `Update Cloud Movement` function call
- `UDW Runtime Tick` message calls
- `Call Runtime Tick` delegate calls
- `Switch Has Authority`
- `Client Check Initial Replication`

The useful mental model:

1. BeginPlay or Force Startup enters `Startup Sky`.
2. Startup initializes static state, active state, UDW references, and tick
   binding.
3. Tick updates cloud movement and calls UDW runtime tick messages/delegates.
4. Most visual work happens through functions bound to UDS update delegates,
   not directly inside the small EventGraph.

## UDS Delegate Groups

UDS time/event delegates:

- `Sunset`
- `Sunrise`
- `Midnight`
- `Hourly`
- `Every Minute`
- `Current Hour Changed`
- `Date Changed`
- `Custom Time`
- `Instant Time of Day Change`
- `Time Transition Complete`

UDS runtime update delegates:

- `Runtime Tick`
- `Max Priority Updates`
- `High Priority Updates 1..4`
- `Low Priority Updates 1..8`
- `Immediate and Unrepeated Updates`

UDS player occlusion delegates:

- `Finished Player Occlusion Cycle`
- `Player Occlusion Hard Update`

The priority delegate layout is one of the major UDS architecture lessons.
Expensive or less urgent updates are not all run directly on Tick. Cubeless
should copy that scheduling concept, not the large Blueprint body.

## UDS Function Families

### Startup And Update

Key functions:

- `Startup Sky`
- `Update Static Variables`
- `Update Active Variables`
- `Monitor for Changes`
- `Bind Events to Tick`
- `Second Frame Startup Functions`
- `Query Project Settings And UDS Version`

These are the entry points to understand initialization and update ordering.

### Time, Sun, Moon, Stars

Key functions:

- `Approximate Real Sun Moon and Stars`
- `Set Date and Time`
- `Transition Time of Day`
- `Tick Time Transition`
- `Find Real Sunset/Sunrise Times`
- `Current Sun Disk Intensity`
- `Current Sun Disk Color`
- `Current Sun Light Color`
- `Current Moon Light Color`
- `Current Moon Phase Angle`
- `Current Moon Lit Percent`
- `Get Current Sky Light Color and Intensity`
- `Current Stars Color`

This family drives lighting vectors, sky color, moon phase, and sky light.

### Volumetric Clouds

Key functions:

- `Apply Volumetric Mode`
- `Current Volumetric Clouds Density`
- `Get Current Volumetric Cloud Extinction Scale`
- `Current Volumetric Cloud Macro Variation`
- `Current Volumetric Cloud Albedo`
- `Current Volumetric Cloud Multiscattering Occlusion`
- `Current Volumetric Multiscattering Phase 1`
- `Volumetric Cloud Base Cloud Height`
- `Volumetric Cloud Layer Height`
- `Volumetric Cloud First Layer Top Altitude`
- `Volumetric Cloud Layer Scale`
- `Volumetric Cloud Floor Variation`
- `Volumetric Clouds SubNoise Scales`
- `Volumetric Clouds Parent Materials`
- `All Volumetric Cloud MIDs`
- `Get Volumetric Cloud Emissive Colors`
- `Cloud Shadows Cloud Density`
- `Cloud Shadows Light Vector And Cancel Value`

The currently fixed failure is in this family: `Update Cloud Coverage Material
Parameters` had a valid derived density but did not write the runtime MPC while
`Composite Weather Change Speed` was zero.

### Static And 2D Clouds

Key functions:

- `Starting Cloud Formation`
- `Static Clouds Lighting Mask`
- `Static Clouds Current Dynamic Rotation`
- `Current 2D Cloud Tint`
- `Current Wisps Opacity`
- `AP - Static Clouds Lighting Mask`
- `AP - Static Clouds Tint Color`
- `AP - Static Clouds Shadow Color`
- `AP - 2D Cloud Tint`
- `AP - Shine Intensity`
- `AP - Sun Highlight Radius`
- `AP - Sun Highlight Intensity`
- `AP - Shading Offset Vector`
- `AP - Painted Cloud Coverage Opacity Layer 1`
- `AP - Painted Cloud Coverage Opacity Layer 2`

`AP - ...` functions are thin apply-property binding functions. They are useful
for tracing where a value is written, but the interesting calculation usually
lives in the corresponding `Current ...` or static-property function.

### Fog, Atmosphere, And Post Process

Key functions:

- `Current Fog Density`
- `Fog Height Falloff`
- `Fog Start Distance`
- `Set Current Fog Base Colors`
- `Current Fog Inscattering Color`
- `Current Fog Directional Inscattering Color`
- `Current Sky Ambient Color`
- `Sky Atmosphere Fog Contribution`
- `Current Sky Atmosphere Luminance`
- `Cache Post Process Blend Weights`
- `AP - Post Process Blend Weights`

These matter if Cubeless wants UDS-like fog color and atmospheric response.

### Cache And Runtime Config

Key functions/macros:

- `Cache Properties`
- `Hard Reset Cache`
- `Cache Float`
- `Get Cached Float`
- `Cache Color`
- `Get Cached Color`
- `Cache Sun and Moon Orientation`
- `Size Cache Arrays`
- `Modifier Float Reference`
- `Modifier Color Reference`
- `Runtime Config Bool Reference`
- `Runtime Config Float Reference`
- `Runtime Config Color Reference`
- `Runtime Config Hard Object Reference`
- `Runtime Config Soft Object Reference`

This family explains why UDS can be difficult to reason about from asset
defaults alone. Many values are cached, modified, or bound to runtime config
before they reach MPCs and MIDs.

## UDW Runtime Entry Points

`Ultra_Dynamic_Weather` event graphs:

| Graph | Nodes | Role |
| --- | ---: | --- |
| `EventGraph` | `31` | Initialize weather, BeginPlay, runtime tick |
| `Lightning Flash` | `36` | Lightning event entry |
| `Weather Change Event` | `55` | Weather transition event |
| `Event End Play` | `9` | Shutdown cleanup |
| `Latent Events` | `52` | Latent timers and deferred work |
| `Notify Events` | `34` | Notification events |

Important `EventGraph` nodes:

- `Initialize Weather` interface event
- `Set UltraDynamicSky`
- `BeginPlay Event`
- `Set UDS Reference`
- `Start Up UDW if It Exists`
- `Start Weather System`
- `UDW Runtime Tick` interface event
- `Force Tick` custom event
- `Call Runtime Tick` delegate

The useful mental model:

1. UDW stores the UDS reference.
2. UDW can connect to an already running UDS.
3. `Start Weather System` constructs and initializes weather state.
4. Runtime tick fans out through UDW delegates and extra-feature updates.

## UDW Delegate Groups

Weather transition delegates:

- `Started Raining`
- `Finished Raining`
- `Started Snowing`
- `Finished Snowing`
- `Getting Cloudy`
- `Clouds Clearing`
- `Getting Foggy`
- `Fog Clearing`
- `Dust/Sand Forming`
- `Dust/Sand Clearing`
- `Instant State Change`

State-change delegates:

- `State Change - Rain`
- `State Change - Snow`
- `State Change - Wind Intensity`
- `State Change - Dust`
- `State Change - Fog`
- `State Change - Thunder/Lightning`
- `State Change - Wind Direction`
- `State Change - Cloud Coverage`
- `State Change - Material Wetness`
- `State Change - Material Snow`
- `State Change - Material Dust`

System delegates:

- `Runtime Tick`
- `Construct Global Weather`
- `Construct Local Weather`
- `Apply Active Updates`
- `Extra Feature Updates`
- `Season Changed`
- `Weather Display Name Changed`
- `Temperature Range Update`
- `Random Weather Season Refresh`
- `Lightning Flash Started`

For Cubeless, the most important weather delegate is
`State Change - Cloud Coverage`. It is the clean boundary between weather state
and sky cloud response.

## UDW Function Families

### Startup And State

Key functions:

- `Start Weather System`
- `Construct All Weather State Objects`
- `Construct Global Weather`
- `Construct Local Weather`
- `Update Current Global And Local Weather State`
- `Make Manual State`
- `Set Entire Manual Weather State`
- `UDW State Apply`
- `UDS Weather Variable Overrides`
- `Get UDS Values Controlled by UDW`

### Cloud And Weather Values

Key functions:

- `Currently Cloudy`
- `Currently Raining`
- `Currently Snowing`
- `Currently Foggy`
- `Currently Dusty`
- `Sky Cloud Speed`
- `Get Weather Speed`
- `Wind Rotation`
- `Wind Force Vector`
- `Get Normalized Wind Direction`

Important trap from the current map: `Currently Cloudy` is false while
`Cloud Coverage` is `3.8`. Treat these boolean helpers as display/event state,
not as the source of truth for rendered cloud density.

### Material Effects And Particles

Key functions:

- `Update Material Effect Parameters`
- `Set Shared Weather Particle Parameters`
- `Static Properties - Material Effects`
- `Static Properties - Shared Particles`
- `Static Properties - Rain`
- `Static Properties - Snow`
- `Static Properties - Dust`
- `Static Properties - Lightning`
- `Make Rain Component`
- `Make Dust Component`
- `Rain Spawn Rate`
- `Snow Spawn Rate`
- `Dust Spawn Rate Scale`

These can be deferred in Cubeless until the sky itself is stable.

### Weather Masks, Local Weather, And DLWE

Key functions:

- `Monitor Local Weather Changes`
- `Bind Local State Update Functions`
- `Local State Update - Changes Above Cloud Layer`
- `Construct Weather Mask Target State`
- `Update Weather Mask Target`
- `Construct WOV Render Target State`
- `Update WOV Render Target`
- `Static Properties - Weather Occlusion Volume`
- `EF - Weather Occlusion Volume Update`
- `Start Up DLWE Interaction System`
- `Check Point for Puddles Snow Or Dust`

These are powerful but broad. They should stay outside the first Cubeless sky
port unless local weather volumes or ground weather are specifically required.

## MCP Note

UnrealMCP can list graphs and nodes well enough for read-only analysis, but
direct Blueprint variable introspection is still limited in the current tool
surface. For now, prefer graph/function indexing plus targeted actor property
reads for specific variables.
