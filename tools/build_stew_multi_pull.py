"""Write the Stew Multi-Pull mod's change to BrgGame.upk as a .PackagePatch.

A maintenance tool. It adds "Purchase xN" to the Mushroom Club's stew menu.
That repeats the game's own single pull N times. Each pull is a normal one: it
costs the normal price, takes the next decal from the save's queue and is
saved. The stew animation plays for the first pull and the closing animation
once at the end. No pull shows its own card: the run's decals go into the grid
online's box stews used for their lineup (five across, with a cursor and the
decal's description), which opens once the run is over.

The mod lets the player choose N, so one patch is written for each choice, as
tfc/x<N>/Game/BrgGame/CookedPCConsole/BrgGame.upk.PackagePatch; mod.json picks
the folder with "source": "tfc/x{{count}}".

Three functions change, all by trampolines: an existing statement is replaced
by a jump to new code on the end of the function, and that code jumps back. No
existing jump target moves.

  SetGachaTopMenuType  the normal stew gets three entries: Purchase,
                       Purchase xN, Check Lineup
  Tick  state 23 (the menu)       the new entry; Check Lineup becomes entry 2
        state 14 (stew animation) skipped on repeat pulls
        state 15 (the card)       during a run, the decal joins the grid
                                  and the card closes at once
        state 16 (closing animation, then the top menu)
                                  skipped between pulls, which go straight on;
                                  after the last, the grid (state 19) opens
        state 19 (box lineup)     after a run, back to the stew menu
  BrgUIMenu_SkillExchange_BoxGachaLineup.DrawList
                                  the rows move with the cursor, as a run
                                  can fill more than the two rows shown

The pull count lives in mReturnState, a byte the class declares and no script
uses, and the run's decals in the box lineup's own list, which no normal stew
uses, so the package's tables do not change.

Reads the game's stock BrgGame.upk and never changes it:

    py -3 tools/build_stew_multi_pull.py --manager PATH/TO/LiD-DB-Editor --upk PATH/TO/BrgGame.upk

It reads the package with the package reader of a LiD DB Editor checkout
(--manager).

The stock BrgGame.upk is the one Steam installs, or the copy the manager keeps
in backups/game_files once a mod has changed it.
"""
from __future__ import annotations

import argparse
import hashlib
import struct
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parent
OUT = REPO / "mods" / "Stew Multi Pull" / "tfc"
PATCH_PATH = Path("Game") / "BrgGame" / "CookedPCConsole" / "BrgGame.upk.PackagePatch"
COUNTS = (5, 10, 15, 20, 25)    # the mod's choice of pulls; one patch each, in tfc/x<N>
HEADER = 48                     # a function's data before its script
NO_ANIMATION = 11               # StartMatinee has no case for it: nothing plays, and it counts as finished
STATE_PULL = 13                 # GetStateAfterGacha's answer when the next pull can go ahead
REFERENCE_VERSION = 12          # the reference layout TFC Installer writes

parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
parser.add_argument("--manager", required=True, type=Path, help="a LiD DB Editor checkout")
parser.add_argument("--upk", required=True, type=Path, help="the game's stock BrgGame.upk")
parser.add_argument("--out", type=Path, default=OUT, help="writes x<N>/" + PATCH_PATH.as_posix() + " here")
parser.add_argument("--counts", type=int, nargs="+", default=list(COUNTS), help="pulls per Purchase xN")
parser.add_argument("--listing", type=Path, help="also write the new code, disassembled, into this folder")
args = parser.parse_args()
if not all(2 <= n <= 255 for n in args.counts):
    sys.exit("every count must be 2 to 255")

sys.path.insert(0, str(args.manager.resolve()))
sys.path.insert(0, str(HERE))
from lid_db_manager.upk import apply as A                   # noqa: E402
from lid_db_manager.upk import package as P                 # noqa: E402
from lid_db_manager.upk import packagepatch as PP           # noqa: E402
from ue3dis import Dis                                      # noqa: E402

raw = args.upk.read_bytes()
print(f"{args.upk.name}: SHA-1 {hashlib.sha1(raw).hexdigest()}")
summary, flat = P.decompressed(raw)
pkg = P.Package(summary, flat, P.read_names(flat, summary), P.read_imports(flat, summary),
                P.read_exports(flat, summary), source=args.upk)
names = lambda i: pkg.names[i].text   # noqa: E731
objs = lambda i: pkg.full_path(i).replace("BrgUIMenu_SkillExchange.", "")   # noqa: E731


def export(path: str) -> int:
    hits = [i + 1 for i in range(len(pkg.exports)) if pkg.full_path(i + 1) == path]
    if len(hits) != 1:
        sys.exit(f"{path}: {len(hits)} matches - is this BrgGame.upk?")
    return hits[0]


CLS = "BrgUIMenu_SkillExchange"
RS = export(f"{CLS}.mReturnState")
TEMPSTATE = export(f"{CLS}.Tick.tempState")
DEBUGMATINEE = export(f"{CLS}.Tick.debugMatinee")
GACHA_SKILL_ID = export(f"{CLS}.Tick.gachaSkillId")
GET_STATE_AFTER = export(f"{CLS}.GetStateAfterGacha")
START_MATINEE = export(f"{CLS}.StartMatinee")
LINEUP = export(f"{CLS}.mBOXGachaLineup")
GRID = f"{CLS}_BoxGachaLineup"
GRID_LIST = export(f"{GRID}.mBoxGachaSkillList")
GRID_LINES = export(f"{GRID}.mNumLines")
GRID_W = export(f"{GRID}.mListW")
GRID_H = export(f"{GRID}.mListH")
GRID_CURSOR = export(f"{GRID}.mCursorPos")
DETAIL = export(f"{GRID}.{CLS}_BoxGachaSkillDetail")
DETAIL_ID = export(f"{GRID}.{CLS}_BoxGachaSkillDetail.SkillID")
DETAIL_ENABLED = export(f"{GRID}.{CLS}_BoxGachaSkillDetail.IsEnable")
DRAW_INDEX = export(f"{GRID}.DrawList.Index")
DRAW_Y = export(f"{GRID}.DrawList.Y")
DRAW_HUD = export(f"{GRID}.DrawList.inHUD")
DRAW_CURSOR = export(f"{GRID}.DrawListCursor")
GRID_GUIDE = export(f"{GRID}.mListLUGuide")
GRID_STEP_X = export(f"{GRID}.mListIndentX")
GRID_STEP_Y = export(f"{GRID}.mListIndentY")
MENU_STATE = export("BrgUIMenu_Base.mState")
NET_GACHA = export(f"{CLS}.mNetGacha")
REPLY_ARG = export(f"{CLS}.GetGachaSkillId.NetGacha")
RESULT_INDEX = export(f"{CLS}.Tick.gachaIndex")
MENU_CURSOR = export(f"{CLS}.mGachaMenu")
STRING_FONT = export(f"{CLS}.mGachaMenu_StringFont")
STATE_GRID = 19                 # the box stew's lineup: a 5-wide grid of decals
GRID_ROWS = 3                   # rows shown; online used 2, the layout has guides for 3
GRID_RAISE = 12                 # the grid sits this much higher than online's
LEFT_HINT = "LID_MULTI_LEFT="   # read by lid-stewpull.dll: pulls left in a run
ALL_HINT = "LID_MULTI_ALL="     # and: do the whole run in this one request

o = lambda ref: struct.pack("<i", ref)      # noqa: E731
u16 = lambda v: struct.pack("<H", v)         # noqa: E731


def mem_len(chunk: bytes) -> int:
    d = Dis(chunk, names, objs)
    while d.pos < len(chunk):
        d.expr()
    return d.mem


# ------------------------------------------------------------ expression bits
def inst(ref): return b"\x01" + o(ref)
def local(ref): return b"\x00" + o(ref)
def intb(v): return b"\x2c" + bytes([v])
def byte(v): return b"\x24" + bytes([v])
def to_int(e): return b"\x38\x3a" + e                   # byte -> int
def to_byte(e): return b"\x38\x3d" + e                  # int -> byte
def to_string(e): return b"\x38\x53" + e                # int -> string
def native(n, *a): return bytes([n]) + b"".join(a) + b"\x16"
def let(a, b): return b"\x0f" + a + b
def fcall(ref, *a): return b"\x1c" + o(ref) + b"".join(a) + b"\x16"
def concat(a, b): return native(0x70, a, b)
def gt(a, b): return native(0x97, a, b)
def lt(a, b): return native(0x96, a, b)
def eq(a, b): return native(0x9A, a, b)
def ne(a, b): return native(0x9B, a, b)
def minus(a, b): return native(0x93, a, b)
def plus(a, b): return native(0x92, a, b)
def times(a, b): return native(0x90, a, b)
def divide(a, b): return native(0x91, a, b)
def ge(a, b): return native(0x99, a, b)
def not_(a): return native(0x81, a)
def member(field, struct_ref, of): return b"\x35" + o(field) + o(struct_ref) + b"\x00\x01" + of
def element(index, of): return b"\x10" + index + of
def le(a, b): return native(0x98, a, b)
def ftimes(a, b): return native(0xAB, a, b)             # float * float
def to_float(e): return b"\x38\x3f" + e                 # int -> float
def fconst(v): return b"\x1e" + struct.pack("<f", v)
def cstr(text): return b"\x1f" + text.encode("latin-1") + b"\0"
def inc(e): return native(0xA5, e)                      # e++
def cond(c, a, b): return b"\x45" + c + u16(mem_len(a)) + a + u16(mem_len(b)) + b
ZERO, ONE, TRUE = b"\x25", b"\x26", b"\x27"
rs = inst(RS)
count = to_int(rs)


def context(obj, prop, inner):
    """obj.<inner>, where inner names `prop` (the form the game's compiler writes)."""
    return b"\x19" + obj + u16(mem_len(inner)) + o(prop) + b"\x00" + inner


def grid(prop):
    """mBOXGachaLineup.<prop>, from the stew menu."""
    return context(inst(LINEUP), prop, inst(prop))


class Asm:
    """Statements with forward labels; jump targets are memory offsets."""

    def __init__(self):
        self.parts = []

    def code(self, b): self.parts.append(("code", b)); return self
    def label(self, name): self.parts.append(("label", name)); return self
    def jump(self, to): self.parts.append(("jump", to, None)); return self
    def jumpifnot(self, to, cond): self.parts.append(("jumpifnot", to, cond)); return self

    def assemble(self, at: int) -> bytes:
        labels, pos = {}, at
        for p in self.parts:
            if p[0] == "label":
                labels[p[1]] = pos
            elif p[0] == "code":
                pos += mem_len(p[1])
            else:
                pos += 3 + (mem_len(p[2]) if p[2] else 0)
        out = b""
        for p in self.parts:
            if p[0] == "code":
                out += p[1]
            elif p[0] in ("jump", "jumpifnot"):
                to = labels[p[1]] if isinstance(p[1], str) else p[1]
                out += (b"\x06" if p[0] == "jump" else b"\x07") + u16(to) + (p[2] or b"")
        return out


class Fn:
    """One function's script, with code added before its END token."""

    def __init__(self, path, stock_sizes):
        self.ref = export(path)
        self.path = path
        self.data = pkg.export_data(self.ref - 1)
        self.mem, self.disk = struct.unpack_from("<ii", self.data, HEADER - 8)
        self.code = bytearray(self.data[HEADER:HEADER + self.disk])
        self.tail = self.data[HEADER + self.disk:]
        d = Dis(bytes(self.code), names, objs)
        self.lines = d.run()
        if (len(self.lines), self.mem, self.disk) != stock_sizes or self.lines[-1][4] != "END":
            sys.exit(f"{path} is not the one this was written for: {len(self.lines)} statements, "
                     f"{self.mem}/{self.disk} bytes. Is this the stock BrgGame.upk of game 1.89?")
        self.targets = {t for _, _, t in d.targets}
        self.by_mem = {line[1]: line for line in self.lines}
        self.added = b""
        self.added_at = self.mem - 1            # the END token's place
        self.replaced = []

    def stmt(self, at, starts_with):
        line = self.by_mem.get(at)
        if not line or not line[4].startswith(starts_with):
            sys.exit(f"{self.path} @{at:#x}: expected `{starts_with}`, found `{line and line[4]}`")
        return line

    def stmt_bytes(self, at, starts_with):
        d, m, dl, ml, _ = self.stmt(at, starts_with)
        return bytes(self.code[d:d + dl])

    def next_at(self):
        return self.added_at + mem_len(self.added)

    def add(self, asm: Asm) -> int:
        at = self.next_at()
        self.added += asm.assemble(at)
        return at

    def after(self, at, starts_with):
        _, m, _, ml, _ = self.stmt(at, starts_with)
        return m + ml

    def divert(self, at, starts_with, asm: Asm):
        """Replace the statement at `at` with a jump to `asm`, added on the end."""
        d, m, dl, ml, _ = self.stmt(at, starts_with)
        to = self.add(asm)
        refs, extra = divmod(ml - dl, 4)
        pad = dl - 3 - 5 * refs
        if pad < 0 or extra:
            sys.exit(f"{self.path} @{at:#x}: statement too short to replace")
        # an object constant is 4 bytes on disk and 8 in memory, so the filler
        # keeps both sizes of the statement it replaces
        self.code[d:d + dl] = b"\x06" + u16(to) + (b"\x20" + o(0)) * refs + b"\x0b" * pad
        self.replaced.append((m, m + ml))

    def poke(self, at, starts_with, offset, value: bytes):
        d, *_ = self.stmt(at, starts_with)
        self.code[d + offset:d + offset + len(value)] = value

    def finish(self):
        for lo, hi in self.replaced:
            if any(lo < t < hi for t in self.targets):
                sys.exit(f"{self.path}: a jump lands inside the replaced statement at {lo:#x}")
        code = bytes(self.code[:-1]) + self.added + b"\x53"
        d = Dis(code, names, objs)
        lines = d.run()
        if lines[-1][4] != "END" or d.pos != len(code):
            sys.exit(f"{self.path}: the new script does not read back cleanly")
        starts = {line[1] for line in lines}
        for _, kind, t in d.targets:
            if t not in starts:
                sys.exit(f"{self.path}: a {kind} to {t:#x} does not land on a statement")
        if d.mem > 0xFFFF:
            sys.exit(f"{self.path}: too long for 16-bit jumps")
        self.new_lines = lines
        self.used_names = {i for _, i in d.names}
        self.used_objects = {r for _, r in d.objs if r}
        self.new_data = self.data[:HEADER - 8] + struct.pack("<ii", d.mem, len(code)) + code + self.tail
        print(f"{self.path}: {self.mem}/{self.disk} -> {d.mem}/{len(code)} script bytes (memory/disk)")
        return self.new_data


def split_let(stmt: bytes):
    """`a = b` -> (a, b)."""
    d = Dis(stmt, names, objs)
    assert d.u8() == 0x0F
    d.expr()
    return stmt[1:d.pos], stmt[d.pos:]


# ------------------------------------------------------- SetGachaTopMenuType
def build(N: int) -> bytes:
    top = Fn(f"{CLS}.SetGachaTopMenuType", (34, 1697, 1125))
    three = top.stmt_bytes(0x2D0, "I:mGachaMenuSelectItemNum = 3")
    grow = top.stmt_bytes(0x2DC, "I:mGachaUIInfoArray.add(I:mGachaMenuSelectItemNum)")
    text0 = top.stmt_bytes(0x2F0, "(I:mGachaUIInfoArray[0]).SkillExchange_GachaUIInfo.mText")
    slot1, lineup = split_let(top.stmt_bytes(0xDB, "(I:mGachaUIInfoArray[1]).SkillExchange_GachaUIInfo.mText"))
    slot2, _ = split_let(top.stmt_bytes(0x3B1, "(I:mGachaUIInfoArray[2]).SkillExchange_GachaUIInfo.mText"))
    _, buy = split_let(text0)


    def localize(key: str, *params: bytes) -> bytes:
        """BrgUIManager.GetLocalizeTextST(key, params...), built like the BUY call it copies."""
        assert buy[0] == 0x12 and buy[1] == 0x20 and buy[13] == 0x1B
        left, retval, size, fn_name = buy[1:6], buy[8:12], buy[12:13], buy[13:22]
        call = (fn_name + b"\x1f" + key.encode("latin-1") + b"\0"
                + b"".join(params) + b"\x4a" * (6 - len(params)) + b"\x16")
        return b"\x12" + left + u16(mem_len(call)) + retval + size + call


    if localize("MSHR_KAIKAN.TXT_GACHA_BUY") != buy:
        sys.exit("could not rebuild the game's own GetLocalizeTextST call")
    # "Purchase x10": the game's own Buy text, then its own unused "x#0" text
    # (MSHR_KAIKAN.TXT_GACHA_TIMES, "#0倍" in Japanese), so every language reads right
    multi = concat(concat(buy, b"\x1f \x00"), localize("MSHR_KAIKAN.TXT_GACHA_TIMES", to_string(intb(N))))
    # the box stew's three-entry layout: bases and cursor guides v00, v01, v02
    layout = b"".join(top.stmt_bytes(at, "(I:mGachaUIInfoArray[") for at in (0x413, 0x476, 0x4D9, 0x53C, 0x59F, 0x603))
    top.stmt(0x66D, "jumpifnot 0x69e")                        # after the switch: clamp the cursor
    top.divert(0x5C, "I:mGachaMenuSelectItemNum = 2", Asm()
               .code(three + grow + text0)
               .code(let(slot1, multi))                                 # Purchase xN
               .code(let(slot2, lineup))
               .code(layout)
               .jump(0x66D))

    # ----------------------------------------------------------------------- Tick
    tick = Fn(f"{CLS}.Tick", (659, 15362, 11146))
    SOUND_OK = "I:BrgUIBase.mUIManager.{skip=0x14,None,0}AddPlaySoundCue(obj:SN_SE_SYS_MomokoShop.SN_SE_SYS_MomokoShop_OK"
    sound_ok = tick.stmt_bytes(0x1BD2, SOUND_OK)
    pull = tick.stmt_bytes(0x1BFB, "StartWait(0.0f, pcast0x3a(F:GetStateAfterGacha()))")
    hide_menu = tick.stmt_bytes(0x1C16, "F:SetVisibleGachaMenu(false)")
    is_box = tick.stmt_bytes(0x2928, "jumpifnot 0x2992 (bool((I:mCurrentGachaProperty).SkillExchange_GachaProperty.mIsBOXGacha")[3:]
    wait_temp = tick.stmt_bytes(0x2492, "StartWait(0.0f, pcast0x3a(L:Tick.tempState))")
    close_anim = tick.stmt_bytes(0x290F, "F:StartMatinee(8b)")
    to_top = tick.stmt_bytes(0x2992, "StartWait(0.0f, 3)")
    to_close = tick.stmt_bytes(0x28A8, "StartWait(0.0f, 16)")
    clear_grid = tick.stmt_bytes(0x35D0, "I:mBOXGachaLineup.{skip=0xa,None,0}ClearLineup()")
    guide_on = tick.stmt_bytes(0x27E8, "I:BrgUIBase.mUIManager.{skip=0x9,BrgUIManager.mMenuButtonGuide,0}"
                               "I:BrgUIManager.mMenuButtonGuide.{skip=0xb,None,0}SetVisible(true)")
    guide_off = tick.stmt_bytes(0x28DA, "I:BrgUIBase.mUIManager.{skip=0x9,BrgUIManager.mMenuButtonGuide,0}"
                                "I:BrgUIManager.mMenuButtonGuide.{skip=0xb,None,0}SetVisible(false)")
    assert to_top.endswith(b"\x2c\x03\x16")
    to_grid = to_top[:-2] + bytes([STATE_GRID]) + b"\x16"
    MENU_DONE = 0x1C77
    tick.stmt(MENU_DONE, "jump 0x1df9")
    # the card's own clean-up when the player closes it, copied as it is
    card = Fn(f"{CLS}.UpdateGachaResult", (13, 258, 190))
    hide_card = b"".join(card.stmt_bytes(at, starts) for at, starts in (
        (0x74, "I:mGachaStickerExplane.{skip=0xb,None,0}SetVisibility(false)"),
        (0x94, "I:mGachaStickerExplane.{skip=0xa,None,0}Clear()"),
        (0xB3, "I:mGachaResultSkillId = ''"),
        (0xBF, "I:mGachaStickerImage = none")))

    # state 23, case 0 (Buy): clear the count. It goes after the sound, so the
    # new case's `mReturnState = N` is the only one the sound follows.
    tick.divert(0x1BD2, SOUND_OK, Asm().code(sound_ok + let(rs, byte(0))).jump(0x1BFB))
    # state 23: Lineup becomes entry 2, and a new case 1 joins the chain before it
    tick.poke(0x1C24, "case 1b: (next 0x1c71)", 3, byte(2))
    new_case = tick.add(Asm()
                        .code(b"\x0a" + u16(0x1C24) + byte(1))          # case 1:
                        .code(let(rs, byte(N)) + clear_grid + sound_ok + pull + hide_menu)
                        .jump(MENU_DONE))
    tick.poke(0x1BCD, "case 0b: (next 0x1c24)", 1, u16(new_case))

    # state 14: keep `debugMatinee = 10`, then on a repeat pull of a multi-pull
    # (0 < count < N, not a box stew) override the animation with one that does
    # not exist, so nothing plays. (Keeping the super-rare animation for 4- and
    # 5-star decals was tried in 0.2.0: in game it zoomed the camera out
    # mid-run, the previous pull's closing animation never having played.)
    back = tick.after(0x2763, "L:Tick.debugMatinee = 10b")
    tick.divert(0x2763, "L:Tick.debugMatinee = 10b", Asm()
                .code(tick.stmt_bytes(0x2763, "L:Tick.debugMatinee = 10b"))
                .jumpifnot("done", gt(count, ZERO))
                .jumpifnot("done", lt(count, intb(N)))
                .jumpifnot("done", not_(is_box))
                .code(let(local(DEBUGMATINEE), byte(NO_ANIMATION)))
                .label("done")
                .jump(back))

    # The price of a run is the price of a pull times the pulls: the menu's own
    # check (GetStateAfterGacha) asks for all of it before a run starts, so a run
    # no longer stops halfway for want of Kill Coins or Death Metals. Every
    # price check below runs with mReturnState = the pulls still to come.
    run_times = cond(gt(count, ZERO), count, ONE)

    # state 16, first tick: no closing animation while more pulls will follow
    back = tick.after(0x290F, "F:StartMatinee(8b)")
    tick.divert(0x290F, "F:StartMatinee(8b)", Asm()
                .jumpifnot("close", gt(count, ONE))
                .jumpifnot("close", not_(is_box))
                .code(let(rs, to_byte(minus(count, ONE))))
                .code(let(local(TEMPSTATE), fcall(GET_STATE_AFTER)))
                .code(let(rs, to_byte(plus(count, ONE))))
                .jumpifnot("close", eq(to_int(local(TEMPSTATE)), intb(STATE_PULL)))
                .code(fcall(START_MATINEE, byte(NO_ANIMATION)))
                .jump(back)
                .label("close")
                .code(close_anim)
                .jump(back))

    # state 15 (the card), first tick: during a run every decal in the reply
    # joins the box stew's lineup grid instead, and the card closes at once (the
    # same clean-up as the player closing it, without the sound). The reply
    # holds one decal a pull, or - when lid-stewpull.dll did the whole run in
    # one request - all of them, and then the run is over.
    reply = Fn(f"{CLS}.GetGachaSkillId", (6, 150, 94))
    first = reply.stmt_bytes(0x44, "L:GetGachaSkillId.gachaSkillId = (L:GetGachaSkillId.NetGacha.")
    assert first[6:8] == b"\x10\x25"
    decals = first[8:]
    if decals.count(local(REPLY_ARG)) != 1:
        sys.exit("GetGachaSkillId: the reply's decal list is not read as expected")
    decals = decals.replace(local(REPLY_ARG), inst(NET_GACHA))  # mNetGacha.mResult.mSklids
    decal_count = b"\x36" + decals
    grid_list = grid(GRID_LIST)
    grid_len = b"\x36" + grid_list
    # mListLUGuide.mPosY: the grid's top-left guide, which every row, the cursor
    # and the page number are drawn from (an int of the game's layout image)
    sticker_y = Fn(f"{GRID}.DrawListItem", (31, 1708, 1084)).stmt_bytes(0x255, f"L:{GRID}.DrawListItem.stickerMinY = ")
    if sticker_y[11:19] != b"\x19" + inst(GRID_GUIDE) + u16(9) or sticker_y[23] != 0:
        sys.exit("DrawListItem: the grid guide's position is not read as expected")
    pos_y = struct.unpack_from("<i", sticker_y, 19)[0]
    guide_y = context(grid(GRID_GUIDE), pos_y, inst(pos_y))
    last = element(minus(grid_len, ONE), grid_list)
    i = local(RESULT_INDEX)
    show = tick.stmt_bytes(0x2846, "F:InitializeGachaResult(")
    back = tick.after(0x2846, "F:InitializeGachaResult(")
    tick.divert(0x2846, "F:InitializeGachaResult(", Asm()
                .jumpifnot("card", gt(count, ZERO))
                .jumpifnot("card", not_(is_box))
                .code(let(i, ZERO))
                .label("each")
                .jumpifnot("all", lt(i, decal_count))
                .code(let(local(GACHA_SKILL_ID), element(i, decals)))
                .code(show)                     # its cost comes off the money shown
                .code(let(grid_len, plus(grid_len, ONE)))
                .code(let(member(DETAIL_ID, DETAIL, last), local(GACHA_SKILL_ID)))
                .code(b"\x14\x2d" + member(DETAIL_ENABLED, DETAIL, last) + TRUE)
                .code(inc(i))
                .jump("each")
                .label("all")
                .jumpifnot("next", gt(decal_count, ONE))
                .code(let(rs, byte(1)))         # the whole run came at once
                .label("next")
                .code(hide_card + guide_off + to_close)
                .jump(0x28B9)                   # this tick is done
                .label("card")
                .code(show)
                .jump(back))

    # state 16, normal stew: the next pull instead of the top menu. A run that
    # is over - the last pull, or GetStateAfterGacha says no (decals full, ...)
    # - shows its decals in the grid; a count above 1 tells the grid the run
    # stopped early, so GetStateAfterGacha's message shows after it.
    back = tick.after(0x2992, "StartWait(0.0f, 3)")
    tick.divert(0x2992, "StartWait(0.0f, 3)", Asm()
                .jumpifnot("over", gt(count, ONE))
                .code(let(rs, to_byte(minus(count, ONE))))
                .code(let(local(TEMPSTATE), fcall(GET_STATE_AFTER)))
                .jumpifnot("short", eq(to_int(local(TEMPSTATE)), intb(STATE_PULL)))
                .code(wait_temp)
                .jump(back)
                .label("short")
                .code(let(rs, to_byte(plus(count, ONE))))
                .label("over")
                .jumpifnot("menu", gt(count, ZERO))
                .code(let(grid(GRID_H), intb(GRID_ROWS)))
                .code(let(guide_y, minus(guide_y, intb(GRID_RAISE))))
                .code(let(grid(GRID_LINES), divide(minus(plus(grid_len, grid(GRID_W)), ONE), grid(GRID_W))))
                .code(guide_on + to_grid)
                .jump(back)
                .label("menu")
                .code(let(rs, byte(0)) + to_top)
                .jump(back))

    # state 19 (the lineup grid), closed: a box stew goes back to its menu as
    # before; after a run, the grid is emptied and set back to two rows, and the
    # stew menu shows, or GetStateAfterGacha's message when the run stopped early
    back = tick.after(0x2F75, "StartWait(0.0f, 18)")
    tick.divert(0x2F75, "StartWait(0.0f, 18)", Asm()
                .jumpifnot("run", is_box)
                .code(tick.stmt_bytes(0x2F75, "StartWait(0.0f, 18)"))
                .jump(back)
                .label("run")
                .code(clear_grid)
                .code(let(grid(GRID_H), intb(2)))
                .code(let(guide_y, plus(guide_y, intb(GRID_RAISE))))
                .jumpifnot("menu", gt(count, ONE))
                .code(let(rs, byte(1)))         # why one more pull cannot go ahead
                .code(let(local(TEMPSTATE), fcall(GET_STATE_AFTER)))
                .code(let(rs, byte(0)) + wait_temp)
                .jump(back)
                .label("menu")
                .code(let(rs, byte(0)) + to_top)
                .jump(back))

    # state 10 (the confirm box, after the Death Metal shop) and state 17's
    # return from that shop: the run's price, not one pull's
    cost = tick.stmt_bytes(0x2A61, "jumpifnot 0x2a9b (N153(F:GetHaveDeathMetal(), ")[10:26]
    assert cost[0] == 0x35 and cost[9:11] == b"\x00\x00"
    cost_set = cost[:9] + b"\x00\x01" + cost[11:]           # mCurrentGachaProperty.mCost, written
    have_dm = tick.stmt_bytes(0x2A61, "jumpifnot 0x2a9b (N153(F:GetHaveDeathMetal(), ")[4:10]
    tick.divert(0x2A61, "jumpifnot 0x2a9b (N153(F:GetHaveDeathMetal(), ", Asm()
                .jumpifnot(0x2A9B, ge(have_dm, times(cost, run_times)))
                .jump(tick.after(0x2A61, "jumpifnot 0x2a9b")))
    back = tick.after(0x1E4F, "L:Tick.confirmMessage = obj:BrgUI_Utility::")
    tick.divert(0x1E4F, "L:Tick.confirmMessage = obj:BrgUI_Utility::", Asm()
                .code(let(cost_set, times(cost, run_times)))
                .code(tick.stmt_bytes(0x1E4F, "L:Tick.confirmMessage = obj:BrgUI_Utility::"))
                .code(let(cost_set, divide(cost, run_times)))
                .jump(back))

    # GetStateAfterGacha: enough money for every pull still to come
    check = Fn(f"{CLS}.GetStateAfterGacha", (14, 229, 149))
    compare = check.stmt_bytes(0x4D, "jumpifnot 0x86 (N152((I:mCurrentGachaProperty).SkillExchange_GachaProperty.mCost")
    if compare[4:20] != cost:
        sys.exit("GetStateAfterGacha: the price is not read as expected")
    money = compare[20:-1]
    check.divert(0x4D, "jumpifnot 0x86 (N152(", Asm()
                 .jumpifnot(0x86, le(times(cost, run_times), money))
                 .jump(check.after(0x4D, "jumpifnot 0x86")))

    # the menu's price line ("#0 Death Metals ...") reads Purchase xN's price
    # while the cursor is on it
    menu = Fn(f"{CLS}.RenderGachaMenu", (31, 2566, 1694))
    price_line = menu.stmt_bytes(0x2A8, "I:BrgUIBase.mUIManager.{skip=0x9,BrgUIManager.mFontRender,0}")
    if price_line.count(b"\x38\x53" + cost) != 1:
        sys.exit("RenderGachaMenu: the price line is not as expected")
    menu_times = cond(eq(to_int(inst(MENU_CURSOR)), ONE), cond(not_(is_box), intb(N), ONE), ONE)
    back = menu.after(0x2A8, "I:BrgUIBase.mUIManager.")
    menu.divert(0x2A8, "I:BrgUIBase.mUIManager.", Asm()
                .code(let(cost_set, times(cost, menu_times)))
                .code(price_line)
                .code(let(cost_set, divide(cost, menu_times)))
                .jump(back))

    # NetGacha: during a run the request also carries the pulls left, and on the
    # run's first pull a request to do them all at once, as entries in its list
    # of the shop's stews. The server only hands that list to a check that
    # always says yes; lid-stewpull.dll reads the entries. Without it each
    # pull is the game's own single pull, as before.
    net = Fn(f"{CLS}.NetGacha", (10, 252, 160))
    template = net.stmt_bytes(0x49, "(L:NetGacha.gachaParam)")
    ids, item = template[:17], template[19:-1]          # AddItem: array, skip word, item, end
    if template[0] != 0x55 or mem_len(item) + 1 != struct.unpack_from("<H", template, 17)[0]:
        sys.exit("NetGacha: the AddItem statement is not as expected")

    def add_id(text):
        return ids + u16(mem_len(text) + 1) + text + b"\x16"

    send = net.stmt_bytes(0x9D, "L:NetGacha.gachaRes = interfacecontext(")
    net.stmt(0xE7, "return L:NetGacha.gachaRes")
    net.divert(0x9D, "L:NetGacha.gachaRes =", Asm()
               .jumpifnot("send", gt(count, ZERO))
               .code(add_id(concat(cstr(LEFT_HINT), to_string(count))))
               .jumpifnot("send", eq(count, intb(N)))
               .code(add_id(concat(cstr(ALL_HINT), to_string(count))))
               .label("send")
               .code(send)
               .jump(0xE7))

    # ------------------------------------------------------------------ the grid
    # BrgUIMenu_SkillExchange_BoxGachaLineup.DrawList draws mListH rows from the
    # first decal: enough for online's box stews, not for a run of 25. A run
    # shows three rows, a page at a time: the page the cursor is on. The row
    # loop's head is the only statement long enough for a jump; on the first
    # row it sets where drawing starts, and once the rows are done it draws the
    # cursor where it now is on screen.
    draw = Fn(f"{GRID}.DrawList", (17, 378, 234))
    w, h, cursor, index = inst(GRID_W), inst(GRID_H), inst(GRID_CURSOR), local(DRAW_INDEX)
    head = draw.stmt_bytes(0x0B, f"jumpifnot 0x15b (N150(L:{GRID}.DrawList.Y, I:{GRID}.mListH))")
    body = draw.after(0x0B, "jumpifnot 0x15b")
    draw.stmt(0x15B, f"F:{GRID}.DrawListCursor(L:{GRID}.DrawList.inHUD, I:{GRID}.mCursorPos)")
    done = draw.after(0x15B, f"F:{GRID}.DrawListCursor(")
    draw.stmt(done, "return nothing")
    draw.divert(0x0B, "jumpifnot 0x15b", Asm()
                .jumpifnot("rows", eq(local(DRAW_Y), ZERO))
                .code(let(index, times(divide(cursor, times(w, h)), times(w, h))))
                .label("rows")
                .jumpifnot("cursor", head[3:])
                .jump(body)
                .label("cursor")
                .code(fcall(DRAW_CURSOR, local(DRAW_HUD), minus(cursor, minus(index, times(h, w)))))
                .jump(done))

    # Render: with more than one page, "page/pages" under the grid, in the
    # menu's own font (digits read the same in every language)
    render = Fn(f"{CLS}.Render", (23, 954, 650))
    template = price_line
    assert template[19 + 7] == 0x1B                       # ...mFontRender.{skip, ret, 0} DrawText_UseImgPos(
    pages = divide(minus(plus(grid(GRID_LINES), grid(GRID_H)), ONE), grid(GRID_H))
    page = plus(divide(grid(GRID_CURSOR), times(grid(GRID_W), grid(GRID_H))), ONE)
    call = (template[26:35] + inst(STRING_FONT) + grid(GRID_GUIDE)
            + concat(to_string(page), concat(cstr("/"), to_string(pages))) + TRUE
            + ftimes(grid(GRID_STEP_X), fconst(2.0)) + ftimes(grid(GRID_STEP_Y), to_float(grid(GRID_H)))
            + b"\x4a\x4a\x16")
    page_text = template[:19] + u16(mem_len(call)) + template[21:26] + call
    lineup_draw = render.stmt_bytes(0x120, "I:mBOXGachaLineup.{skip=0x13,None,0}RenderProcess(")
    item = Fn(f"{GRID}.DrawListItem", (31, 1708, 1084))   # its own `font colour = white`, copied
    white = b"".join(item.stmt_bytes(at, "I:BrgUIBase.mUIManager.{skip=0x9,BrgUIManager.mFontRender,0}")
                     for at in (0x5BF, 0x5F5, 0x62B, 0x661))
    back = render.after(0x120, "I:mBOXGachaLineup.")
    render.divert(0x120, "I:mBOXGachaLineup.", Asm()
                  .code(lineup_draw)
                  .jumpifnot("done", ne(grid(MENU_STATE), ZERO))
                  .jumpifnot("done", not_(is_box))
                  .jumpifnot("done", gt(pages, ONE))
                  .code(white + page_text)
                  .label("done")
                  .jump(back))

    functions = [top, tick, check, menu, net, render, draw]
    for fn in functions:
        fn.finish()


    # ---------------------------------------------------------------- the patch
    def fstring(text: str) -> bytes:
        encoded = text.encode("latin-1") + b"\0"
        return struct.pack("<i", len(encoded)) + encoded


    def class_of(ref: int) -> tuple[str, str]:
        if ref < 0:
            imp = pkg.imports[-ref - 1]
            return pkg.name(imp.class_name, imp.class_name_number), \
                pkg.name(imp.class_package, imp.class_package_number)
        class_index = pkg.exports[ref - 1].class_index
        return (pkg.object_name(class_index) if class_index else "Class"), ""


    def path_of(ref: int) -> str:
        return pkg.full_path(ref).replace(".", "\\")


    used_objects = {fn.ref for fn in functions}
    used_names = set()
    for fn in functions:
        used_objects |= fn.used_objects
        used_names |= fn.used_names
    object_refs = sorted(used_objects, key=lambda r: (r < 0, abs(r)))
    name_refs = sorted(used_names)

    out = bytearray()
    out += struct.pack("<i", 2)
    for table in (pkg.names, pkg.imports, pkg.exports):
        out += struct.pack("<ii", len(table), 0)
    out += struct.pack("<i", len(functions))
    for fn in functions:
        out += struct.pack("<ii", fn.ref - 1, 0)
        out += struct.pack("<i", len(fn.new_data)) + fn.new_data
    out += struct.pack("<i", REFERENCE_VERSION)
    out += struct.pack("<i", len(name_refs))
    for i in name_refs:
        out += struct.pack("<i", i) + fstring(pkg.names[i].text)
    out += struct.pack("<i", len(object_refs))
    for ref in object_refs:
        class_name, class_package = class_of(ref)
        out += struct.pack("<i", ref) + fstring(path_of(ref))
        if ref < 0:
            out += fstring(path_of(ref))
        out += fstring(class_name)
        if ref < 0:
            out += fstring(class_package)
    patch_bytes = bytes(out)

    # ------------------------------------------------- check it before writing it
    patch = PP.read(patch_bytes)
    # apply() moves the export entries it is given, so it gets its own copy of
    # the table and `pkg` stays the stock package for the next count
    fresh = P.Package(summary, flat, pkg.names, pkg.imports, P.read_exports(flat, summary),
                      source=args.upk)
    if A.problems(fresh, patch):
        sys.exit("the patch does not fit: " + "; ".join(A.problems(fresh, patch)))
    applied, _ = A.apply(fresh, patch)
    expected = bytearray(flat)
    entries = P.read_exports(flat, summary)
    for fn in functions:
        e = entries[fn.ref - 1]
        expected[e.serial_offset:e.serial_offset + e.serial_size] = bytes(e.serial_size)
    for fn in functions:
        at = len(expected)
        expected += fn.new_data
        A._place_entry(expected, entries[fn.ref - 1], at, len(fn.new_data))
    if applied != bytes(expected):
        sys.exit("applying the patch does not give the expected package")
    # and the applied package reads back with every new function in it
    re_exports = P.read_exports(applied, summary)
    for fn in functions:
        e = re_exports[fn.ref - 1]
        if bytes(applied[e.serial_offset:e.serial_offset + e.serial_size]) != fn.new_data:
            sys.exit(f"{fn.path}: not where its export entry says")

    out_file = args.out / f"x{N}" / PATCH_PATH
    out_file.parent.mkdir(parents=True, exist_ok=True)
    out_file.write_bytes(patch_bytes)
    print(f"x{N}: checks {len(name_refs)} name(s), {len(object_refs)} object(s); wrote {out_file} "
          f"({len(patch_bytes):,} bytes), SHA-256 {hashlib.sha256(patch_bytes).hexdigest()}")

    if args.listing:
        args.listing.mkdir(parents=True, exist_ok=True)
        with open(args.listing / f"x{N}.txt", "w", encoding="utf-8") as f:
            for fn in functions:
                f.write(f"# {fn.path}\n")
                changed = {m for m, _ in fn.replaced}
                for d, m, dl, ml, t in fn.new_lines:
                    if m >= fn.added_at or m in changed or m in (0x1BCD, 0x1C24):
                        f.write(f"{m:04x} {t}\n")
    return patch_bytes


for n in args.counts:
    build(n)
