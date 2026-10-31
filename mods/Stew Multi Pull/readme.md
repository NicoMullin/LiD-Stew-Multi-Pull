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

## The regular stew: random, your own list

Three more settings, under **The regular stew** in Configuration. They change
the Kill Coin stew, not the Death Metal or festival ones.

| Setting                                        | Default                  | What it does |
|------------------------------------------------|--------------------------|--------------|
| **The regular stew**                           | The list in your save    | Or **a fresh random pick every pull** |
| **A decal can come twice in one Purchase xN**  | Off                      | With random picks: off, no decal twice in one run until every decal has come; on, any decal on any pull |
| **Use my own list of decals and weights**      | Off                      | On: the stew gives only the decals you tick, at your weights |

**Why the regular stew is not random already.** The game does not roll when
you pull: it rolls a long list of decals ahead of time, keeps it in your save,
and every pull takes the next one. That is why a change to the stew's odds
seems to do nothing for hundreds of pulls - the list rolled before it has to
run out first.

- **A fresh random pick every pull** rolls each pull there and then from the
  stew's odds, so a change counts from the next pull. It needs
  `lid-stewpull.dll`. The list in your save is left alone, and is used again
  if you switch back.
- **Your own list** opens a window with every decal in the stew, a tick for
  whether it is in and a weight for how often it comes up (weight 3 comes up
  three times as often as weight 1); **Add decals...** puts in any other decal
  in your database. It starts as the stew's own list - the game's odds run from
  2 to 74 - including any decals another mod adds, such as the Crossover
  pack's (tick that pack above this mod in the load order). With **the list in
  your save**, your list is used once the save's list runs out; the save
  editor's **Mushroom Stew > Clear** makes that the next pull.

**Check Lineup** shows the decals the stew can give.

## Pity: the decal you want, after so many pulls

In a stew's **Check Lineup**, put the cursor on the decal you want and press
**Triangle** (the button guide says **Pity decal**). From then on, every pull
of that stew that does not give it counts, and once the count reaches the
number you set, the next pull gives that decal instead. Getting it from an
ordinary pull starts the count again. The pick goes along with your next pull
of that stew.

The stew's purchase menu shows how far you are, on the line box stews use for
**Remaining**: **Pity 37/100: Super Shock Absorber**, or **Pity: pick a decal
in Check Lineup** until you have picked one. A new pick shows there straight
away; the count moves on with each pull.

Each stew counts on its own, with its own settings under **Pity** in
Configuration:

| Setting                             | Default       | Greyed out unless                    |
|-------------------------------------|---------------|--------------------------------------|
| **Pity in the regular stew**        | On, 100 pulls | -                                    |
| **Pity in the Death Metal stew**    | On, 100 pulls | the Death Metals mod is ticked       |
| **Pity in the Super Stew Festival** | On, 100 pulls | the Events and Seasons mod is ticked |

The number of pulls is 10 to 500. Changing your pick keeps the count, so you
can change your mind at any time: whichever decal you pick, it comes after the
same number of pulls. A Purchase xN counts every pull in it, and gives the
pity decal at the right pull of the run. The regular stew counts all its forms
together (the list in your save, a fresh random pick, your own list).

The picks and counts are kept in your save, so each save has its own, and
they come with it when you switch saves in the manager's **Saves** tab. The
pity needs `lid-stewpull.dll` 0.2.0 or later; without it, Triangle does
nothing that lasts.

## What it changes

Seven functions of the stew menu and one of the bonus-box lineup grid in
`BrgGame.upk`, as a `.PackagePatch` - one for each choice, in `tfc/x5` to
`tfc/x25`. A run also marks each pull's request with the pulls left, and its
first pull with a request for the whole run; the game's own server ignores
both, `lid-stewpull.dll` reads them. If you use the Death Metals mod, it needs
Death Metals 0.5.0 or later: older ones change the same request. The manager applies the one you
chose to your own `BrgGame.upk` alongside other mods that change that file,
such as Instant Drops, and puts the stock file back when every mod that
changes it is switched off. Like any changed game package, it needs the game's
file check switched off (Tools > Hash Patcher).

The regular stew's settings, when not at their defaults, give it an odds list
of its own in `masters.db` (`master_skillgacha_odds`):
`SKLGACH_RANDOM_NORMAL` for random picks, `SKLGACH_RANDOM_NORMAL_REPEATS` with
repeats, `LID_STEW_NORMAL` for your own list with the save's list, and rename
the stew (`master_skillgacha`) to match, so Check Lineup reads the same list.
The game's own list, `SKLGACH_NORMAL_OFFLINE`, is not changed. `lid-stewpull.dll`
0.1.3 or later reads the `_REPEATS` ending. These settings need LiD DB Editor
0.13.4 or later.

The pity writes each stew's number of pulls to `master_const_int`
(`LID_PITY_NORMAL`, `LID_PITY_DM`, `LID_PITY_FEST`; a stew with its pity off
has none) and its words to `master_text` (`MSHR_KAIKAN.TXT_LID_PITY`, `_LEFT`
and `_NONE`, in every language). A pick goes with the next pull's request as
`LID_PITY=<stew>|<decal>`, and the plugin keeps the picks and counts in the
save's `user.olid` - a text the online game used, which the offline game only
loads and saves - as `LIDPITY;N=<decal>,<count>;DM=...`. A save whose `olid`
holds anything else is left alone, and has no pity. For the menu's pity line,
the plugin adds an entry named `LID_PITY_<stew>` to the box stews' list in the
shop's reply and in every pull's reply; no box stew has a name like that.

The menu text comes from the game's own words, "Purchase" and its "x#0", so
it reads right in every language the game ships.

The patches are made by `tools/build_stew_multi_pull.py` (in the
[LiD Stew Multi Pull](https://github.com/NicoMullin/LiD-Stew-Multi-Pull)
repository) from the stock `BrgGame.upk`; 1.89 and 1.91 have the same scripts.

## Switched with this mod

From LiD DB Editor 0.12.8, saving with this mod ticked switches
`lid-stewpull.dll` on, and saving with it unticked switches it off: renamed to
`.dll.off`, which the Plugin Loader skips. Nothing is deleted, and ticking the
mod again brings it back. Close the game first. Death Metals names it too (its
Death Metal stew needs it), so it stays on while either mod is ticked.
