# Stew Multi-Pull

Adds a third entry to the Mushroom Club's stew menu, between **Purchase** and
**Check Lineup**: **Purchase x10**, or x5, x15, x20 or x25 - pick the number
in the **Configuration** tab ("Pulls per purchase") and save.

It pulls that many decals in a row. The stew animation plays once, for the
first pull, and the closing animation once at the end. Then every decal you
got shows at once, in a grid five across and three rows down: move the cursor
over one to read what it does and how many you have, and press Back to return
to the stew menu. A bigger run fills more than one page; the page number
("1/2") shows under the grid, and the next page comes up as the cursor goes
down.

With the cursor on Purchase xN, the price under the stew's name is the price
of the whole run.

The grid is the one the online game used for its bonus-box stews' lineup; the
offline game still has it, but never shows it.

## What a pull costs

N pulls' worth: each pull costs what one pull costs, so a price mod such as
Decal Draw Price still applies to every one of them. You need the whole amount
before the run starts; if you are short, the game says so straight away, as it
does for a single pull, and nothing is pulled.

## One request for the whole run

With this mod's plugin, `lid-stewpull.dll`, in the game's
`Binaries\Win64\lid-plugins`, the whole run is one request: paid for, drawn
and saved once, so even x25 takes about as long as one pull. It works for
every stew. The Kill Coin stew gives the decals lined up in your save (the same
ones single pulls would), repeats and all; a stew with its own random list,
such as the Death Metals mod's Death Metal stew, never gives the same decal
twice in one run.

The manager's **Get mods...** window installs the mod and the plugin together.
Without the plugin the mod still works: each pull is then the game's normal
single pull, one after another, and the game saves after each one - the same
grid at the end, it just takes longer.

If one kind of decal reaches 99 partway through, the pulls stop there. The
closing animation still plays, the grid shows the decals you did get, and
after it the game's own message says why.

Plain **Purchase** works exactly as before, and the bonus-box stews are not
changed.

## What it changes

Six functions of the stew menu and one of the bonus-box lineup grid in
`BrgGame.upk`, as a `.PackagePatch` - one for each choice, in `tfc/x5` to
`tfc/x25`. A run also marks each pull's request with the pulls left, and its
first pull with a request for the whole run; the game's own server ignores
both, `lid-stewpull.dll` reads them. If you use the Death Metals mod, it needs
Death Metals 0.5.0 or later: older ones change the same request. The manager applies the one you
chose to your own `BrgGame.upk` alongside other mods that change that file,
such as Instant Drops, and puts the stock file back when every mod that
changes it is switched off. Like any changed game package, it needs the game's
file check switched off (Tools > Hash Patcher).

The menu text comes from the game's own words, "Purchase" and its "x#0", so
it reads right in every language the game ships.

The patches are made by `tools/build_stew_multi_pull.py` (in the
[LiD Stew Multi Pull](https://github.com/NicoMullin/LiD-Stew-Multi-Pull)
repository) from the stock `BrgGame.upk`; 1.89 and 1.91 have the same scripts.
