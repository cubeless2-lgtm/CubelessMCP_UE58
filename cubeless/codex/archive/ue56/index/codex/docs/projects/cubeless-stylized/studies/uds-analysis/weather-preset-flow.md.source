# Weather Preset Flow

UDW is the weather-side companion to `Ultra_Dynamic_Sky`. It is not just rain
particles. It owns weather preset state, blends global and local weather, writes
the weather MPC, and pushes cloud-related values into UDS.

## Current DemoMap Actors

Current inspected actors:

- UDS:
  `/Game/UltraDynamicSky/Blueprints/Ultra_Dynamic_Sky.Ultra_Dynamic_Sky_C`
- UDW:
  `/Game/UltraDynamicSky/Blueprints/Ultra_Dynamic_Weather.Ultra_Dynamic_Weather_C`

Current UDS values:

| Field | Value |
| --- | ---: |
| `Sky Mode` | `VOLUMETRIC_CLOUDS` |
| `Feature Level` | `DESKTOP_CONSOLE` |
| `Project Mode` | `GAME_REAL_TIME` |
| `Cloud Coverage` | `3.799999952316284` |
| `Cloud Coverage 0-3` | `1.1399999856948853` |
| `Cloud Speed` | `0.35` |
| `Cloud Direction` | `180.0` |
| `Composite Weather Change Speed` | `0.0` |
| `Using Volumetric Clouds` | `true` |
| `Two Layers` | `false` |
| `Current Volumetric Clouds Density()` | `1.310999983549118` |

Current UDW values:

| Field | Value |
| --- | ---: |
| `Weather Speed` | `1.0` |
| `Cloud Coverage` | `3.799999952316284` |
| `Fog` | `1.0` |
| `Rain` | `0.0` |
| `Snow` | `0.0` |
| `Dust` | `0.0` |
| `Wind Intensity` | `2.0` |
| `Sky Cloud Speed()` | `0.0735427785233055` |
| `Get Weather Speed()` | `1.0` |
| `Currently Cloudy()` | `false` |
| `Currently Raining()` | `false` |
| `Currently Snowing()` | `false` |
| `Currently Foggy()` | `false` |

Important trap: the boolean helpers such as `Currently Cloudy()` are not the
source of truth for visible cloud density. In the inspected state,
`Currently Cloudy()` is false while `Cloud Coverage` is `3.8` and the
volumetric cloud density function returns about `1.311`.

## Weather Preset Assets

Weather presets live under:

`/Game/UltraDynamicSky/Blueprints/Weather_Effects/Weather_Presets`

They are `UDS_Weather_Settings_C` data assets/Blueprint instances.

Inspected presets:

| Preset | Cloud | Fog | Rain | Snow | Dust | Wind |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `Clear_Skies` | `0.0` | `0.0` | `0.0` | `0.0` | `0.0` | `2.0` |
| `Partly_Cloudy` | `3.8` | `1.0` | `0.0` | `0.0` | `0.0` | `2.0` |
| `Cloudy` | `5.0` | `1.0` | `0.0` | `0.0` | `0.0` | `2.5` |
| `Overcast` | `7.5` | `1.5` | `0.0` | `0.0` | `0.0` | `3.0` |
| `Foggy` | `0.0` | `10.0` | `0.0` | `0.0` | `0.0` | `1.0` |
| `Rain_Light` | `6.0` | `1.5` | `3.0` | `0.0` | `0.0` | `2.0` |
| `Rain` | `7.5` | `3.0` | `7.0` | `0.0` | `0.0` | `3.0` |
| `Rain_Thunderstorm` | `8.0` | `6.5` | `10.0` | `0.0` | `0.0` | `10.0` |
| `Sand_Dust_Calm` | `0.0` | `1.0` | `0.0` | `0.0` | `10.0` | `1.0` |
| `Sand_Dust_Storm` | `0.0` | `1.0` | `0.0` | `0.0` | `10.0` | `10.0` |
| `Snow_Light` | `6.0` | `3.0` | `0.0` | `3.0` | `0.0` | `1.0` |
| `Snow` | `8.5` | `5.0` | `0.0` | `6.0` | `0.0` | `4.0` |
| `Snow_Blizzard` | `10.0` | `10.0` | `0.0` | `10.0` | `0.0` | `10.0` |

All inspected presets reported `Temperature=0.0` through the same property read.

The current DemoMap UDW values match the `Partly_Cloudy` preset numerically:
`Cloud Coverage=3.8`, `Fog=1.0`, `Wind Intensity=2.0`, and no rain/snow/dust.

## Blueprint Flow

Relevant UDW graph groups from the earlier Blueprint read:

- `Start Weather System`
- `Update Active Variables`
- `Update Static Variables`
- `Update Current Global And Local Weather State`
- `Monitor Local Weather Changes`
- `Update Material Effect Parameters`
- `Set Shared Weather Particle Parameters`
- `Sparse Movement Updates`

The key flow is:

1. UDW selects or blends global/local weather state.
2. Weather state exposes cloud, fog, rain, snow, dust, wind, and temperature
   values.
3. UDW updates weather-facing material and particle parameters.
4. UDS reads cloud/weather values and updates sky MID/MPC values.
5. Volumetric cloud density is derived on the UDS side from cloud coverage.

The cloud failure found in this pass happened after UDW supplied reasonable
cloud coverage. UDS had a valid derived density, but the UDS apply path was
gated by `Composite Weather Change Speed > 0`, leaving the volumetric MPC
runtime `Cloud Density` at `0`.

## Cubeless Porting Notes

Cubeless does not need to clone the full UDW weather system to get UDS-like
clouds. Start with the subset that actually feeds the sky:

- `Cloud Coverage`
- `Cloud Coverage 0-3` or an equivalent normalized coverage value
- derived cloud density
- `Fog`
- `Wind Intensity`
- cloud speed
- cloud direction
- weather transition speed

Keep precipitation and ground weather as later modules:

- rain/snow particles
- wetness/puddles
- dust/fog post process
- local weather masks
- weather override volumes
- sound

For Cubeless, the safest shape is a small weather preset data asset that writes
to a Cubeless-owned sky/weather state object, then a separate sky controller
turns that state into Cubeless MPC/MID values. This avoids importing UDW's
large local weather, Niagara, sound, and landscape-weather machinery before it
is needed.
