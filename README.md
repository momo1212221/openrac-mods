# Moritz' OpenRAC mod list

This branch is a personal mod repository independent of the official OpenRAC mod index.

## One launcher URL

Paste this in **OpenRAC Launcher -> Mods -> Add from a link**:

`https://github.com/momo1212221/openrac-mods/tree/my-mods`

The repository exposes only the folders listed in `openrac-mods.json`. The normal `master`
branch of this fork is left unchanged.

## Included

The catalog mirrors the latest deliberately selected published build of each active mod. It contains
Modern Controls, Steam Deck Pack, Better HUD, QoL Hub, Checkpoint Plus, Collectible Tracker,
Cutscene Manager, Death Improvements, Fast Travel, Improved Vendor, Mission Tracker, New Game Plus,
Save Manager, Weapon Wheel Pause, Multiplayer, Graphics preparation, Graphics Effects Runtime,
Backup & Cloud Saves, Enhanced Weapons, Enhanced Audio, the current Enhanced Accessibility Pack and Arsenal Mastery.

Native release archives are mirrored from their source repositories into one personal bundle release
and re-hashed in this repository. Graphics preparation and Graphics Effects Runtime publish universal
multi-platform archives; the mirror exposes those same verified archives under the platform-specific
names the OpenRAC link installer expects.

## Deliberate exclusions / legacy

- `momo1212221/accessibility-pack` v0.2.0 (hold-to-mash) is not active in this list because it has
  the same id, `accessibility-pack`, as the newer Enhanced RAC accessibility build. Its repository
  is not deleted.
- `momo1212221/openrac-enhanced-vanilla` is tracked as a draft because it has no published mod
  release yet.
- Graphics Effects Runtime is retained for standalone/older packs, but Graphics preparation 0.17.0
  already includes its runtime.

## Source of truth

`my-sources.json` pins every source repository, tag and version. The bundle workflow never takes an
unreleased branch by accident. If a source repository is ahead of its latest release, this catalog
keeps using the published version until the pin is intentionally updated.


## Arsenal Mastery port

`arsenal-mastery` 0.1.0 is the private OpenRAC port of AsobiCodes' RAC-RS Arsenal Mastery.
It is built for Android ARM64, Linux ARM64/x86_64, macOS ARM64/x86_64 and Windows x86_64.
It is included only in this personal `my-mods` catalog and is not submitted to the official OpenRAC mod index.
