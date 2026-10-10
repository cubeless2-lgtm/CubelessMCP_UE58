# UDS Blueprint Flow

## Inspected Assets

- `/Game/UltraDynamicSky/Blueprints/Ultra_Dynamic_Sky`
- `/Game/UltraDynamicSky/Blueprints/Ultra_Dynamic_Weather`
- active map: `/Game/UltraDynamicSky/Maps/DemoMap`

The analysis used read-only Blueprint graph inspection through UnrealMCP. It
did not edit the Blueprint assets.

## Main UDS Graph Groups

The UDS Blueprint is built from many function graphs, but the runtime sky flow
centers on these groups:

| Graph | Node count | Role |
| --- | ---: | --- |
| `Startup Sky` | 103 | Full initialization and first update pipeline |
| `Update Static Variables` | 38 | Component/static property refresh |
| `Update Active Variables` | 54 | Main active update dispatcher fan-out |
| `Cache Properties` | 460 | Expensive property caching and apply binding |
| `Monitor for Changes` | 88 | Detects active/weather/time changes |
| `Update Cloud Movement` | 59 | Per-frame interpolation of cached cloud offsets |
| `Bind Events to Tick` | 76 | Runtime tick delegate binding |
| `Current Volumetric Clouds Density` | 20 | Derives the semantic cloud density value |
| `Update Cloud Coverage Material Parameters` | 23 | Writes cloud density to the sky MID and MPC |
| `Apply Volumetric Mode` | 11 | Applies volumetric render target console variables |

## Startup Sky

`Startup Sky` is the root orchestration graph. It has visible comment regions
for:

- configuration reset
- UDW actor discovery
- static properties
- starting time of day and cloud formation
- camera location and occlusion
- sky modifiers
- weather connection
- cloud painting
- first active update
- runtime-only startup
- editor-only refresh

The useful high-level order is:

1. Revert/clear runtime data and apply configuration override.
2. Query scalability, project settings, and UDS version.
3. Find the UDW actor.
4. Run `Update Static Variables`.
5. Initialize time/date and starting cloud formation.
6. Initialize occlusion and camera-dependent state.
7. Connect to UDW and apply weather-controlled variables.
8. Start cloud paint manager and painted coverage target updates.
9. Run `Update Common Derivatives`, size cache arrays, and `Cache Properties`.
10. Run the first `Update Active Variables`.
11. Bind runtime tick events and start cloud movement cache updates.

This explains why a single material copy cannot reproduce the UDS look. The
material expects a fully initialized Blueprint/MPC/runtime cache state.

## Active Update Dispatcher Model

`Update Active Variables` always calls:

- `Update Cloud Coverage Material Parameters`
- `Max Priority Updates`

Then it spreads work across high-priority and low-priority delegate groups:

- `High Priority Updates 1..4`
- `Low Priority Updates 1..8`

The graph uses `Active Update Speed`, `High Priority Update Step`, and
`Low Priority Set Toggle` to distribute update cost. The design is a useful
pattern for Cubeless: update small semantic groups every tick, but rotate
expensive or low-priority groups across frames.

## Tick Binding

`Bind Events to Tick` starts by clearing existing `Runtime Tick` bindings, then
binds runtime update groups based on project mode and enabled features.

Important gates and regions:

- dedicated server path
- `Disable All Runtime Updating`
- UDW-controlled variables
- time-of-day control
- player occlusion
- time-of-day specific modifiers
- cache updates
- cinematic runtime update
- apply changes
- path tracer fog update

This means UDS runtime behavior is not just `Tick -> update everything`. It is
a set of event dispatcher bindings assembled at startup.

## UDW Runtime Flow

Important UDW graphs include:

| Graph | Node count | Role |
| --- | ---: | --- |
| `Start Weather System` | 81 | Weather startup and object/state initialization |
| `Update Active Variables` | 7 | Weather active update dispatcher entry |
| `Update Static Variables` | 24 | Static weather properties |
| `Update Current Global And Local Weather State` | 33 | Global/local weather composite |
| `Monitor Local Weather Changes` | 56 | Detects local weather state changes |
| `Update Material Effect Parameters` | 28 | Weather material effects/MPC write path |
| `Set Shared Weather Particle Parameters` | 80 | Niagara shared particle parameters |
| `Sparse Movement Updates` | 18 | Weather/cloud movement update support |

UDW owns weather state, local override volumes, material wetness/snow/dust,
weather particles, sound, lightning, WOV, and DLWE. UDS consumes weather values
from UDW through variables controlled by weather and shared MPCs.

## Key Runtime Couplings

- UDS owns sky, atmosphere, fog, lights, sky sphere, volumetric cloud component,
  cloud movement, and sky material MIDs.
- UDW owns weather state and pushes cloud coverage, wind, fog, rain/snow/dust,
  local weather, particles, sound, and material effects.
- `UltraDynamicWeather_Parameters` carries weather-wide semantic state.
- `UDS_VolumetricClouds_MPC` carries volumetric-cloud semantic state.
- The sky sphere MID carries sky-dome and static/2D cloud parameters that are
  not equivalent to the volumetric MPC.

## Implementation Lesson

For Cubeless, copy the architecture, not the vendor Blueprint bulk:

1. Keep a small Cubeless sky state object.
2. Derive semantic values from weather/time once.
3. Write those values to Cubeless-owned MPCs and MIDs.
4. Rotate expensive updates across frames.
5. Keep visual branches explicit: sky dome, static clouds, 2D clouds,
   volumetric clouds, fog, weather particles, and sound.
