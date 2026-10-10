# UDS State Signal Matrix

This matrix turns the current UDS/UDW inspection into a signal map. Use it to
answer three questions:

1. Where does a visible sky value come from?
2. Which runtime value should be trusted over an asset default?
3. What is the Cubeless-owned equivalent?

The current reference state is `/Game/UltraDynamicSky/Maps/DemoMap` after the
runtime-only volumetric cloud repair.

## Read First

UDS is not driven by one source of truth. A visible result can be split across:

- UDW actor-facing weather values
- UDS actor-facing sky values
- Blueprint-derived functions
- runtime Material Parameter Collection values
- runtime dynamic material instance values
- material function defaults
- component visibility and material assignment

Do not copy one layer blindly. Capture the source signal, the derived signal,
and the final material signal together.

## Coverage And Density Signals

| Signal | Current value | Source | Runtime target | Meaning | Cubeless equivalent |
| --- | ---: | --- | --- | --- | --- |
| `UDW.Cloud Coverage` | `3.799999952316284` | `Ultra_Dynamic_Weather` actor | UDW state and weather MPC | Weather-facing cloud amount. Matches `Partly_Cloudy`. | `WeatherPreset.CloudCoverage` |
| `UDS.Cloud Coverage` | `3.799999952316284` | `Ultra_Dynamic_Sky` actor | UDS Blueprint functions | Sky-facing cloud amount after UDW connects to UDS. | `SkyState.CloudCoverage` |
| `UDS.Cloud Coverage 0-3` | `1.1399999856948853` | UDS actor/derived state | UDS density and sky MID formulas | Normalized-ish coverage lane used by UDS cloud formulas. | `SkyState.CloudCoverageNormalized` |
| `Current Volumetric Clouds Density` | `1.310999983549118` | UDS Blueprint function | `UDS_VolumetricClouds_MPC.Cloud Density` | Derived density that should make the volume visible. | `SkyState.VolumetricDensity` |
| `UDS_VolumetricClouds_MPC.Cloud Density` asset default | `0` | MPC asset | None until runtime write | Stored default, not the visual state. This default caused the trap. | Do not copy as visual state |
| `UDS_VolumetricClouds_MPC.Cloud Density` runtime | `1.3109999895095825` | runtime MPC value | `Volumetric_Clouds` material | Actual repaired volume density used by the material. | `MPC_CubelessSky.CloudDensity` |
| `Sky_Sphere MID.Cloud Density` runtime | `1.5959999561309814` | sky sphere MID | sky-dome material instance | 2D/sky-dome cloud density path, separate from volume density. | `SkyDomeMID.WispOrLayerDensity` |
| `UDS_VolumetricClouds_MPC.Layer 2 Density` runtime | `0` | runtime MPC value | optional second cloud layer | Disabled in current state because `Two Layers=false`. | `SkyState.Layer2Density` |

Important trap:

- `Currently Cloudy=false` on UDW does not mean the sky has no visible clouds.
  The current visual state still has `Cloud Coverage=3.8` and repaired
  volumetric density about `1.311`.

## Altitude, Scale, And Shape Signals

| Signal | Current value | Source | Runtime target | Meaning | Cubeless equivalent |
| --- | ---: | --- | --- | --- | --- |
| `Bottom Altitude` runtime | `59940` | `UDS_VolumetricClouds_MPC` | volume material | Volume layer bottom. Asset default is `0`, so runtime is mandatory. | `MPC_CubelessSky.CloudBottomAltitude` |
| `Top Altitude` runtime | `129940` | `UDS_VolumetricClouds_MPC` | volume material | Volume layer top. Asset default is `0`. | `MPC_CubelessSky.CloudTopAltitude` |
| `Cloud Layer Height` runtime | `100000` | `UDS_VolumetricClouds_MPC` | volume material | Height span or sampling scale reference. | `MPC_CubelessSky.CloudLayerHeight` |
| `Current Base Clouds Scale` | `1200000.0` | UDS Blueprint function | `Clouds Scale` runtime | Main broad cloud scale. | `SkyState.CloudScale` |
| `Clouds Scale` runtime | `1200000` | `UDS_VolumetricClouds_MPC` | volume material | Actual scale consumed by the volume shader. | `MPC_CubelessSky.CloudScale` |
| `Macro Scale` runtime | `1.7549999952316284` | `UDS_VolumetricClouds_MPC` | volume material | Large pattern frequency. | `MPC_CubelessSky.MacroScale` |
| `Current Volumetric Cloud Macro Variation` | `0.16` | UDS Blueprint function | `Macro Variation` runtime | Derived macro breakup amount. | `SkyState.MacroVariation` |
| `Macro Variation` runtime | `0.1599999964237213` | `UDS_VolumetricClouds_MPC` | volume material | Actual macro variation consumed by material. | `MPC_CubelessSky.MacroVariation` |
| `High Frequency Noise` runtime | `0.23999999463558197` | `UDS_VolumetricClouds_MPC` | volume material | Fine breakup. | `MPC_CubelessSky.HighFrequencyNoise` |
| `3D Erosion` runtime | `1.2000000476837158` | `UDS_VolumetricClouds_MPC` | volume material | Edge erosion and thinning. | `MPC_CubelessSky.Erosion` |

Important trap:

- Asset defaults for `Bottom Altitude`, `Top Altitude`, and `Cloud Density` do
  not describe the current view. Use runtime MPC values for visual analysis.

## Lighting And Scattering Signals

| Signal | Current value | Source | Runtime target | Meaning | Cubeless equivalent |
| --- | ---: | --- | --- | --- | --- |
| `Get Current Volumetric Cloud Extinction Scale` | `10.0` | UDS Blueprint function | `Extinction Scale` runtime | Derived extinction reference. | `SkyState.ExtinctionScale` |
| `Extinction Scale` runtime | `10` | `UDS_VolumetricClouds_MPC` | volume material | Actual extinction used by material. | `MPC_CubelessSky.ExtinctionScale` |
| `PhaseG` runtime | `0.8500000238418579` | `UDS_VolumetricClouds_MPC` | volume material | Primary anisotropic scattering. | `MPC_CubelessSky.PhaseG` |
| `PhaseG2` runtime | `0.4000000059604645` | `UDS_VolumetricClouds_MPC` | volume material | Secondary scattering lobe. | `MPC_CubelessSky.PhaseG2` |
| `Phase Blend` runtime | `0.6499999761581421` | `UDS_VolumetricClouds_MPC` | volume material | Blend between phase lobes. | `MPC_CubelessSky.PhaseBlend` |
| `Weather MPC.Sun Vector` | `(0.4786598, -0.2890313, -0.8290631, 1)` | weather MPC runtime | sky, clouds, fog | Directional light vector reference. | `MPC_CubelessSky.SunVector` |
| `Weather MPC.Moon Vector` | `(-0.6245860, 0.1414588, 0.7680376, 1)` | weather MPC runtime | sky and night lighting | Moon direction reference. | `MPC_CubelessSky.MoonVector` |
| `Weather MPC.Ambient Fog Color` | `(0.1435080, 0.2595527, 0.4861628, 0.96)` | weather MPC runtime | fog and sky effects | Ambient tint used by weather materials. | `MPC_CubelessSky.AmbientFogColor` |
| `Sky_Sphere MID.Cloud Wisps Color` | `(0.303107, 0.357943, 0.413193, 1.0)` | sky sphere MID | sky-dome wisp branch | Runtime color for wisp overlay. | `SkyDomeMID.WispColor` |
| `Sky_Sphere MID.Cloud Wisps Gradient` | `(0.4786598, -0.2890313, -0.8290631, 5.0)` | sky sphere MID | sky-dome wisp branch | Sun-vector-like gradient with strength in alpha. | `SkyDomeMID.WispGradient` |

Important trap:

- UDS scattering values can be useful ranges, but the previous Cubeless probe
  showed that directly copying UDS volume MPC values can make the Cubeless
  candidate too dark or patterned. Treat these as reference ranges, not final
  Cubeless settings.

## Wind, Motion, And Time Signals

| Signal | Current value | Source | Runtime target | Meaning | Cubeless equivalent |
| --- | ---: | --- | --- | --- | --- |
| `UDS.Cloud Speed` | `0.35` | UDS actor | cloud movement functions | Sky actor cloud motion amount. | `SkyState.CloudSpeed` |
| `UDS.Cloud Direction` | `180.0` | UDS actor | weather MPC and material offsets | Main cloud movement direction. | `SkyState.CloudDirection` |
| `UDS.Cloud Phase` | `0.0` | UDS actor | cloud movement offsets | Accumulated movement phase. | `SkyState.CloudPhase` |
| `UDW.Wind Intensity` | `2.0` | UDW actor | weather logic | Actor-facing weather wind intensity. | `WeatherPreset.WindIntensity` |
| `UDW.Sky Cloud Speed` | `0.0735427785233055` | UDW function | UDS/cloud movement | Weather-derived cloud speed contribution. | `WeatherState.SkyCloudSpeed` |
| `Weather MPC.Wind Intensity` | `0.20000000298023224` | weather MPC runtime | materials | Remapped material-facing wind intensity. | `MPC_CubelessSky.WindIntensity` |
| `Weather MPC.Wind Angle` | `180` | weather MPC runtime | materials | Material-facing wind angle. | `MPC_CubelessSky.WindAngle` |
| `Weather MPC.Wind Force` | `(-178.885437, 0.000004, 0, 1)` | weather MPC runtime | particles/materials | Vector wind force. | `MPC_CubelessSky.WindForce` |
| `UDS.Time of Day` | `1088.000178` | UDS actor | sky, sun, moon updates | Actor-facing time. | `SkyState.TimeOfDay` |
| `Weather MPC.Time of Day` | `960` | weather MPC runtime | weather materials | Material-facing time lane. | `MPC_CubelessSky.TimeOfDay` |

Important trap:

- UDW actor `Wind Intensity=2.0` becomes weather MPC
  `Wind Intensity=0.2`. Cubeless should define its own public range and its
  own material-facing range instead of assuming the same unit is used
  everywhere.

## Weather State And Event Signals

| Signal | Current value | Source | Runtime target | Meaning | Cubeless equivalent |
| --- | ---: | --- | --- | --- | --- |
| `Weather Speed` | `1.0` | UDW actor | interpolation/update functions | Weather transition speed. | `WeatherState.TransitionSpeed` |
| `Composite Weather Change Speed` | `0.0` | UDS actor | `Update Cloud Coverage Material Parameters` gate | Current gate that blocked cloud density write. | Avoid this gate or add forced sync |
| `UDW.Fog` | `1.0` | UDW actor | weather MPC, fog material | Weather-facing fog amount. | `WeatherPreset.Fog` |
| `UDW.Rain` | `0.0` | UDW actor | Niagara/sound/wetness | Rain amount. | Later weather effect |
| `UDW.Snow` | `0.0` | UDW actor | Niagara/sound/snow effects | Snow amount. | Later weather effect |
| `UDW.Dust` | `0.0` | UDW actor | dust storm materials/effects | Dust amount. | Later weather effect |
| `State Change - Cloud Coverage` | active delegate | UDW Blueprint | UDS `AP - Cloud Coverage` path | Weather-to-sky cloud update event. | `OnCloudCoverageChanged` |
| `AP - Cloud Coverage` family | binding path | UDS Blueprint | actor values and update functions | Thin apply-property family, not the core formula by itself. | Public setter plus state sync |

Important trap:

- The current failure is a sync failure, not a density formula failure. UDS
  could derive a valid density but did not write it into the runtime MPC
  because `Composite Weather Change Speed` was `0`.

## Static, 2D, And Wisp Signals

| Signal | Current value | Source | Runtime target | Meaning | Cubeless equivalent |
| --- | --- | --- | --- | --- | --- |
| `Sky Mode` | `VOLUMETRIC_CLOUDS` | UDS actor | component/material paths | Current mode uses volume clouds, but sky-dome layers still matter. | `SkyMode.VolumeWithWisp` |
| `Composite_Cloud_Layers` | calls `Cloud_Layer` twice | material function | sky-dome material | Combines one or two 2D cloud layers. | `MF_Cubeless_CompositeCloudLayers` |
| `Cloud_Layer` | uses mapping, distribution, filtering | material function | sky-dome material | Main 2D layer assembly. | Separate mapping/sample/filter functions |
| `Map_Cloud_Textures` | samples `ParticleClouds` | material function | sky-dome material | Maps packed cloud texture channels. | Cubeless packed cloud sampler |
| `Cloud_Wisps` | default `ParticleClouds` | material function | sky-dome material | Wisp overlay branch. | Cubeless wisp overlay |
| `Sky_Sphere MID.Cloud_Wisps_Texture` | `/Game/UltraDynamicSky/Textures/Sky/Cloud_Wisps` | runtime MID override | sky-dome material | Runtime texture override differs from function default. | Cubeless-owned wisp texture |
| Static cloud reference | `/Game/UltraDynamicSky/Textures/StaticClouds/Custom/cloub02` | reference texture | static cloud layer | Current polar/radial UV reference for Keilan. | Cubeless-owned packed static cloud |

Important traps:

- `Map_Cloud_Textures` depends on `UDS_VolumetricClouds_MPC`, so a material
  function that looks like a 2D/static cloud helper can still pull volume MPC
  state.
- Function defaults and runtime MID overrides differ. Capture the MID when
  checking the actual view.
- Static cloud RGBA is data, not final beauty color:
  - `R`: upper-right key light response
  - `G`: upper-left key light response
  - `B`: overhead/front fill response
  - `A`: opacity/density

## Cubeless Translation Rule

Translate in this order:

1. Public weather preset value.
2. Cubeless semantic sky state.
3. Cubeless material-facing MPC value.
4. Runtime material instance override.
5. Rendered screenshot.

UDS often jumps between these layers through Blueprint functions and MPC writes.
Cubeless should keep the layers explicit so the same visibility bug cannot hide
behind a vendor graph gate.
