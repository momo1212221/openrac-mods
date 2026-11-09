# Arsenal Mastery — OpenRAC port

This is the native OpenRAC port of **Arsenal Mastery** from
[RAC-RS](https://github.com/AsobiCodes/rac-rs/tree/main/bundled/mods/arsenal-mastery).

Version **0.2.0** replaces the fragile first milestone with OpenRAC's current 1.14 APIs.

## Implemented

- Damage-based weapon XP.
- Original V1–V5 XP thresholds and per-weapon XP scaling.
- Original V1–V5 weapon power curve.
- Direct hero attacks resolved through OpenRAC's hero/figure API instead of hard-coded hero globals.
- Projectile attribution through OpenRAC moby slots plus the original Arsenal Mastery projectile-class table.
- Non-hero world targets no longer depend on an incomplete fixed enemy-class whitelist.
- Original planet enemy durability curve.
- Hard rebalance multiplier.
- New Game starts fresh progression.
- New Game+ preserves progression.
- **Persistent XP per OpenRAC memory-card slot.**
- Unsaved New Game XP transfers to its slot when the game is first saved.
- **Enhanced RAC Style in-game XP panel.**
- Visible V1–V5 progress bar.
- Level-up banner.
- Trace logging remains available for diagnostics.
- PAL and NTSC-U.

## Persistence

Arsenal Mastery stores only its own progression in OpenRAC's private mod data directory:

`arsenal-mastery-xp-v2.dat`

It does not rewrite the game's memory-card save. Each OpenRAC save slot gets its own weapon-XP
table. The file is written periodically and immediately on a weapon level-up.

## Progression

V2, V3, V4 and V5 begin at cumulative base XP:

| Level | Base XP |
| --- | ---: |
| V1 | 0 |
| V2 | 750 |
| V3 | 2,250 |
| V4 | 5,000 |
| V5 | 10,000 |

Per-weapon scaling from the original mod remains in place: early weapons use 1.0, mid-game weapons
0.75 and late weapons 0.5.

The original power multipliers remain **1.0, 1.2, 1.5, 1.9, 2.5**.

## OpenRAC UI

The equipped supported weapon now displays:

- weapon name;
- V1–V5 badge;
- current / next XP;
- segmented five-stage progression;
- RAC-style level-up banner.

This removes the old situation where the port could technically earn XP with no visible indication.

## Enhanced Weapons compatibility

Arsenal Mastery is intended to own gameplay progression when it is installed together with
Enhanced Weapons. Enhanced Weapons should leave its hit-marker/visual effects enabled but disable
its own XP and damage progression to avoid double scaling.

## Remaining parity gaps

The port still intentionally does not claim full RAC-RS feature parity for:

- Gold / Mega / Giga / endless V99 progression;
- Gold-case integration;
- Nanotech levelling;
- ammo-capacity progression;
- perks and special weapon behaviours;
- exact post-damage health-removed / last-hit XP accounting.

Those are separate gameplay systems, not required for reliable V1–V5 progression.

## Testing

For normal play, the HUD is enough to verify progression. For diagnosis, enable **Trace** in the
launcher; each attributed hit logs weapon, XP gain, total XP, V-level, original/scaled damage and
rebalance factor.

## Attribution

Original Arsenal Mastery design and RAC-RS implementation: **AsobiCodes**, MIT licensed.

OpenRAC persistence/HUD integration and port hardening: **Moritz Wallinger**.

No game assets are included.
