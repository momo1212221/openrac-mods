# Arsenal Mastery for OpenRAC

## Native UI changes (0.4.8 development branch)

Arsenal Mastery now uses the weapon HUD design of **Ratchet & Clank 2: Going Commando** as a reference, while retaining the original Ratchet & Clank 1 icon and ammo number:

- The **large separate weapon XP panel at the bottom-left is not drawn**. There are no always-on weapon-name / XP-number / debug lines.
- A slim, red (gold after V5) XP strip appears immediately **under the original ammo badge in the top-left**. Its position, weapon and opacity follow the actual game's ammo HUD record, including switching weapons and its built-in hide/fade animation. The V level appears as a small tag on the same line.
- XP bar filling interpolates over a few ticks. A level-up briefly lights up the V label within that same HUD area; it does not open a secondary notification.
- When there is no ammo badge (wrench, gadgets, cutscenes or a hidden game HUD), no detached progress panel is displayed. Weapon levels and XP still advance normally.
- In the game's existing Weapons menu, each icon receives a compact XP bar and V1-V99 label; there is no separate Arsenal Mastery pause page.

This is **unreleased source**, not an installable macOS build. Do not paste the GitHub branch tree URL into the launcher's *Add a mod* field. The current build has to pass the native tests and generate `arsenal-mastery-0.4.8-macos-arm64.zip` before the launcher can install it. Existing save progression is kept in the per-slot sidecar; no save-data migration is part of this HUD change.

OpenRAC mod API 1.14 is required for the attached on-screen XP bar. On older builds, progression keeps working but no separate fallback text panel is drawn. The weapon-menu integration and alignment still need a game-runtime check on PAL and NTSC-U.

OpenRAC port of **Arsenal Mastery** from RAC-RS.

- **Original developer / design:** AsobiCodes
- **OpenRAC port:** Moritz Wallinger (`momo1212221`)

## Install in OpenRAC Launcher

In **Mods → Add a mod**, paste this repository/release link:

`https://github.com/momo1212221/openrac-mods`

OpenRAC chooses the archive for the current platform and verifies it against `SHA256SUMS`.

## Previous release: 0.4.6

The following describes the previous 0.4.6 implementation, not the 0.4.8 native HUD development branch.

0.4.6 keeps the 0.4.5 gameplay and runtime hardening, including `hero_pose()` for the held item and current health. The first-gameplay log stays available for diagnostics; a visible mod notice appears only with Trace logging enabled.

### Native in-game UI

- The pause-menu entry is **Weapons** instead of a branded mod page.
- The game's own menus show two pages of selectable weapons; select a weapon for level, XP, damage, ammo and kill details. Nanotech has its own native detail page.
- On OpenRAC interface 1.14, the weapon HUD uses a compact amber framed level/XP gauge. The Nanotech gauge appears briefly after earning XP; weapon upgrades flash a short message.
- On interface 1.12/1.13, the display uses OpenRAC's game-font HUD text instead of a separate overlay.
- If the shared pause-menu slots are filled by other mods, it tries the title menu; if those slots are also full, gameplay and HUD still load.

Implemented:

- weapon XP from post-hit enemy health delta, capped to the hit's effective strength;
- death-confirmed kill credit and the original 50% last-hit bonus;
- original per-weapon XP scaling;
- V1–V5 progression and damage curve;
- Mega / Giga / Omega progression and Endless levels up to V99;
- optional Portable Mega fallback for OpenRAC;
- per-save-slot persistent XP, Mega point, kill counts and Nanotech XP;
- New Game reset and New Game+ carry-over;
- planet-based enemy durability rebalance, Hard and NG+ multipliers;
- Nanotech progression up to 100 points, 200 in NG+;
- weapon ammo-capacity growth and Survival kill ammo refund;
- native-looking weapon XP gauge, contextual Nanotech gauge, and fallback game-font HUD;
- native Weapons pages in Pause → Options with weapon details;
- PAL and NTSC-U;
- Visibomb split-projectile attribution;
- trace logging;
- standalone progression and persistence tests.

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
