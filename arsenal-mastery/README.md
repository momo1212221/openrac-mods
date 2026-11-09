# Arsenal Mastery for OpenRAC

OpenRAC port of **Arsenal Mastery** from RAC-RS. Original mod and design: **AsobiCodes**. OpenRAC port: **Moritz Wallinger**.

## Install in OpenRAC Launcher

In **Mods → Add a mod**, paste this repository/release link:

`https://github.com/momo1212221/openrac-mods/releases/tag/arsenal-mastery-v0.4.3`

OpenRAC chooses the archive for the current platform and verifies it against `SHA256SUMS`.

## 0.4.3 status

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
- held-weapon XP HUD and Nanotech HUD;
- Arsenal Mastery page in Pause → Options;
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
