# Runtime Flow

## UDS Startup And Update Model

UDS is organized around a small set of large Blueprint functions:

- `Startup Sky`
  - Shared startup path for construction script, BeginPlay, runtime restart,
    configuration changes, and sky-mode changes.
  - Finds UDW, applies static variables, initializes time, weather connection,
    camera/occlusion, cloud paint, cache arrays, bindings, first active update,
    and cloud movement.
- `Update Static Variables`
  - Applies properties that are not expected to change every frame.
  - Runs during construction/startup and when manually restarted.
- `Update Active Variables`
  - Runs frequently during runtime.
  - Calls high/low-priority dispatcher groups instead of recalculating every
    expensive property every frame.
  - Directly calls `Update Cloud Coverage Material Parameters` before the
    priority dispatcher fan-out.
- `Cache Properties`
  - Spreads expensive calculations over update periods.
  - Caches old/new values and binds `AP - ...` apply functions to dispatchers.
- `Monitor for Changes`
  - Watches time/weather change speed and adjusts cache/update timing.

The important idea: UDS separates "derive values" from "apply values".
Derived values are cached or calculated in helper functions. Apply functions
push those values into components, MIDs, and MPCs.

## Tick Model

The event graph:

- `BeginPlay` and custom `Force Startup` call `Startup Sky`.
- `Tick` stores `Tick Delta Seconds`.
- Tick calls UDW runtime tick when a weather actor exists.
- Tick calls UDS `Runtime Tick` dispatchers.
- Tick calls `Update Cloud Movement`.
- There is a half-rate tick path controlled by `Half Rate Tick` and threshold
  settings.

`Bind Events to Tick` clears old `Runtime Tick` bindings, then binds update
groups according to project mode and enabled features. This is why UDS can look
correct after startup but later drift or stall if one bound update path is
gated by a state value such as weather change speed.

## Time And Lighting

Time is primarily represented by `Time of Day` and Date variables. Sun/moon
rotation may be derived from time/date/location through simulation functions,
unless manual positioning is used.

The main path:

1. Update time/date.
2. Cache sun/moon orientation.
3. Use sun/moon forward vectors to drive:
   - sky color
   - fog color
   - skylight color/intensity
   - sun/moon disk material values
   - cloud lighting vectors
   - static cloud lighting masks
   - volumetric scattering/emissive values

## Cloud Movement

UDS does not simply recalculate every cloud coordinate every frame.

- `Increment Cloud Movement Cache`
  - Runs a few times per second by default.
  - Computes new cloud/fog offsets from speed, wind direction, cloud phase, and
    time-of-day movement.
- `Update Cloud Movement`
  - Runs on tick.
  - Interpolates between cached old/new positions.
  - Writes values such as `Clouds Position`, `Clouds B Time`, and `Fog Position`
    into `UDS_VolumetricClouds_MPC`.

For Cubeless, this is a useful pattern: calculate cloud movement sparsely, then
interpolate lightweight MPC values every frame.

The inspected DemoMap used:

- `Cloud Movement Update Period`: `0.55`
- `Cloud Speed`: `0.35`
- UDW `Sky Cloud Speed()`: about `0.0735427785`

## UDW Weather Flow

UDW mirrors the UDS architecture:

- `Start Weather System`
  - Creates weather settings/state objects and starts initial systems.
- `Update Current Global And Local Weather State`
  - Builds global weather and local weather state.
- `Monitor Local Weather Changes`
  - Detects changed weather values and binds relevant updater functions.
- `Update Active Variables`
  - Applies weather state into Niagara, sound, material effects, and MPCs.
- `Update Static Variables`
  - Applies weather properties that do not normally change during runtime.

Global weather comes from the UDW actor/preset/random-weather transition.
Local weather is the player-location composite, including weather override
volumes, radial storms, and above-cloud conditions.

## Weather Output Targets

UDW drives:

- `UltraDynamicWeather_Parameters` MPC.
- Niagara rain/snow/dust/wind debris/lightning components.
- Weather material state, wetness, dust, snow, and puddle effects.
- Weather mask and WOV render targets.
- Global and directional weather sounds.
- Lightning flash light and post-process bloom.

## Occlusion And Sound

UDS owns the player occlusion component. UDW uses the occlusion values for sound
and weather perception.

The readme describes two weather sound sources:

- Global sound source: not spatialized.
- Directional sound source: spatialized and directionally occluded.

Both route through `UDS_Weather_AudioBus`, then through the UDW sound mixer that
applies panning and occlusion.
