# Arsenal Mastery for OpenRAC

OpenRAC native C++ port of **Arsenal Mastery** from RAC-RS.

- **Original developer / design:** AsobiCodes
- **OpenRAC port:** Moritz Wallinger (`momo1212221`)

## 0.4.8 native HUD and Weapons page

The large centered extra HUD and persistent text messages have been removed in favor of:

- a small animated weapon XP bar, aligned immediately below the original ammunition panel;
- the original Arsenal Mastery V1–V99 color palette and a brief fade-out V-level indicator;
- XP bars and V numbers directly beside existing weapon icons on the original 5×3 Weapons screen, with a separate wrench progress bar;
- no level-up `play_notice` banner or permanent debug text;
- an **optional** extra Weapons statistics page (new `details_page` setting, off by default).

The weapons-grid integration uses the existing RAC1 item-grid draw hook to recognize
the 15 original weapon slots. It never replaces the game's weapon icons and only draws
XP for weapons owned by the player. The HUD snapshot is transferred safely from the
game thread to the render thread.

**Compatibility:** The standard OpenRAC 1.14 overlay API intentionally does not
draw during menus. The original Weapons-grid bars therefore require
`momo1212221/OpenRAC` with the opt-in `overlay_menus` host extension from
`feat/arsenal-menu-overlay-opt-in-20261109` (or a build containing that change).
The ammo HUD and level-up popup use the standard 1.14 gameplay overlay.
If running an unmodified OpenRAC build, turn on `details_page` to inspect
weapon levels in the native-styled optional details page.

The grid positions are based on AsobiCodes' `bars.luau` coordinates (512×448);
PAL, NTSC and widescreen need live visual review. This is an implementation,
not yet a claim of pixel-perfect in-game parity.

## Install in OpenRAC Launcher

In **Mods → Add a mod**, paste the release URL after the 0.4.8 tag is published:

`https://github.com/momo1212221/openrac-mods/releases/tag/arsenal-mastery-v0.4.8`

The launcher selects the correct platform archive and verifies `SHA256SUMS`.
The prior 0.4.7 version remains available as a rollback.

## Gameplay systems preserved

- post-hit, confirmed enemy health-delta weapon XP and death-confirmed kill credit;
- V1–V5, Mega, Giga, Omega and optional Endless levels through V99;
- original per-weapon XP scaling, damage/power curve and optional Portable Mega;
- per-save-slot persistent XP, kill counts and Nanotech XP, with NG+ carry-over;
- level-based ammo capacity, Survival kill refunds, enemy rebalance and Nanotech;
- PAL / NTSC-U and Visibomb split projectile attribution;
- trace logging (off by default).

## Validation

`arsenal_mastery_native_ui` tests ammo bar coordinates, V5/V99 colors, popup
expiry, original weapon-grid placement, ownership filtering, menu suppression
and off-state behavior. Existing progression, persistence and UI-style tests
remain registered with CTest. A successful CI build is necessary before release;
automated tests do not replace a game screenshot comparison on both discs.

## OpenRAC-specific differences from RAC-RS

OpenRAC does not currently expose every RAC-RS `omni.*` service as a stable native-mod hook. This port therefore does not pretend to have exact parity where the API cannot safely provide it.

Not yet exact:

- retail Gold/Mega weapon-case UI and automatically granted Gold visuals;
- individual RAC-RS weapon perk modules;
- Survival's exact half-ammo crate rule (kill refund is implemented).

Gold-equivalent V5 power is implemented without granting the retail Gold flag, avoiding incorrect gold-bolt accounting. Portable Mega can unlock Mega through XP where the retail case flow is not exposed.

## Save data

Progress is stored in OpenRAC's per-mod data directory, separate from the game's memory-card save. Sidecars are written atomically and parsed transactionally. Earlier private development sidecars are migrated where supported.

## Build

The repository's GitHub Actions workflow builds and packages native OpenRAC mods for supported desktop platforms. Local builds use the OpenRAC Mod SDK and `openrac_add_mod`.

## License

MIT. See `LICENSE`. The original Arsenal Mastery implementation and design are by AsobiCodes.
