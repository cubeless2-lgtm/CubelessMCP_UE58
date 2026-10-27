# Crimson Dawn motion and interface revision

Implementation is complete. [Current Korean delivery and verification record](UPDATE_V2.md) supersedes the initial values below, including enemy cap, jump cooldown and hero identities. The initial proposal is preserved as planning history.

## Goal and self-review before implementation

The first version has only two hero locomotion poses and static enemies. Its oversized default-font title and flat panels weaken the gothic pixel-art direction. Preserve the 180-second loop and two hunters, and replace presentation and combat timing through standard Engine Blueprint graphs only.

- Heroes: 24 generated poses each: four walk, four run, four attack frames, with front/right and back/right views. Mirror UVs for left-facing movement. Attack windup, release/impact, recovery must drive the actual damage timing.
- Sprint: hold either Shift while moving; speed x1.55, stamina 100, drain 34/s, recover 24/s after a 0.65s delay. Exhaustion requires 25 stamina to resume. Normalize diagonal movement and retain facing when stationary.
- Monsters: five types, four locomotion and four attack poses each. Individual animation phase, left/right facing, hit flash, attack windup, recovery and death particles. Keep per-enemy metadata arrays aligned on spawn/death.
- Challenge: earlier mixed waves, higher density and speed, increasing enemy HP, less frequent healing, stronger boss pulse with visible warning. Cap ordinary enemies at 119 and reserve a boss slot. Keep the 180s survival win; boss kill is a bonus.
- Strong attacks: draw the exact ground circle before impact for revenant, stalker and boss; gold windup becomes red during the damage window. Use identical centers and radii for display and damage: 66, 58, 75 px local strikes and 165 px boss pulse.
- Jump addition requested during production: Space starts a 0.95s jump, character height 1m = 75 virtual pixels, apex 3m = 225px. A second Space in flight dives to the point 5m = 375px from the jump origin along the captured movement direction; arena limits clamp the destination. 0.30s dive, weak damage (35% normal damage, at least 6), 110px extra knockback in a 95px circle, 1.2s cooldown after landing. Generated anticipation/rise/apex/fall/dive/impact/recovery poses in front/back views. Ground attacks can be avoided while more than 30px airborne. Keep the shadow on the ground and show the dive landing marker.
- UI: generated title logo, 16 original inventory/status icons and four metal frame assets. Clear hierarchy: life/stamina left, countdown center, kills/experience right; icon-led upgrade cards, animated hunter previews, consistent gold/crimson/teal palette.

Self-review findings: attack sprites without delayed hit timing would still feel disconnected; facing needs both mirroring and back views; sprint needs clear exhaustion feedback; dense waves must retain telegraphs and a population limit; title must be an actual image rather than enlarged body text. Validate these in PIE with input, snapshots and full normal-speed play.

## Web reference research

- [Supergiant Hades II official game page](https://www.supergiantgames.com/games/hades-ii/): inspected official key art and the game's media links. Use a focused identity, strong artwork-led opening and a small number of clear actions as design references. No copied logos or art.
- [Red Hook's Darkest Dungeon II official website](https://www.darkestdungeon.com/darkest-dungeon-2/): inspected the opening/action links, large feature headings and official media structure. Use a focused entry action, dark panels, clear title/body hierarchy and warm highlights. This is a design interpretation, not a measured usability finding or a copied layout.
- [Path of Exile 2 official website](https://pathofexile2.com/early-access): attempted as a dark fantasy web reference; text fetch requires JavaScript and the in-app browser timed out. This attempt is not a completed visual review.
- Diablo IV official site returned 403; Awwwards games gallery timed out. Do not claim these pages were visually reviewed.

## Delivery and verification

Create BP_CrimsonHUDV2 alongside the original HUD, import versioned art, compile/save, then update only the owned GameMode's HUD class. Avoid graph deletion in the live editor. Record current compile, rendering, input, animation, combat and balance results under cubeless/records/verification/vampire-survival-v2. This is self-review, not independent QA.

## User-directed revisions during production

- Replace Lyra with Shami as a mage and Kael with Minie as a swordswoman. Preserve the retrieved character identity in newly generated pixel sprites; Shami casts violet magic, Minie uses the yellow industrial spirit-level weapon shown in the original design.
- [Shami standing illustration source](https://rxbase.slack.com/archives/C06DFPPKKJ8/p1786326279516209), Slack file F0BP5358AKB, Kiwi_샤미_Scg_공유용크기줄임.jpg.
- Minie standing illustration, Slack file F095KFE8SEB, Kumquat_미니에(윌오더위스프)_Scg사이즈조절.jpg; [Minie weapon design thread](https://rxbase.slack.com/archives/C070N6MRSQ4/p1740449995999469), final shared design file F08HSQHKCP9, 미니에 수평계.jpg. Retrieved references are preserved under SourceArt/References. No Slack messages were sent or modified.
- Jump cooldown is now 10 seconds after landing, with a visible countdown. Supersedes the earlier 1.2s proposal.
- Dive landing range uses a cyan circle of radius95px at the predicted destination. At impact it flashes white/cyan at the exact damage center and radius; enemy telegraphs use gold/red.
- Ordinary enemy cap is now 179 plus one reserved boss. Spawn two enemies per batch after60s and three after90s. Supersedes the earlier119 proposal.
- Editor authoring failures: the official add_call_function_node route crashed repeatedly when creating promoted arithmetic/comparison nodes. Preserve the logs and saved prefixes; arithmetic now uses the existing editor-only CubelessFunctionLibrary.add_blueprint_call_function_node route to author standard Engine UK2Node_CallFunction nodes. No runtime helper or new native code is added. Exact native crash cause is unconfirmed.
