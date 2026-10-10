# StackOBot Live Read Drill 2026-06-19

This drill validates that the request compiler routes still match the live
StackOBot editor assets after the UnrealMCP bridge was restarted.

Scope:

- Project: `<workspace-parent>/SampleProject/StackOBot`
- Primary bridge: `127.0.0.1:55557`
- Editor start: hidden `UE_5.7` editor process
- Asset mutation: none
- C++ changes: none
- Dirty content/map packages after reads: `0`

## Live Read Results

| Compiler route | Asset read | Node id | Key result | Status |
| --- | --- | --- | --- | --- |
| Post Process ModifyBone | `/Game/_MCP_Sample/AnimStudy/ABP_Bot_PostProcess_Study_HeadYawPlus5Study` | `15E54B7F47915619D6ACBF9A5EF3BDB6` | `Bone=head`, additive bone-space `Yaw=5`, input and output pose links connected | Pass |
| Trail secondary motion | `/Game/_MCP_Sample/AnimStudy/ABP_Bot_Trail_Study` | `E6CA339B47B4B75F5CCCB19B09796556` | `TrailBone=antenna_04_l`, `BaseJoint=head`, `ChainLength=4`, `Alpha=1`, `FakeVelocity=(0,0,0)` | Pass |
| RigidBody physics | `/Game/StackOBot/Characters/Blobling/Anim/ABP_Baddy` | `81E779C34D36CC52F0125F91BF52BAF3` | `SimulationSpace=ComponentSpace`, `Alpha=1`, no external force, world geometry disabled | Pass |
| ControlRig late correction | `/Game/StackOBot/Characters/Bot/ABP_Bot` | `AC1F677F4F57DC2325C370BA09E4577A` | `CR_Bot_Correction_C`, source linked from `BlendListByBool`, output linked to root, `InteractionWorldLocation` and `ShouldDoIKTrace` wired | Pass |
| UpperBody layered route | `/Game/StackOBot/Characters/Bot/ABP_Bot` | `A6513D7A4006C58E2BC82AADE84F15F6` | `BasePose` and `BlendPoses_0` connected, `BlendWeight=1`, branch filters `pelvis depth 4`, `thigh_r/-1`, `thigh_l/-1` | Pass |
| State-machine route | `/Game/StackOBot/Characters/Bot/ABP_Bot` | `5055E39F4D4E08F5A30600AC70E5EF29`, `5FCAFB78410BDF1670D00F85A92ECFE8` | `GroundLocomotion` and `AirLocomotion` state machine nodes present and linked to cached poses | Pass |

## Interpretation

The natural-language compiler remains aligned with live assets for the current
safe routes:

- static late head/antenna edits route to Post Process ModifyBone samples.
- antenna lag routes to the existing Bot Trail Post Process sample.
- Baddy soft-body/stalk requests route to RigidBody evidence or sample tuning.
- interaction foot IK routes to the existing ControlRig node and forced-driver
  sample if gameplay gates are inactive.
- upper-body action requests should reuse the existing `UpperBody` slot and
  `LayeredBoneBlend` route before authoring a new overlay branch.
- state changes should start with read/runtime-driver probes before any graph
  authoring.

## Caveats

- Advanced PoseWatch, BlendSpace grid, and ControlRig forced-driver commands were
  not exposed in the current tool list during this pass, so this drill is a live
  read validation rather than a new runtime pose-capture rehearsal.
- The latest log scan showed the known startup `LogAutomationTest: Error:
  Condition failed` lines and one audio mixer warning. No new command error or
  asset mutation was observed during this read drill.
- If a future request needs visible upper-body action playback, new action-source
  authoring remains a candidate rather than covered by this read drill.
