# LiD Stew Multi Pull

**Purchase x10** (or x5, x15, x20, x25) at the Mushroom Club's stew in the
offline version of Let It Die (Steam): that many decals in one go, all shown
together at the end, paid for, drawn and saved in one request.

- **Stew Multi Pull** (`mods/Stew Multi Pull`): the mod for
  [LiD DB Editor](https://github.com/NicoMullin/LiD-DB-Editor) (the LET IT DIE
  DB Mod Manager). It adds the Purchase xN entry, the price of the whole run,
  and the results grid, as a patch to `BrgGame.upk`. See its
  [readme](mods/Stew%20Multi%20Pull/readme.md).
- **lid-stewpull.dll** (`dll/`): a plugin for the
  [LiD Plugin Loader](https://github.com/NicoMullin/LiD-Plugin-Loader) that
  changes the offline game's stew server code, so a whole run is one request.

## What the plugin does

The offline game answers the stew menu with a server built into the game. The
plugin makes three changes to it, **in memory only** (the exe on disk is never
touched):

1. **A whole run in one request.** The mod marks each pull's request with how
   many pulls are left, and the first one also asks for the whole run. That
   request then pays for every pull with the game's own purchase (the price
   times the count, or nothing if you are short), runs the game's draw that
   many times into the reply, and the game saves once. x25 takes about as long
   as one pull. Without the plugin the mod still works, one pull after
   another.
2. **Random stews.** Every normal stew draws from one queue kept in your save.
   A stew whose odds list is named `SKLGACH_RANDOM_*` gets a fresh pick from
   its own list instead, by its odds, every pull - and never the same decal
   twice in one run. The Death Metals mod's Death Metal stew and the Events and
   Seasons festival stew are stews like that.
3. **Every stew offered.** The offline shop offers only the first normal stew
   in `masters.db`; with the plugin it offers them all, so those stews show up.

These used to be part of `lid-deathmetals.dll` (up to 0.5.x). They live here
so the multi pull does not need the Death Metals mod. With Death Metals 0.6.0
or later, the two plugins change different parts of the game.

Safety rules, all in [dll/lid_stewpull.c](dll/lid_stewpull.c):

- It only does anything inside `BrgGame-Steam.exe`. Loaded by any other
  program, it changes nothing.
- Every piece of game code is found by its exact bytes and must be found
  exactly once. Anything not found is left alone and named in the log; after a
  game update that moves the code, the plugin does nothing at all.
- No network, no registry. The only file it writes is `lid-stewpull.log` next
  to itself.
- Delete `lid-stewpull.dll` and the game is exactly stock again.

## Why the DLL is built by GitHub

A DLL can do anything, so you should not have to take one on trust. Every
release on this page is built by GitHub Actions from the source in this
repository, on GitHub's own machines, and the tests run before anything is
published. You can check the DLL in two ways:

1. **Signed build record.** Each release DLL has an attestation: a signed
   record, kept by GitHub, of the exact workflow run and commit that built it.
   With the [GitHub CLI](https://cli.github.com/):

   ```
   gh attestation verify lid-stewpull.dll --repo NicoMullin/LiD-Stew-Multi-Pull
   ```

2. **Rebuild it yourself.** The build is reproducible: the same source and the
   same compiler give a byte-identical file. See
   [Building it yourself](#building-it-yourself) and compare the SHA-256 with
   the one on the release page.

## Installing

**The easy way.** In LiD DB Editor (the LET IT DIE DB Mod Manager) 0.12.6 or
newer, close the game, click **Get mods...** and then **Get** next to **Stew
Multi Pull**. It downloads this repository's newest release, checks it against
the SHA-256 GitHub lists for it, and once you say yes puts the Stew Multi Pull
mod in the manager and `lid-stewpull.dll` in the game's
`Binaries\Win64\lid-plugins` - with the
[LiD Plugin Loader](https://github.com/NicoMullin/LiD-Plugin-Loader) too, if
you do not have it yet. Then switch the game's file check off for
`BrgGame.upk` (Tools > Hash Patcher) if it is not already, tick **Stew Multi
Pull**, pick the number of pulls in its Configuration tab and click **Save Mod
List**.

**By hand.**

1. Install the [LiD Plugin Loader](https://github.com/NicoMullin/LiD-Plugin-Loader)
   once, if you have not already. It puts a `dinput8.dll` and a `lid-plugins`
   folder in the game's `Binaries\Win64` folder.
2. Download `LiD-Stew-Multi-Pull.zip` from the
   [latest release](https://github.com/NicoMullin/LiD-Stew-Multi-Pull/releases/latest).
   Put `lid-plugins\lid-stewpull.dll` from it in the game's
   `Binaries\Win64\lid-plugins`.
3. In the manager, switch the game's file check off for `BrgGame.upk` (Tools >
   Hash Patcher) if it is not already, drop the `mods\Stew Multi Pull` folder
   from the zip onto it, tick it and click **Save Mod List**.

The release also has `lid-stewpull.dll` on its own, for anyone who only wants
the plugin.

**Updating from the copy that came with LiD DB Editor 0.12.5 or older.** That
copy was the mod without the plugin; its runs were one request only with the
Death Metals plugin 0.5.x. Get this release's mod and plugin, and update Death
Metals to 0.6.0 or later if you use it - 0.5.x would change the same server
code as this plugin.

## Building it yourself

You need Windows and Python 3. The compiler is Zig, installed with pip; no
Visual Studio needed.

```
py -3 -m pip install ziglang==0.16.0
py -3 build.py --test
```

The DLL lands in `out\lid-stewpull.dll` and the SHA-256 is printed at the end.
Use Zig 0.16.0 exactly: another version builds a working DLL, but not a
byte-identical one.

`--test` builds stand-ins for the game exe and runs them:

- `tests/stew.c` holds the server's stew pull, shop list, payment and draw in
  the game's own byte shapes, puts the plugin's real code in, and pulls: random
  stews by their odds and without repeats in a run, the normal stew untouched,
  a whole run paid for and drawn in one request, and a run you cannot afford
  refused with nothing drawn.
- `tests/nocode.c` holds none of them, as after a game update, and checks the
  plugin changes nothing and says so in its log.

`--exe PATH\BrgGame-Steam.exe` also runs `tests/realexe.c`: the plugin's own
search on the real game file (mapped, not run), which must find every piece.
The game is not in this repository, so GitHub does not run it.

`--package` also writes `out\LiD-Stew-Multi-Pull.zip`, the release package the
manager's Get mods window installs: `lid-package.json`, the mod under `mods\`
and the plugin under `lid-plugins\`.

## Making a release

1. Set the new version in [dll/version.h](dll/version.h).
2. Push a tag with the same version, for example `v0.1.0`.

GitHub builds, tests and publishes the release. The build stops if the tag and
`version.h` don't match.

## Rebuilding the mod files

The patches in `mods/Stew Multi Pull/tfc/x<N>/` are written from the game's
stock `BrgGame.upk` by
[tools/build_stew_multi_pull.py](tools/build_stew_multi_pull.py), which uses the
package reader from a [LiD DB Editor](https://github.com/NicoMullin/LiD-DB-Editor)
checkout:

```
py -3 tools/build_stew_multi_pull.py --manager PATH\TO\LiD-DB-Editor --upk PATH\TO\BrgGame.upk
```

Run it again after a game update that changes `BrgGame.upk`.

## Credit

The `.PackagePatch` format is FCH823's, from
[TFC Installer](https://www.nexusmods.com/site/mods/587); LiD DB Editor applies
it with their permission.

## License

MIT - see [LICENSE](LICENSE). Made by KSFA.
